/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 23:28:49 by apestana          #+#    #+#             */
/*   Updated: 2026/09/16 01:14:18 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include <iostream>
#include <stdexcept>
#include <new>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <csignal>

const char Server::SERVER_NAME[] = "irc.local";

// Written only by the shutdown signal handler.
static volatile sig_atomic_t g_stopRequested = 0;
// Wake periodically to check registration and closing deadlines.
static const int POLL_TIMEOUT_MS = 1000;

static void requestStop(int)
{
	g_stopRequested = 1;
}

Server::Server(int port, const std::string &password)
	: _port(port), _password(password), _serverFd(-1)
{
}

Server::~Server()
{
	// Close the client connections before the listening socket. Letting the
	// process exit take care of them would work, but only by accident: a
	// server that shuts down on purpose should hand back what it borrowed.
	for (std::map<int, Client>::const_iterator it = _clients.begin();
		it != _clients.end(); ++it)
		close(it->first);
	_clients.clear();

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

void Server::catchShutdownSignals()
{
	struct sigaction sa;
	std::memset(&sa, 0, sizeof(sa));
	sa.sa_handler = requestStop;
	sigemptyset(&sa.sa_mask);
	// No SA_RESTART on purpose: poll() has to come back with EINTR so the
	// loop gets a chance to look at the flag. Restarting it automatically
	// would leave the server blocked until some client happened to speak.
	sa.sa_flags = 0;

	// All three signals request the same orderly shutdown through pollLoop().
	if (sigaction(SIGINT, &sa, NULL) < 0 || sigaction(SIGTERM, &sa, NULL) < 0
		|| sigaction(SIGQUIT, &sa, NULL) < 0)
		throw std::runtime_error(std::string("sigaction: ") + std::strerror(errno));
}

void Server::run()
{
	ignoreSigpipe();
	catchShutdownSignals();
	initSocket();
	pollLoop();
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

	while (!g_stopRequested)
	{
		// Wait for one of the monitored descriptors to have an event, or for
		// the timeout to come round so the registration and closing deadlines
		// get looked at.
		int ready = poll(&_pollFds[0], _pollFds.size(), POLL_TIMEOUT_MS);
		if (ready < 0)
		{
			// A signal interrupted poll(). If it was one asking the server to
			// stop, the loop condition is about to see it; anything else just
			// means going round again.
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

			try
			{
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
			catch (const std::bad_alloc &)
			{
				// recv buffering, parsing and command construction all belong
				// to this connection. Defer erasure until this poll pass ends.
				handleMemoryFailure(fd);
			}
		}

		disconnectStaleClients();

		// The single point where clients are erased: removeClient() modifies
		// _clients and _pollFds, which the loop above is still indexing.
		removePendingClients();
	}

	std::cout << "Shutting down, closing " << _clients.size()
		<< " connection(s)" << std::endl;
}
