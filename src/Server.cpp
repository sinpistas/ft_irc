/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/27 23:16:08 by apestana          #+#    #+#             */
/*   Updated: 2026/09/13 16:58:39 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "IrcMessage.hpp"
#include "IrcCaseMapping.hpp"
#include "IrcLimits.hpp"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <cerrno>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <csignal>
#include <cctype>
#include <ctime>
#include <set>

static const char *SERVER_NAME = "irc.local";

// How long a connection may stay without completing PASS/NICK/USER. The
// password is what protects this server, so a client that never gets past
// it may not keep a descriptor, a buffer and a poll slot indefinitely.
static const std::time_t REGISTRATION_TIMEOUT = 60;
// How long a client being disconnected is kept alive so that the reply
// explaining why still reaches it. A client that does not read it loses it.
static const std::time_t CLOSING_LINGER = 2;
// poll() has to come back on its own every now and then, or the timeouts
// above would only be noticed when some other client happens to send
// something.
static const int POLL_TIMEOUT_MS = 1000;

// A parameter carrying several values separated by commas, the shape JOIN,
// PART and KICK all accept. Empty pieces are dropped: "#a,,#b" names two
// channels, not three.
static std::vector<std::string> splitOnCommas(const std::string &value)
{
	std::vector<std::string> parts;
	std::string::size_type start = 0;

	while (start <= value.size())
	{
		std::string::size_type end = value.find(',', start);
		if (end == std::string::npos)
			end = value.size();
		if (end > start)
			parts.push_back(value.substr(start, end - start));
		start = end + 1;
	}
	return parts;
}

static bool isValidChannelName(const std::string &name)
{
	if (name.size() < 2 || name.size() > 50 || (name[0] != '#' && name[0] != '&'))
		return false;
	for (std::string::size_type i = 0; i < name.size(); ++i)
	{
		if (name[i] == '\0' || name[i] == '\a' || name[i] == '\r'
			|| name[i] == '\n' || name[i] == ' ' || name[i] == ',' || name[i] == ':')
			return false;
	}
	return true;
}

// A value taken from the client goes into a reply as ONE parameter. A space
// would split it into two, and a leading ':' would turn it into the trailing
// parameter that swallows the rest of the line: either way the client would
// parse a numeric with a different shape than the one meant for it. Anything
// that cannot be sent as itself is replaced by the '*' placeholder.
static std::string safeParameter(const std::string &value)
{
	const std::string token = value.substr(0, value.find(' '));
	if (token.empty() || token[0] == ':')
		return "*";
	return token;
}

static bool isNicknameSpecial(char character)
{
	return character == '[' || character == ']' || character == '\\'
		|| character == '^' || character == '_' || character == '`'
		|| character == '{' || character == '}' || character == '|';
}

static bool areSameNicknames(const std::string &left, const std::string &right)
{
	if (left.size() != right.size())
		return false;
	for (std::string::size_type i = 0; i < left.size(); ++i)
	{
		if (foldIrcCase(left[i]) != foldIrcCase(right[i]))
			return false;
	}
	return true;
}

Server::Server(int port, const std::string &password)
	: _port(port), _password(password), _serverFd(-1)
{
}

Server::~Server()
{
	// Release the listening socket when the server is destroyed.
	if (_serverFd >= 0)
		close(_serverFd);
}

void Server::ignoreSigpipe()
{
	// A write to a peer that already closed the connection raises SIGPIPE,
	// which kills the process by default. sigaction() (rather than the
	// MSG_NOSIGNAL send() flag, which is Linux-only) is the portable way
	// to make that a plain -1/EPIPE return from send() instead.
	struct sigaction sa;
	std::memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	if (sigaction(SIGPIPE, &sa, NULL) < 0)
		throw std::runtime_error(std::string("sigaction: ") + std::strerror(errno));
}

/*setsockopt config use:
	-sockfd: The socket descriptor you wish to modify.
	-level: SOL_SOCKET for general socket options.
	-optname: SO_REUSEADDR allows binding to a port.
	-optval: A pointer to the value you wish to assign to the option.
	-optlen: The size in bytes of the value pointed to by optval.
*/
void Server::initSocket()
{
	// 1-Create an IPv4 TCP socket for incoming client connections.
	_serverFd = socket(AF_INET, SOCK_STREAM, 0);
	if (_serverFd < 0)
		throw std::runtime_error(std::string("socket: ") + std::strerror(errno));

	// Allow the port to be reused immediately after a previous server shutdown.
	int opt = 1;
	if (setsockopt(_serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
		throw std::runtime_error(std::string("setsockopt: ") + std::strerror(errno));

	// poll() needs every monitored socket to be non-blocking.
	setNonBlocking(_serverFd);

	// 2-Configure the server address to listen on every local network interface.
	// htons and htonl convert numbers from processor’s memory format (Host 
	// Byte Order) to the standard network format (Network Byte Order).
	struct sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(static_cast<unsigned short>(_port));
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// 3-The IP address is assigned to the socket (FD)
	if (bind(_serverFd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0)
		throw std::runtime_error(std::string("bind: ") + std::strerror(errno));

	// 4-Start listening and allow the system to queue pending connections.
	if (listen(_serverFd, SOMAXCONN) < 0)
		throw std::runtime_error(std::string("listen: ") + std::strerror(errno));

	std::cout << "Server listening on port " << _port << std::endl;
}

void Server::setNonBlocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0); //F_GETFL returns the file access mode
	if (flags < 0)
		throw std::runtime_error(std::string("fcntl(F_GETFL): ") + std::strerror(errno));

	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
		throw std::runtime_error(std::string("fcntl(F_SETFL): ") + std::strerror(errno));
}

void Server::acceptNewClients()
{
	while (true)
	{
		// The peer address is needed for the host part of this client's
		// prefix, and accept() is the only chance to collect it.
		struct sockaddr_in address;
		socklen_t addressLen = sizeof(address);
		std::memset(&address, 0, sizeof(address));

		int clientFd = accept(_serverFd,
			reinterpret_cast<struct sockaddr *>(&address), &addressLen);
		if (clientFd < 0)
		{
			// No more pending connections; this is the normal way out of the loop.
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			// A signal interrupted accept(); try again.
			if (errno == EINTR)
				continue;
			// Any other error must not take the whole server down.
			std::cerr << "accept: " << std::strerror(errno) << std::endl;
			break;
		}

		try
		{
			setNonBlocking(clientFd);
		}
		catch (const std::exception &e)
		{
			std::cerr << e.what() << std::endl;
			close(clientFd);
			continue;
		}

		// Keep the numeric address as the host name. Turning it into a real
		// name would take a reverse DNS lookup, which blocks for as long as
		// the resolver takes and would freeze every other client with it.
		char numericHost[INET_ADDRSTRLEN];
		std::string hostname = "unknown";
		if (inet_ntop(AF_INET, &address.sin_addr, numericHost, sizeof(numericHost)) != NULL)
			hostname = numericHost;

		struct pollfd clientPoll;
		clientPoll.fd = clientFd;
		clientPoll.events = POLLIN;
		clientPoll.revents = 0;
		_pollFds.push_back(clientPoll);

		_clients.insert(std::pair<int, Client>(clientFd, Client(clientFd, hostname)));

		std::cout << "Accepted new client, fd " << clientFd << std::endl;
	}
}

bool Server::receiveFromClient(int fd)
{
	char buffer[4096];
	// One read per POLLIN event; poll() will report any remaining input.
	ssize_t bytes = recv(fd, buffer, sizeof(buffer), 0);

	if (bytes > 0)
	{
		std::map<int, Client>::iterator it = _clients.find(fd);
		if (it != _clients.end())
			it->second.appendToBuffer(buffer, static_cast<size_t>(bytes));

		std::cout << "Received " << bytes << " bytes from client fd " << fd
			<< ": " << std::string(buffer, static_cast<size_t>(bytes)) << std::endl;
		return true;
	}

	// The subject forbids using errno after recv(): EOF or an error removes
	// this client without retrying the operation or stopping the server.
	if (bytes == 0)
		std::cout << "Client fd " << fd << " closed the connection" << std::endl;
	else
		std::cerr << "recv failed for client fd " << fd << std::endl;
	return false;
}

void handleChannelMode(Client &client, const IrcMessage &msg)
{
	client.getModes();
	std::cout << msg.command << std::endl;
	std::cout << "Changing channel mode." << std::endl;
}

void handleUserMode(Client &client, const IrcMessage &msg)
{
	client.getModes();
	std::cout << msg.command << std::endl;
	std::cout << "Changing user mode." << std::endl;
}

void Server::handleMode(Client &client, const IrcMessage &msg)
{
    if (msg.params.empty())
        return;

    const std::string &target = msg.params[0];

    if (target[0] == '#')
        handleChannelMode(client, msg);
    else if (target == client.getNickname())
        handleUserMode(client, msg);
}

void Server::handleTopic(Client &client, const IrcMessage &msg)
{
    if (msg.params.empty())
        return;

    std::map<std::string, Channel>::iterator it =
        _channels.find(normalizeIrcName(msg.params[0]));

    if (it == _channels.end())
        return;

    Channel &channel = it->second;

    if (!channel.hasMember(client.getFd()))
        return;

    if (msg.params.size() == 1)
    {
        // Send 331 if empty, otherwise 332 with the topic.
		if(msg.params[0] == "")
			std::cout << "Empty parameter. Invalid topic name." << std::endl;
		if(msg.params[0][0] == '#')
		{
			if (channel.getTopic().empty())
				queueMessage(client.getFd(), std::string(":") + SERVER_NAME
						+ " 331 " + client.getNickname() + " " + channel.getName() + " :No topic is set");
			else
			{
				const std::string topicPrefix = std::string(":") + SERVER_NAME + " 332 " + client.getNickname() + " " + channel.getName() + " :";
				const size_t topicSpace = topicPrefix.size() < 510 ? 510 - topicPrefix.size() : 0;
				queueMessage(client.getFd(), topicPrefix
					+ channel.getTopic().substr(0, topicSpace));
			}
		}
        return;
    }

    if (channel.hasMode('t') && !channel.isOperator(client.getFd()))
    {
        // Send 482: channel operator privileges needed.
        return;
    }

    channel.setTopic(msg.params[1]);
    // Broadcast TOPIC to every channel member.
}

void Server::processMessage(Client &client, const IrcMessage &msg)
{
	// Registration commands are the only commands accepted before welcome (001).
	if (!client.isRegistered() && msg.command != "PASS" && msg.command != "NICK"
		&& msg.command != "USER")
	{
		std::string target;

		if (client.getNickname().empty())
			target = "*";
		else 
			target = client.getNickname();
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 451 " + target + " :You have not registered");
		return;
	}
		if (msg.command == "PASS")
			handlePass(client, msg);
		else if (msg.command == "NICK")
			handleNick(client, msg);
		else if (msg.command == "USER")
			handleUser(client, msg);
		else if (msg.command == "JOIN")
			handleJoin(client, msg);
		else if (msg.command == "PART")
			handlePart(client, msg);
		else if (msg.command == "PRIVMSG")
			handlePrivmsg(client, msg);
		else if (msg.command == "QUIT")
			handleQuit(client, msg);
		else if (msg.command == "KICK")
			handleKick(client, msg);
		else if (msg.command == "MODE")
			handleMode(client, msg);
		else if (msg.command == "TOPIC")
			handleTopic(client, msg);
		else
		{
			// Saying nothing is the worst possible answer: the client cannot
			// tell "the server does not know this command" from "it ran and
			// did nothing". The parser has already checked that a command is
			// letters or three digits, so it is safe to echo back as it is.
			queueMessage(client.getFd(), std::string(":") + SERVER_NAME
				+ " 421 " + client.getNickname() + " " + msg.command
				+ " :Unknown command");
		}
}

bool Server::isValidNickname(const std::string &nickname) const
{
	// RFC 2812 caps a nickname at nine characters. Without a cap, a long
	// enough nickname alone would push the messages announcing it past the
	// size an IRC message is allowed to have.
	if (nickname.empty() || nickname.size() > IRC_NICKNAME_MAX_LENGTH)
		return false;

	for (std::string::size_type i = 0; i < nickname.size(); ++i)
	{
		unsigned char character = static_cast<unsigned char>(nickname[i]);
		if (std::isalpha(character) || isNicknameSpecial(nickname[i]))
			continue;
		if (i > 0 && (std::isdigit(character) || nickname[i] == '-'))
			continue;
		return false;
	}
	return true;
}

bool Server::isNicknameInUse(const std::string &nickname, int ignoredFd) const
{
	for (std::map<int, Client>::const_iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (it->first != ignoredFd && areSameNicknames(it->second.getNickname(), nickname))
			return true;
	}
	return false;
}

void Server::extractCompleteLines(int fd)
{
	std::map<int, Client>::iterator it = _clients.find(fd);
	if (it == _clients.end())
		return;

	// A handler may mark this client for removal. Nothing is erased before
	// the end of the poll() pass, so `it` stays valid, but the rest of the
	// lines it had buffered must not be executed on a connection that is
	// already closing.
	std::string line;
	while (!isMarkedForRemoval(fd) && it->second.extractLine(line))
	{
		// Temporary: parse and print here until command dispatch exists.
		// A syntax error only discards this one line, the client stays connected.
		IrcMessage msg;
		if (!IrcMessage::parse(line, msg))
		{
			std::cerr << "Failed to parse line from client fd " << fd << ": \"" << line << "\"" << std::endl;
			continue;
		}
		else
		{
			processMessage(it->second, msg);
		}

		std::cout << "Parsed message from fd " << fd
			<< " -> prefix: \"" << msg.prefix
			<< "\", command: \"" << msg.command
			<< "\", params: [";
		for (size_t i = 0; i < msg.params.size(); ++i)
		{
			if (i > 0)
				std::cout << ", ";
			std::cout << "\"" << msg.params[i] << "\"";
		}
		std::cout << "]" << std::endl;
	}
}

void Server::updateClientPollEvents(int fd)
{
	std::map<int, Client>::iterator clientIt = _clients.find(fd);
	if (clientIt == _clients.end())
		return;

	for (std::vector<struct pollfd>::iterator it = _pollFds.begin(); it != _pollFds.end(); ++it)
	{
		if (it->fd == fd)
		{
			// Watch for readability, unless the client is on its way out and
			// its input is not going to be read any more; and only ask for
			// POLLOUT while there is something queued, otherwise poll() would
			// keep reporting it writable forever and spin the loop for no
			// reason.
			it->events = isMarkedForRemoval(fd) ? 0 : POLLIN;
			if (clientIt->second.hasPendingOutput())
				it->events |= POLLOUT;
			break;
		}
	}
}

void Server::queueMessage(int fd, const std::string &message)
{
	std::map<int, Client>::iterator it = _clients.find(fd);
	if (it == _clients.end())
		return;

	// Strip the terminator a caller may already have added, so the message
	// is neither measured nor terminated twice.
	std::string line = message;
	if (line.size() >= 2 && line[line.size() - 2] == '\r'
		&& line[line.size() - 1] == '\n')
		line.erase(line.size() - 2);

	// Every reply leaves through here, so this is the one place where the
	// server can promise it never puts a line on the wire that is longer
	// than the 512 bytes it demands from its own clients.
	if (line.size() > IRC_MESSAGE_MAX_CONTENT)
		line.erase(IRC_MESSAGE_MAX_CONTENT);

	it->second.appendToSendBuffer(line + "\r\n");
	updateClientPollEvents(fd);
}

bool Server::sendToClient(int fd)
{
	std::map<int, Client>::iterator it = _clients.find(fd);
	if (it == _clients.end())
		return true;

	if (it->second.hasPendingOutput())
	{
		// One write per POLLOUT event, even if only part of the buffer fits.
		const std::string &buffer = it->second.getSendBuffer();
		ssize_t sent = send(fd, buffer.data(), buffer.size(), 0);

		// Decide from the return value only, as required by the subject.
		if (sent <= 0)
		{
			std::cerr << "send failed for client fd " << fd << std::endl;
			return false;
		}
		it->second.consumeSendBuffer(static_cast<size_t>(sent));
	}

	updateClientPollEvents(fd);
	return true;
}

void Server::markForRemoval(int fd)
{
	// Only ever records the descriptor: erasing here would invalidate the
	// iterators and indices that the read loop and the poll loop are using.
	std::map<int, Client>::iterator it = _clients.find(fd);
	if (it == _clients.end())
		return;

	if (_pendingRemoval.insert(fd).second)
	{
		it->second.startClosing();
		// Nothing this client says from now on will be acted upon, so stop
		// asking poll() about its input: it is only the queued reply that
		// still has somewhere to go.
		updateClientPollEvents(fd);
	}
}

bool Server::isMarkedForRemoval(int fd) const
{
	return _pendingRemoval.find(fd) != _pendingRemoval.end();
}

void Server::removePendingClients()
{
	// Take the whole set aside first: removeClient() must be free to mark
	// another client (a broadcast to a peer that is gone, for instance)
	// without modifying the container being iterated here. Anything marked
	// during this call is simply handled in the next pass.
	std::set<int> pending;
	pending.swap(_pendingRemoval);

	const std::time_t now = std::time(NULL);
	for (std::set<int>::const_iterator it = pending.begin(); it != pending.end(); ++it)
	{
		std::map<int, Client>::const_iterator clientIt = _clients.find(*it);
		// close() throws away whatever is still queued, so a client being
		// disconnected is held on to until the reply explaining why has gone
		// out -- but never longer than CLOSING_LINGER, or one that simply
		// stops reading would keep its descriptor alive by doing nothing.
		if (clientIt != _clients.end()
			&& clientIt->second.hasPendingOutput()
			&& now - clientIt->second.getClosingTime() < CLOSING_LINGER)
		{
			_pendingRemoval.insert(*it);
			continue;
		}
		removeClient(*it);
	}
}

void Server::addToChannel(Client &client, Channel &channel)
{
	channel.addMember(client.getFd());
	client.joinChannel(channel.getName());
}

void Server::removeFromChannel(Client &client, std::string channelName)
{
	// Taken by value on purpose: a caller may well be handing over the key
	// of the very map entry this function is about to erase.
	const std::string key = normalizeIrcName(channelName);

	client.leaveChannel(key);

	std::map<std::string, Channel>::iterator it = _channels.find(key);
	if (it == _channels.end())
		return;

	// removeMember() drops the operator privilege along with the membership.
	it->second.removeMember(client.getFd());

	// A channel exists only as long as it has members: the last one to leave
	// takes its topic, its modes and its operator list with it.
	if (it->second.isEmpty())
		_channels.erase(it);
}

void Server::removeFromAllChannels(Client &client)
{
	// Walk the channels rather than the client's own list: should a fd ever
	// end up in a channel without that list being updated, this still cleans
	// it out. Leaving an fd behind in a channel would mean writing to a
	// descriptor that the system may already have handed to someone else.
	for (std::map<std::string, Channel>::iterator it = _channels.begin(); it != _channels.end(); )
	{
		// removeFromChannel() may erase the entry, so step past it first.
		std::map<std::string, Channel>::iterator current = it++;
		if (current->second.hasMember(client.getFd()))
			removeFromChannel(client, current->first);
	}
}

void Server::broadcastQuit(const Client &client)
{
	// An unregistered client has no name to announce and has joined nothing.
	if (!client.isRegistered())
		return;

	// Collect the peers while the client is still in its channels: once it
	// is out of them, there is no way left to know who shared one with it.
	std::set<int> recipients;
	for (std::map<std::string, Channel>::const_iterator it = _channels.begin();
		it != _channels.end(); ++it)
	{
		if (!it->second.hasMember(client.getFd()))
			continue;
		const std::set<int> &members = it->second.getMembers();
		recipients.insert(members.begin(), members.end());
	}

	// A set gives each peer exactly one notification, however many channels
	// they had in common. The one leaving is not among them.
	recipients.erase(client.getFd());

	const std::string notification =
		":" + client.getPrefix() + " QUIT :" + client.getQuitReason();
	for (std::set<int>::const_iterator it = recipients.begin(); it != recipients.end(); ++it)
		queueMessage(*it, notification);
}

void Server::removeClient(int fd)
{
	std::map<int, Client>::iterator clientIt = _clients.find(fd);
	if (clientIt != _clients.end())
	{
		// Announce first, leave the channels second: the announcement needs
		// the memberships that the next call is about to undo.
		broadcastQuit(clientIt->second);
		// Leave every channel before close() allows this fd to be reused.
		removeFromAllChannels(clientIt->second);
	}

	for (std::vector<struct pollfd>::iterator it = _pollFds.begin(); it != _pollFds.end(); ++it)
	{
		if (it->fd == fd)
		{
			_pollFds.erase(it);
			break;
		}
	}

	_clients.erase(fd);
	close(fd);

	std::cout << "Removed client fd " << fd << std::endl;
}

void Server::disconnectStaleClients()
{
	const std::time_t now = std::time(NULL);
	for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (it->second.isRegistered() || isMarkedForRemoval(it->first))
			continue;
		if (now - it->second.getConnectionTime() < REGISTRATION_TIMEOUT)
			continue;

		queueMessage(it->first, "ERROR :Closing Link: " + it->second.getHostname()
			+ " (Registration timeout)");
		markForRemoval(it->first);
	}
}

void Server::pollLoop()
{
	// Register the listening socket and ask poll() to report incoming data.
	struct pollfd serverPoll;
	serverPoll.fd = _serverFd;
	serverPoll.events = POLLIN;
	serverPoll.revents = 0;
	// Keep the listening socket in the list so new connections and clients
	// can be handled through the same poll() loop.
	_pollFds.push_back(serverPoll);

	std::cout << "Waiting for activity on the server socket..." << std::endl;

	while (true)
	{
		// Wait indefinitely until one of the monitored descriptors has an event.
		int ready = poll(&_pollFds[0], _pollFds.size(), POLL_TIMEOUT_MS);
		if (ready < 0)
		{
			// A signal may interrupt poll(); retry instead of treating it as a fatal error.
			if (errno == EINTR)
				continue;
			throw std::runtime_error(std::string("poll: ") + std::strerror(errno));
		}

		// poll() fills revents with the events that occurred for each descriptor.
		// The events field remains the subscription used for the next call.

		// Only inspect the descriptors that were already monitored before this
		// pass; acceptNewClients() may push_back() new client entries, which
		// must wait for the next poll() call instead of being processed now.
		size_t count = _pollFds.size();

		for (size_t i = 0; i < count; ++i)
		{
			short revents = _pollFds[i].revents;
			if (revents == 0)
				continue;

			int fd = _pollFds[i].fd;

			if (fd == _serverFd)
			{
				if (revents & POLLIN)
					acceptNewClients();
				continue;
			}

			// This client is already on its way out: the only thing left to
			// do with it is push out the reply that says why.
			if (isMarkedForRemoval(fd))
			{
				if (revents & POLLOUT)
					sendToClient(fd);
				continue;
			}

			// A peer can send its last bytes and close in the same instant, so
			// the kernel may report POLLIN and POLLHUP/POLLERR together. Read
			// first and always extract whatever complete lines that leaves in
			// the buffer. Defer a hangup while input is readable so data larger
			// than one receive buffer is processed across successive polls.
			bool stillConnected = true;
			if (revents & POLLIN)
				stillConnected = receiveFromClient(fd);

			extractCompleteLines(fd);

			// Only try to flush output if the client is still there and no
			// handler decided to close it; no point writing to a connection we
			// already know is gone, or that is about to be.
			if (stillConnected && !isMarkedForRemoval(fd) && (revents & POLLOUT))
				stillConnected = sendToClient(fd);

			if (!stillConnected || (revents & (POLLERR | POLLNVAL))
				|| ((revents & POLLHUP) && !(revents & POLLIN)))
				markForRemoval(fd);
		}

		disconnectStaleClients();

		// The single point where clients are erased: removeClient() modifies
		// _clients and _pollFds, which the loop above is still indexing.
		removePendingClients();
	}
}

void Server::run()
{
	ignoreSigpipe();
	initSocket();
	pollLoop();
}

void Server::handlePass(Client &client, const IrcMessage &msg)
{
	// Unregistered clients do not yet have a nickname, so IRC errors use '*'.
	std::string target;

	if (client.getNickname().empty())
		target = "*";
	else
		target = client.getNickname();

	if (client.isRegistered())
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 462 " + target + " :You may not reregister");
		return;
	}

	if (msg.params.size() != 1)
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 461 " + target + " PASS :Not enough parameters");
		return;
	}

	if (msg.params[0] != _password)
	{
		client.setPasswordAccepted(false);
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 464 " + target + " :Password incorrect");
		// Without the password there is nothing this connection can go on to
		// do, so it is told why and shown the door instead of being left to
		// sit there for as long as it likes.
		queueMessage(client.getFd(), "ERROR :Closing Link: "
			+ client.getHostname() + " (Bad password)");
		markForRemoval(client.getFd());
		return;
	}

	client.setPasswordAccepted(true);
}

void Server::handleNick(Client &client, const IrcMessage &msg)
{
	std::string target = client.getNickname().empty() ? "*" : client.getNickname();

	if (!client.hasAcceptedPassword())
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 464 " + target + " :Password incorrect");
		return;
	}

	// "NICK" with nothing after it, and "NICK :" with an empty parameter, are
	// the same thing to the user: no nickname was given.
	if (msg.params.empty() || msg.params[0].empty())
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 431 " + target + " :No nickname given");
		return;
	}

	const std::string &nickname = msg.params[0];
	if (msg.params.size() != 1 || !isValidNickname(nickname))
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 432 " + target + " " + safeParameter(nickname) + " :Erroneous nickname");
		return;
	}

	if (isNicknameInUse(nickname, client.getFd()))
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 433 " + target + " " + safeParameter(nickname) + " :Nickname is already in use");
		return;
	}

	if (client.isRegistered())
	{
		if (client.getNickname() == nickname)
			return;

		// The prefix carries the name the client had until now: that is how
		// its peers know which of them is the one renaming itself.
		const std::string notification = ":" + client.getPrefix() + " NICK :" + nickname;
		client.setNickname(nickname);
		queueMessage(client.getFd(), notification);

		// Notify each registered peer once, even when several channels are shared.
		const std::set<std::string> &channels = client.getChannels();
		for (std::map<int, Client>::const_iterator it = _clients.begin(); it != _clients.end(); ++it)
		{
			if (it->first == client.getFd() || !it->second.isRegistered())
				continue;
			for (std::set<std::string>::const_iterator channel = channels.begin(); channel != channels.end(); ++channel)
			{
				if (it->second.isInChannel(*channel))
				{
					queueMessage(it->first, notification);
					break;
				}
			}
		}
		return;
	}

	client.setNickname(nickname);
	// NICK and USER may arrive in either order after PASS.
	if (client.tryRegister())
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME + " 001 "
			+ client.getNickname() + " :Welcome to the ft_irc server");
	}
}

//USER <username> <mode> <unused> :<realname>
void Server::handleUser(Client &client, const IrcMessage &msg)
{
	std::string target;

	if (client.getNickname().empty())
		target = "*";
	else
		target = client.getNickname();

	if (!client.hasAcceptedPassword())
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 464 " + target + " :Password incorrect");
		return;
	}

	if (client.isRegistered())
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 462 " + target + " :You may not reregister");
		return;
	}

	if (msg.params.size() != 4)
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 461 " + target + " USER :Not enough parameters");
		return;
	}

	client.setUsername(msg.params[0]);
	client.setRealname(msg.params[3]);

	// NICK and USER may arrive in either order after PASS.
	if (client.tryRegister())
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME + " 001 "
			+ client.getNickname() + " :Welcome to the ft_irc server");
	}
}

void Server::handleJoin(Client &client, const IrcMessage &msg)
{
	if (msg.params.empty() || msg.params[0].empty())
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 461 " + client.getNickname() + " JOIN :Not enough parameters");
		return;
	}

	// JOIN takes a list of channels and, after it, an optional list of the
	// keys that go with them, paired by position. The keys are accepted but
	// not checked: no channel can have one until MODE +k stores it, and that
	// is where pairing them and answering 475 (ERR_BADCHANNELKEY) belongs.
	// What matters here is that the key form stops being an error, since
	// without it there would be no way into a channel that has a key.
	const std::vector<std::string> channels = splitOnCommas(msg.params[0]);
	for (std::vector<std::string>::const_iterator it = channels.begin();
		it != channels.end(); ++it)
	{
		if (!isValidChannelName(*it))
		{
			// One bad name in the list is refused on its own: the channels
			// named next to it are still perfectly good.
			queueMessage(client.getFd(), std::string(":") + SERVER_NAME
				+ " 476 " + client.getNickname() + " "
				+ safeParameter(*it) + " :Bad Channel Mask");
			continue;
		}

		// Channels are looked up without regard to case, so the key is the
		// folded name; but the channel is created carrying the spelling it
		// was asked for, and that spelling is what every client is shown
		// from then on, whichever way they typed it themselves.
		const std::string key = normalizeIrcName(*it);
		std::map<std::string, Channel>::iterator channel = _channels.find(key);
		const bool created = channel == _channels.end();
		if (created)
			channel = _channels.insert(std::make_pair(key, Channel(*it))).first;
		else if (channel->second.hasMember(client.getFd()))
			continue;

		const std::string &channelName = channel->second.getName();

		addToChannel(client, channel->second);
		// Whoever brings a channel into being is left in charge of it.
		if (created)
			channel->second.addOperator(client.getFd());

		std::cout << client.getNickname() << " joined "
			<< channelName << " channel." << std::endl;

		const std::string notification = ":" + client.getPrefix() + " JOIN :" + channelName;
		const std::set<int> &members = channel->second.getMembers();
		for (std::set<int>::const_iterator member = members.begin();
			member != members.end(); ++member)
			queueMessage(*member, notification);

		sendJoinReplies(client, channel->second);
	}
}

void Server::sendJoinReplies(const Client &client, const Channel &channel)
{
	const std::string target = client.getNickname() + " " + channel.getName();
	if (channel.getTopic().empty())
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 331 " + target + " :No topic is set");
	else
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 332 " + target + " :" + channel.getTopic());

	const std::string namesPrefix = std::string(":") + SERVER_NAME + " 353 "
		+ client.getNickname() + " = " + channel.getName() + " :";
	std::string names;
	for (std::map<int, Client>::const_iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (!channel.hasMember(it->first))
			continue;
		const std::string entry = (channel.isOperator(it->first) ? "@" : "")
			+ it->second.getNickname();
		// Split between nicknames, leaving two bytes for the terminating CRLF.
		// The names are split across several replies rather than truncated:
		// a member list must arrive whole, however many members there are.
		if (!names.empty()
			&& namesPrefix.size() + names.size() + 1 + entry.size() > IRC_MESSAGE_MAX_CONTENT)
		{
			queueMessage(client.getFd(), namesPrefix + names);
			names.clear();
		}
		if (!names.empty())
			names += " ";
		names += entry;
	}
	if (!names.empty())
		queueMessage(client.getFd(), namesPrefix + names);
	queueMessage(client.getFd(), std::string(":") + SERVER_NAME
		+ " 366 " + target + " :End of NAMES list");
}



void Server::handlePart(Client &client, const IrcMessage &msg)
{
	if (msg.params.empty() || msg.params[0].empty())
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 461 " + client.getNickname() + " PART :Not enough parameters");
		return;
	}

	// The part message is optional. Without one the notification carries no
	// trailing parameter at all, rather than an empty one.
	std::string reason;
	if (msg.params.size() > 1)
		reason = " :" + msg.params[1];

	const std::vector<std::string> channels = splitOnCommas(msg.params[0]);
	for (std::vector<std::string>::const_iterator it = channels.begin();
		it != channels.end(); ++it)
	{
		std::map<std::string, Channel>::iterator channel =
			_channels.find(normalizeIrcName(*it));

		if (channel == _channels.end())
		{
			queueMessage(client.getFd(), std::string(":") + SERVER_NAME
				+ " 403 " + client.getNickname() + " " + safeParameter(*it)
				+ " :No such channel");
			continue;
		}

		if (!channel->second.hasMember(client.getFd()))
		{
			queueMessage(client.getFd(), std::string(":") + SERVER_NAME
				+ " 442 " + client.getNickname() + " " + channel->second.getName()
				+ " :You're not on that channel");
			continue;
		}

		// Announce to the whole channel, the one leaving included: its own
		// copy of this message is how its client learns the PART went
		// through. Sent before the membership is undone, so the list of
		// members is still the one this message is about.
		const std::string notification = ":" + client.getPrefix() + " PART "
			+ channel->second.getName() + reason;
		const std::set<int> &members = channel->second.getMembers();
		for (std::set<int>::const_iterator member = members.begin();
			member != members.end(); ++member)
			queueMessage(*member, notification);

		// Takes the name by value, which matters here: the call may erase the
		// very channel whose name is being handed over.
		removeFromChannel(client, channel->second.getName());
	}
}

void Server::handlePrivmsg(Client &client, const IrcMessage &msg)
{
	if (msg.params.size() < 2)
	{
		std::cout << "Not enough parameters" << std::endl;
		return ;
	}

	std::string target = normalizeIrcName(msg.params[0]);
	std::string msgprint;

	// Example MSG: :Wardog_E!wardoge@localhost PRIVMSG adios :hola

	msgprint = ":" + client.getNickname() + "!" + client.getUsername() + " " + "PRIVMSG " + target;
	for (std::vector<std::string>::const_iterator it = msg.params.begin(); it != msg.params.end(); ++it)
	{
		msgprint += " ";
		if(it == msg.params.begin())
			++it;
		msgprint += *it;
	}

	if (msg.params[0][0] == '#')
	{
		std::map<std::string, Channel>::iterator it = _channels.find(target);
		if (it != _channels.end())
		{
			Channel &channel = it->second;
			if (channel.hasMember(client.getFd()))
			{
				for (std::map<int, Client>::iterator clientIt = _clients.begin();
					clientIt != _clients.end(); ++clientIt)
				{
					if (clientIt->first != client.getFd()
						&& channel.hasMember(clientIt->first))
					{
						queueMessage(clientIt->first, msgprint);
						std::cout << "Sending message through channel." << std::endl;
					}	
				}
			}
		}
	}
	else
	{
		std::map<int, Client>::iterator it;

		for (it = _clients.begin(); it != _clients.end(); ++it)
		{
			if (areSameNicknames(it->second.getNickname(), target))
			{
				Client &recipient = it->second;
				queueMessage(recipient.getFd(), msgprint);
				std::cout << "Sending private message." << std::endl;
				break;
			}
		}
	}

	std::cout << client.getNickname() << std::endl;
	std::cout << msg.command << std::endl;
}

void Server::handleQuit(Client &client, const IrcMessage &msg)
{
	std::string quitmsg;

	for (std::vector<std::string>::const_iterator it = msg.params.begin(); it != msg.params.end(); ++it)
	{
		if (it != msg.params.begin())
			quitmsg += " ";
		quitmsg += *it;
	}

	queueMessage(client.getFd(),
		":" + client.getNickname() + "!" + client.getUsername() + " QUIT " + quitmsg);
	//client needs to be removed from server
	std::cout << "Removed client fd " << std::endl;
}
void Server::handleKick(Client &client, const IrcMessage &msg)
{
	if (msg.params.size() != 1)
	{
		queueMessage(client.getFd(),
		"Only one parameter permitted for kick");
	}
	client.leaveChannel(msg.params[0]);
	std::cout << msg.command << std::endl;
	std::cout << "Kicking user " << client.getNickname() << std::endl;
}
