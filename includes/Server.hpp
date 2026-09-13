/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/27 23:16:28 by apestana          #+#    #+#             */
/*   Updated: 2026/09/13 23:32:05 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <map>
#include <set>
#include <poll.h>
#include "Client.hpp"
#include "Channel.hpp"
#include "IrcMessage.hpp"

// Own the connections and channels; definitions are grouped by responsibility
// in src/server, with each IRC command implemented in src/commands.
class Server
{
	public:
		Server(int port, const std::string &password);
		~Server();
		void run();

	private:
		Server();
		Server(const Server &other);
		Server &operator=(const Server &other);

		// Lifecycle and the single event loop (Server.cpp).
		void ignoreSigpipe();
		void catchShutdownSignals();
		void pollLoop();

		// Socket setup and non-blocking I/O (ServerNetwork.cpp).
		void initSocket();
		void setNonBlocking(int fd);
		void acceptNewClients();
		// Read/write once after the corresponding poll event. False means
		// the connection must be removed; partial output remains queued.
		bool receiveFromClient(int fd);
		bool sendToClient(int fd);
		void updateClientPollEvents(int fd);

		// Client lookup, deadlines and disconnection (ServerClients.cpp).
		std::map<int, Client>::iterator findClientByNickname(const std::string &nickname);
		// Handlers mark instead of erasing: command dispatch and poll still
		// hold iterators. Marking must not allocate, including on bad_alloc.
		void markForRemoval(int fd);
		bool isMarkedForRemoval(int fd) const;
		void handleMemoryFailure(int fd);
		// Called after the poll iteration, when erasing clients is safe.
		void removePendingClients();
		void broadcastQuit(const Client &client);
		void removeClient(int fd);
		void disconnectStaleClients();

		// Membership operations (ServerChannels.cpp). These keep the Client
		// and Channel indexes consistent and erase channels left empty.
		void addToChannel(Client &client, Channel &channel);
		void removeFromChannel(Client &client, std::string channelName);
		// Disconnection cleanup must complete without new allocations.
		void removeFromAllChannels(Client &client);

		// Framing, dispatch and replies (ServerProtocol.cpp).
		void extractCompleteLines(int fd);
		void processMessage(Client &client, const IrcMessage &msg);
		// Append CRLF once. Only free-text trailing parameters may be
		// shortened; prefix, command and targets must remain complete.
		void queueMessage(int fd, const std::string &message, bool truncateText = false);
		// Called once, immediately after registration succeeds.
		void sendWelcome(const Client &client);

		// IRC commands. Each handler is defined in its own src/commands file.
		void handlePass(Client &client, const IrcMessage &msg);
		void handleNick(Client &client, const IrcMessage &msg);
		void handleUser(Client &client, const IrcMessage &msg);
		void handleJoin(Client &client, const IrcMessage &msg);
		void handlePart(Client &client, const IrcMessage &msg);
		void handleInvite(Client &client, const IrcMessage &msg);
		void handlePrivmsg(Client &client, const IrcMessage &msg);
		void handleQuit(Client &client, const IrcMessage &msg);
		void handleKick(Client &client, const IrcMessage &msg);
		void handleTopic(Client &client, const IrcMessage &msg);
		void handleMode(Client &client, const IrcMessage &msg);

		// NICK-only helpers, implemented in Nick.cpp.
		bool isValidNickname(const std::string &nickname) const;
		bool isNicknameInUse(const std::string &nickname, int ignoredFd) const;
		// JOIN-only replies, implemented in Join.cpp.
		void sendJoinReplies(const Client &client, const Channel &channel);

		static const char SERVER_NAME[];
		int _port;
		std::string _password;
		int _serverFd;
		std::vector<struct pollfd> _pollFds;
		// Closing state is stored in Client, so marking never allocates.
		std::map<int, Client> _clients;
		// Keys use normalizeIrcName; Channel preserves the original spelling.
		std::map<std::string, Channel> _channels;
};

#endif
