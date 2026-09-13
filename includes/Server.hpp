/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/27 23:16:28 by apestana          #+#    #+#             */
/*   Updated: 2026/09/13 16:15:32 by apestana         ###   ########.fr       */
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

// Store the configuration and expose the server lifecycle entry point.
class Server
{
	public:
		// Initialize the server with the listening port and client password.
		Server(int port, const std::string &password);
		~Server();

		// Start the server's main runtime logic.
		void run();

	private:
		// Disable default construction and copying for this server instance.
		Server();
		Server(const Server &other);
		Server &operator=(const Server &other);

		// Ignore SIGPIPE so a send() to an already-closed client cannot kill the process.
		void ignoreSigpipe();
		// Create, bind and listen on the server's TCP socket.
		void initSocket();
		// Add O_NONBLOCK to a file descriptor's existing flags.
		void setNonBlocking(int fd);
		// Accept every pending connection on the server socket.
		void acceptNewClients();
		// Read once after POLLIN; EOF or an error means the client must be removed.
		bool receiveFromClient(int fd);
		// Pull every complete "\r\n"-terminated line out of a client's buffer.
		void extractCompleteLines(int fd);
		// Queue a line for a client, appending "\r\n" exactly once.
		void queueMessage(int fd, const std::string &message);
		// Write once after POLLOUT, preserving unsent bytes for the next event;
		// false means the client must be removed.
		bool sendToClient(int fd);
		// Sync a client's pollfd events with whether it has pending output.
		void updateClientPollEvents(int fd);
		// Mark a client as "must be disconnected" without touching any
		// container. Command handlers must call this instead of removeClient():
		// while a handler runs, both extractCompleteLines() and pollLoop() are
		// still holding an iterator/index into _clients and _pollFds.
		void markForRemoval(int fd);
		bool isMarkedForRemoval(int fd) const;
		// Disconnect every client marked during this pass. Called from one
		// single place, once both loops above are done with their iterators.
		void removePendingClients();
		// The only operations allowed to change a membership. They keep the
		// two sides of it in step -- Channel::_members, which every lookup
		// reads, and the client's own channel list, which the NICK
		// notification walks -- and they destroy a channel once its last
		// member leaves. No handler may touch either side on its own.
		void addToChannel(Client &client, Channel &channel);
		void removeFromChannel(Client &client, std::string channelName);
		// Take a client out of every channel before it is destroyed.
		void removeFromAllChannels(Client &client);
		// Tell everyone sharing a channel with this client that it is leaving.
		// Must run before it is taken out of its channels.
		void broadcastQuit(const Client &client);
		// Remove channel membership, then close the fd and erase the client.
		void removeClient(int fd);
		// Drop the connections that are taking too long to register: an
		// unauthenticated client must not hold a descriptor open for ever.
		void disconnectStaleClients();
		// Watch every monitored descriptor with poll() and report activity.
		void pollLoop();
		
		// New stuff
		void processMessage(Client &client, const IrcMessage &msg);
		bool isValidNickname(const std::string &nickname) const;
		bool isNicknameInUse(const std::string &nickname, int ignoredFd) const;
	
		void handlePass(Client &client, const IrcMessage &msg);
		void handleNick(Client &client, const IrcMessage &msg);
		void handleUser(Client &client, const IrcMessage &msg);
		void handleJoin(Client &client, const IrcMessage &msg);
		void handlePart(Client &client, const IrcMessage &msg);
		// Send the topic and member list to a client after a successful JOIN.
		void sendJoinReplies(const Client &client, const Channel &channel);
		void handlePrivmsg(Client &client, const IrcMessage &msg);
		void handleQuit(Client &client, const IrcMessage &msg);
		void handleKick(Client &client, const IrcMessage &msg);
		void handleMode(Client &client, const IrcMessage &msg);
		void handleTopic(Client &client, const IrcMessage &msg);


		int                       _port;
		std::string               _password;
		int                       _serverFd;
		// File descriptors monitored by the poll() event loop.
		std::vector<struct pollfd> _pollFds;
		// Connected clients, keyed by their fd.
		std::map<int, Client>     _clients;
		// Clients to disconnect at the end of the current poll() pass.
		std::set<int>             _pendingRemoval;
		// Channel keys use normalizeIrcName() so all spellings share one entry.
		std::map<std::string, Channel> _channels;
};

#endif
