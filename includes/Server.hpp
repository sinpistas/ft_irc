/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/27 23:16:28 by apestana          #+#    #+#             */
/*   Updated: 2026/09/10 18:30:41 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <map>
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
		// Close a client's fd and drop it from _pollFds and _clients.
		void removeClient(int fd);
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
		std::map<std::string, Channel> _channels;
};

#endif
