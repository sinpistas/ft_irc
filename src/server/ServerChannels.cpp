/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerChannels.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 23:28:43 by apestana          #+#    #+#             */
/*   Updated: 2026/09/13 23:28:45 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "IrcCaseMapping.hpp"
#include <new>

void Server::addToChannel(Client &client, Channel &channel)
{
	channel.addMember(client.getFd());
	try
	{
		client.joinChannel(channel.getName());
	}
	catch (const std::bad_alloc &)
	{
		channel.removeMember(client.getFd());
		throw;
	}
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
		// Erase directly using existing nodes: removeFromChannel() copies
		// and normalizes names, which can allocate during error cleanup.
		std::map<std::string, Channel>::iterator current = it++;
		// An invitation must not outlive the client holding it: descriptors
		// get reused, and the next owner of this one was invited to nothing.
		current->second.removeInvite(client.getFd());
		current->second.removeMember(client.getFd());
		if (current->second.isEmpty())
			_channels.erase(current);
	}
	client.clearChannels();
}
