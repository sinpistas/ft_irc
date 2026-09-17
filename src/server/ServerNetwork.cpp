/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerNetwork.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 23:28:11 by apestana          #+#    #+#             */
/*   Updated: 2026/09/17 14:07:23 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include <iostream>
#include <stdexcept>
#include <new>
#include <cstring>
#include <cerrno>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

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
}

void Server::setNonBlocking(int fd)
{
	// The subject allows exactly one form of fcntl(), and this is it. Reading
	// the current flags to merge them in would be the usual way of doing this,
	// but it is not needed here: the only descriptors this is called on come
	// straight out of socket() and accept(), which hand them over with no
	// status flags set, so there is nothing to preserve. Note also that
	// F_SETFL cannot change the access mode, which stays as it was.
	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
		throw std::runtime_error(std::string("fcntl(F_SETFL): ") + std::strerror(errno));
}

void Server::acceptNewClients()
{
	// One accept per listener POLLIN; further connections wait for poll().
	// The peer address supplies the host part of the client's prefix.
	struct sockaddr_in address;
	socklen_t addressLen = sizeof(address);
	std::memset(&address, 0, sizeof(address));

	int clientFd = accept(_serverFd,
		reinterpret_cast<struct sockaddr *>(&address), &addressLen);
	if (clientFd < 0)
	{
		// Apply the same backoff to every failure, including resource
		// exhaustion, without using errno to select a retry.
		pauseAccepting();
		return;
	}

	try
	{
		setNonBlocking(clientFd);

		// Keep the accepted fd private until both containers own their
		// entries. A failed insertion must not leak a socket or poll slot.
		char numericHost[INET_ADDRSTRLEN];
		std::string hostname = "unknown";
		if (inet_ntop(AF_INET, &address.sin_addr, numericHost, sizeof(numericHost)) != NULL)
			hostname = numericHost;

		struct pollfd clientPoll;
		clientPoll.fd = clientFd;
		clientPoll.events = POLLIN;
		clientPoll.revents = 0;
		_clients.insert(std::pair<int, Client>(clientFd, Client(clientFd, hostname)));
		_pollFds.push_back(clientPoll);
	}
	catch (const std::bad_alloc &)
	{
		_clients.erase(clientFd);
		close(clientFd);
		pauseAccepting();
		return;
	}
	catch (const std::exception &)
	{
		close(clientFd);
		return;
	}
}

void Server::pauseAccepting()
{
	// EMFILE/ENFILE leave connections pending: polling POLLIN again would
	// immediately wake us up. Retry after the next deadline instead.
	_acceptRetryAt = std::time(NULL) + 1;
	_pollFds[0].events = 0;
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

		return true;
	}

	// The subject forbids using errno after recv(): EOF or an error removes
	// this client without retrying the operation or stopping the server.
	return false;
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
			return false;
		it->second.consumeSendBuffer(static_cast<size_t>(sent));
	}

	updateClientPollEvents(fd);
	return true;
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
