/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vbullock <vbullock@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/27 23:16:08 by apestana          #+#    #+#             */
/*   Updated: 2026/09/09 15:48:25 by vbullock         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "IrcMessage.hpp"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <cerrno>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <csignal>
#include <cctype>

static const char *SERVER_NAME = "irc.local";

static bool isNicknameSpecial(char character)
{
	return character == '[' || character == ']' || character == '\\'
		|| character == '^' || character == '_' || character == '`'
		|| character == '{' || character == '|';
}

static char foldIrcNickname(char character)
{
	if (character >= 'A' && character <= 'Z')
		return static_cast<char>(character - 'A' + 'a');
	if (character == '[')
		return '{';
	if (character == ']')
		return '}';
	if (character == '\\')
		return '|';
	if (character == '~')
		return '^';
	return character;
}

static bool areSameNicknames(const std::string &left, const std::string &right)
{
	if (left.size() != right.size())
		return false;
	for (std::string::size_type i = 0; i < left.size(); ++i)
	{
		if (foldIrcNickname(left[i]) != foldIrcNickname(right[i]))
			return false;
	}
	return true;
}

Server::Server(int port, const std::string &password)
	: _port(port), _password(password), _serverFd(-1)
{
    _channels.insert(std::make_pair("#general", Channel("#general")));
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
		int clientFd = accept(_serverFd, NULL, NULL);
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

		struct pollfd clientPoll;
		clientPoll.fd = clientFd;
		clientPoll.events = POLLIN;
		clientPoll.revents = 0;
		_pollFds.push_back(clientPoll);

		_clients.insert(std::pair<int, Client>(clientFd, Client(clientFd)));

		std::cout << "Accepted new client, fd " << clientFd << std::endl;
	}
}

bool Server::receiveFromClient(int fd)
{
	char buffer[4096];

	while (true)
	{
		ssize_t bytes = recv(fd, buffer, sizeof(buffer), 0);

		if (bytes > 0)
		{
			std::map<int, Client>::iterator it = _clients.find(fd);
			if (it != _clients.end())
				it->second.appendToBuffer(buffer, static_cast<size_t>(bytes));

			std::cout << "Received " << bytes << " bytes from client fd " << fd
				<< ": " << std::string(buffer, static_cast<size_t>(bytes)) << std::endl;
			// Non-blocking: keep draining in case more data is already queued.
			continue;
		}

		if (bytes == 0)
		{
			// Orderly shutdown from the client's side.
			std::cout << "Client fd " << fd << " closed the connection" << std::endl;
			return false;
		}

		// bytes < 0
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return true;
		if (errno == EINTR)
			continue;

		// Any other error must not take the whole server down.
		std::cerr << "recv: " << std::strerror(errno) << std::endl;
		return false;
	}
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
        _channels.find(msg.params[0]);

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
		std::cout << "Topic name changed to: " << msg.params[0] << std::endl;
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


}

bool Server::isValidNickname(const std::string &nickname) const
{
	if (nickname.empty())
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

	std::string line;
	while (it->second.extractLine(line))
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
			// Always watch for readability; only ask for POLLOUT while there
			// is something queued, otherwise poll() would keep reporting it
			// writable forever and spin the loop for no reason.
			it->events = POLLIN;
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

	// Append "\r\n" only if the caller did not already include it, so a
	// message is never queued with the terminator doubled.
	bool alreadyTerminated = message.size() >= 2
		&& message[message.size() - 2] == '\r'
		&& message[message.size() - 1] == '\n';

	it->second.appendToSendBuffer(alreadyTerminated ? message : message + "\r\n");
	updateClientPollEvents(fd);
}

bool Server::sendToClient(int fd)
{
	std::map<int, Client>::iterator it = _clients.find(fd);
	if (it == _clients.end())
		return true;

	while (it->second.hasPendingOutput())
	{
		const std::string &buffer = it->second.getSendBuffer();
		ssize_t sent = send(fd, buffer.data(), buffer.size(), 0);

		if (sent > 0)
		{
			it->second.consumeSendBuffer(static_cast<size_t>(sent));
			continue;
		}

		if (sent == 0)
		{
			// The buffer is non-empty (the while condition guarantees that),
			// so a 0-byte write isn't a legitimate "try again" case; leaving
			// it as one would keep POLLOUT set on a socket that never
			// drains, spinning poll() forever. Treat it as a client error.
			std::cerr << "send: unexpected 0-byte write to fd " << fd << std::endl;
			return false;
		}

		// sent < 0
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			break; // Kernel send buffer is full; the rest waits for the next POLLOUT.
		if (errno == EINTR)
			continue;

		// Any other error must not take the whole server down.
		std::cerr << "send: " << std::strerror(errno) << std::endl;
		return false;
	}

	updateClientPollEvents(fd);
	return true;
}

void Server::removeClient(int fd)
{
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
		int ready = poll(&_pollFds[0], _pollFds.size(), -1);
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
		// Every fd collected here is removed after the loop: removeClient()
		// erases from _pollFds, which would invalidate the indices/iterators
		// this loop is still using if called mid-pass.
		std::vector<int> toRemove;

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

			// A peer can send its last bytes and close in the same instant, so
			// the kernel may report POLLIN and POLLHUP/POLLERR together. Read
			// first and always extract whatever complete lines that leaves in
			// the buffer before honoring the hangup/error below; otherwise a
			// message that arrived right before the close would be dropped.
			bool stillConnected = true;
			if (revents & POLLIN)
				stillConnected = receiveFromClient(fd);

			extractCompleteLines(fd);

			// Only try to flush output if the client is still there; no point
			// writing to a connection we already know is gone.
			if (stillConnected && (revents & POLLOUT))
				stillConnected = sendToClient(fd);

			if (!stillConnected || (revents & (POLLHUP | POLLERR | POLLNVAL)))
				toRemove.push_back(fd);
		}

		for (std::vector<int>::iterator it = toRemove.begin(); it != toRemove.end(); ++it)
			removeClient(*it);
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

	if (msg.params.empty())
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 431 " + target + " :No nickname given");
		return;
	}

	const std::string &nickname = msg.params[0];
	if (msg.params.size() != 1 || !isValidNickname(nickname))
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 432 " + target + " " + nickname + " :Erroneous nickname");
		return;
	}

	if (isNicknameInUse(nickname, client.getFd()))
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 433 " + target + " " + nickname + " :Nickname is already in use");
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
    if (msg.params.size() != 1)
    {
        queueMessage(client.getFd(), "Incorrect number of parameters.");
        return;
    }

    const std::string &channelName = msg.params[0];
    std::map<std::string, Channel>::iterator channel =
        _channels.find(channelName);

    if (channel == _channels.end())
    {
        queueMessage(client.getFd(),
            std::string("No channel found with name: ") + channelName);
        return;
    }

    client.joinChannel(channelName);
    channel->second.addMember(client.getFd());

    std::cout << client.getNickname() << " joined "
              << channelName << " channel." << std::endl;
	queueMessage(client.getFd(),
            std::string("You have joined channel: ") + channelName);
}
void Server::handlePrivmsg(Client &client, const IrcMessage &msg)
{
	std::cout << client.getNickname() << std::endl;
	std::cout << msg.command << std::endl;
	std::cout << "Sending private message." << std::endl;
}
void Server::handleQuit(Client &client, const IrcMessage &msg)
{
	client.leaveChannel("leavechannel");
	std::cout << msg.command << std::endl;
	std::cout << "Removed client fd " << std::endl;
}
void Server::handleKick(Client &client, const IrcMessage &msg)
{
	client.leaveChannel("leavechannel");
	std::cout << msg.command << std::endl;
	std::cout << "Kicking user." << std::endl;
}
