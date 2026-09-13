/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Part.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 23:30:35 by apestana          #+#    #+#             */
/*   Updated: 2026/09/13 23:30:37 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "IrcCaseMapping.hpp"
#include "IrcParameters.hpp"

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

	const std::vector<std::string> channels = IrcParameters::splitOnCommas(msg.params[0]);
	for (std::vector<std::string>::const_iterator it = channels.begin();
		it != channels.end(); ++it)
	{
		std::map<std::string, Channel>::iterator channel =
			_channels.find(normalizeIrcName(*it));

		if (channel == _channels.end())
		{
			queueMessage(client.getFd(), std::string(":") + SERVER_NAME
				+ " 403 " + client.getNickname() + " " + IrcParameters::safeParameter(*it)
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
			queueMessage(*member, notification, true);

		// Takes the name by value, which matters here: the call may erase the
		// very channel whose name is being handed over.
		removeFromChannel(client, channel->second.getName());
	}
}
