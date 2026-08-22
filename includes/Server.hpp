/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/22 13:58:08 by apestana          #+#    #+#             */
/*   Updated: 2026/08/22 14:19:13 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef __SERVER_HPP__
# define __SERVER_HPP__

# include <string>
# include <map>
# include <vector>
# include <poll.h>

# include "Client.hpp"
# include "Channel.hpp"

class Server
{
	public:

		Server(const std::string &port,
			const std::string &password);
		~Server();

		// Prepare socket, bind, listen...
		void setup();
		// main loop
		void run();

	private:

		/*****attributes*****/
		int         _port;
		std::string _password;

		// Socket listening new connections
		int         _serverFd;

		// main loop control
		bool        _running;

		/*****functions*****/

		// FD list monitored by poll()
		std::vector<struct pollfd> _pollFds;

		// Connected clients:
		// fd -> Client
		std::map<int, Client> _clients;

		// Channels:
		// name -> Channel
		std::map<std::string, Channel> _channels;

		// Create socket and set it up
		void createSocket();

		// bind port
		void bindSocket();

		// Start listening
		void listenSocket();

		// Process events returned by poll()
		void handlePollEvents();

		// Accept a new client
		void acceptClient();

		// Read client data
		void receiveFromClient(int clientFd);

		// Process aggregated client data
		void processClientBuffer(Client &client);

		// Process ful IRC line
		void processCommand(Client &client,
							const std::string &line);

		// Send data
		void sendToClient(int clientFd,
						const std::string &message);

		// Disconnect and cleanning
		void removeClient(int clientFd);

		//  Search for/remove the FD from the poll vector
		void removePollFd(int fd);

};

#endif