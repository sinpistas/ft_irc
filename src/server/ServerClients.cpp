/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerClients.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 23:28:27 by apestana          #+#    #+#             */
/*   Updated: 2026/09/17 14:07:22 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "IrcCaseMapping.hpp"
#include <iostream>
#include <new>
#include <unistd.h>
#include <ctime>

// Registration deadline and maximum time to flush a closing reply.
static const std::time_t REGISTRATION_TIMEOUT = 60;
static const std::time_t CLOSING_LINGER = 2;

std::map<int, Client>::iterator Server::findClientByNickname(const std::string &nickname)
{
	for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (areSameNicknames(it->second.getNickname(), nickname))
			return it;
	}
	return _clients.end();
}

void Server::markForRemoval(int fd)
{
	// Only changes closing state: erasing here would invalidate the
	// iterators and indices that the read loop and the poll loop are using.
	std::map<int, Client>::iterator it = _clients.find(fd);
	if (it == _clients.end())
		return;

	if (!isMarkedForRemoval(fd))
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
	std::map<int, Client>::const_iterator it = _clients.find(fd);
	return it != _clients.end() && it->second.getClosingTime() != 0;
}

void Server::abortConnection(int fd)
{
	std::map<int, Client>::iterator it = _clients.find(fd);
	if (it == _clients.end())
		return;
	// A failed transport cannot deliver a closing reply. Drop its queue so
	// removal happens this pass, without another send or a two-second wait.
	it->second.consumeSendBuffer(it->second.getSendBuffer().size());
	markForRemoval(fd);
	updateClientPollEvents(fd);
}

void Server::handleMemoryFailure(int fd)
{
	std::map<int, Client>::iterator it = _clients.find(fd);
	if (it == _clients.end())
		return;
	it->second.failForMemory();
	updateClientPollEvents(fd);
}

void Server::removePendingClients()
{
	const std::time_t now = std::time(NULL);
	for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); )
	{
		// Advance before erasing; broadcasts can mark other clients but
		// never erase them. No temporary set or allocation is needed.
		std::map<int, Client>::iterator clientIt = it++;
		if (!isMarkedForRemoval(clientIt->first))
			continue;
		// close() throws away whatever is still queued, so a client being
		// disconnected is held on to until the reply explaining why has gone
		// out -- but never longer than CLOSING_LINGER, or one that simply
		// stops reading would keep its descriptor alive by doing nothing.
		if (!clientIt->second.hasMemoryFailure()
			&& clientIt->second.hasPendingOutput()
			&& now - clientIt->second.getClosingTime() < CLOSING_LINGER)
		{
			continue;
		}
		removeClient(clientIt->first);
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
		queueMessage(*it, notification, true);
}

void Server::removeClient(int fd)
{
	std::map<int, Client>::iterator clientIt = _clients.find(fd);
	if (clientIt != _clients.end())
	{
		// Announce first, leave the channels second: the announcement needs
		// the memberships that the next call is about to undo.
		try
		{
			broadcastQuit(clientIt->second);
		}
		catch (const std::bad_alloc &)
		{
			// A QUIT notification is best effort under memory pressure.
			// Removing memberships and closing the socket must still finish.
		}
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

		try
		{
			queueMessage(it->first, "ERROR :Closing Link: " + it->second.getHostname()
				+ " (Registration timeout)");
			markForRemoval(it->first);
		}
		catch (const std::bad_alloc &)
		{
			handleMemoryFailure(it->first);
		}
	}
}
