/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Join.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vbullock <vbullock@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 23:31:11 by apestana          #+#    #+#             */
/*   Updated: 2026/09/15 17:29:54 by vbullock         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "IrcLimits.hpp"
#include "IrcCaseMapping.hpp"
#include "IrcParameters.hpp"
#include <iostream>

// JOIN pairs channels and keys by position. Keep empty fields so a missing
// key (or an ignored empty channel) never shifts the following pairs.
static std::vector<std::string> splitJoinList(const std::string &value)
{
	std::vector<std::string> parts;
	std::string::size_type start = 0;
	while (true)
	{
		const std::string::size_type end = value.find(',', start);
		if (end == std::string::npos)
		{
			parts.push_back(value.substr(start));
			break;
		}
		parts.push_back(value.substr(start, end - start));
		start = end + 1;
	}
	return parts;
}

static bool isValidChannelName(const std::string &name)
{
	if (name.size() < 2 || name.size() > 50 || (name[0] != '#' && name[0] != '&'))
		return false;
	for (std::string::size_type i = 0; i < name.size(); ++i)
	{
		if (name[i] == '\0' || name[i] == '\a' || name[i] == '\r'
			|| name[i] == '\n' || name[i] == ' ' || name[i] == ',' || name[i] == ':')
			return false;
	}
	return true;
}

void Server::handleJoin(Client &client, const IrcMessage &msg)
{
	if (msg.params.empty() || msg.params[0].find_first_not_of(',') == std::string::npos)
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 461 " + client.getNickname() + " JOIN :Not enough parameters");
		return;
	}

	// RFC 2812 3.2.1: JOIN 0 is a PART from every current channel.
	if (msg.params[0] == "0")
	{
		const std::set<std::string> &channels = client.getChannels();
		for (std::set<std::string>::const_iterator it = channels.begin();
			it != channels.end() && !isMarkedForRemoval(client.getFd()); )
		{
			IrcMessage part;
			part.command = "PART";
			part.params.push_back(*it);
			// PART erases this membership. Keep the name in the request
			// and advance first so the next iterator remains valid.
			++it;
			handlePart(client, part);
		}
		return;
	}

	const std::vector<std::string> channels = splitJoinList(msg.params[0]);
	const std::vector<std::string> keys = msg.params.size() > 1
		? splitJoinList(msg.params[1]) : std::vector<std::string>();
	size_t keyIndex = 0;
	for (std::vector<std::string>::const_iterator it = channels.begin();
		it != channels.end() && !isMarkedForRemoval(client.getFd()); ++it, ++keyIndex)
	{
		if (it->empty())
			continue;
		if (!isValidChannelName(*it))
		{
			// One bad name in the list is refused on its own: the channels
			// named next to it are still perfectly good.
			queueMessage(client.getFd(), std::string(":") + SERVER_NAME
				+ " 476 " + client.getNickname() + " "
				+ IrcParameters::safeParameter(*it) + " :Bad Channel Mask");
			continue;
		}

		// Channels are looked up without regard to case, so the key is the
		// folded name; but the channel is created carrying the spelling it
		// was asked for, and that spelling is what every client is shown
		// from then on, whichever way they typed it themselves.
		const std::string key = normalizeIrcName(*it);
		std::map<std::string, Channel>::iterator channel = _channels.find(key);
		const bool created = channel == _channels.end();
		if (created)
			channel = _channels.insert(std::make_pair(key, Channel(*it))).first;
		else if (channel->second.hasMember(client.getFd()))
			continue;

		const std::string &channelName = channel->second.getName();

		// An invite-only channel is entered with an invitation and not
		// otherwise.
		if (!created && channel->second.hasMode('i')
			&& !channel->second.isInvited(client.getFd()))
		{
			queueMessage(client.getFd(), std::string(":") + SERVER_NAME
				+ " 473 " + client.getNickname() + " " + channelName
				+ " :Cannot join channel (+i)");
			continue;
		}

		if (channel->second.hasMode('k')
			&& (keyIndex >= keys.size() || keys[keyIndex] != channel->second.getChannelKey()))
		{
			queueMessage(client.getFd(), std::string(":") + SERVER_NAME
				+ " 475 " + client.getNickname() + " " + channelName
				+ " :Cannot join channel (+k)");
			continue;
		}
		if (channel->second.hasMode('l') && channel->second.getLimit() > 0
			&& channel->second.getMembers().size() >= static_cast<size_t>(channel->second.getLimit()))
		{
			queueMessage(client.getFd(), std::string(":") + SERVER_NAME
				+ " 471 " + client.getNickname() + " " + channelName
				+ " :Cannot join channel (+l)");
			continue;
		}

		addToChannel(client, channel->second);
		// The invitation has done its job.
		channel->second.removeInvite(client.getFd());
		// Whoever brings a channel into being is left in charge of it.
		if (created)
			channel->second.addOperator(client.getFd());

		std::cout << client.getNickname() << " joined "
			<< channelName << " channel." << std::endl;

		const std::string notification = ":" + client.getPrefix() + " JOIN :" + channelName;
		const std::set<int> &members = channel->second.getMembers();
		for (std::set<int>::const_iterator member = members.begin();
			member != members.end(); ++member)
			queueMessage(*member, notification);

		sendJoinReplies(client, channel->second);
	}
}

void Server::sendJoinReplies(const Client &client, const Channel &channel)
{
	const std::string target = client.getNickname() + " " + channel.getName();

	// RFC 2812 3.2.1: a successful JOIN is answered with the channel's topic
	// and its member list. A channel without a topic has nothing to say about
	// it, so nothing is sent. RPL_NOTOPIC (331) is the answer owed to someone
	// who asked for the topic, not part of welcoming anyone into a channel.
	if (!channel.getTopic().empty())
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 332 " + target + " :" + channel.getTopic(), true);

	const std::string namesPrefix = std::string(":") + SERVER_NAME + " 353 "
		+ client.getNickname() + " = " + channel.getName() + " :";
	std::string names;
	for (std::map<int, Client>::const_iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (!channel.hasMember(it->first))
			continue;
		const std::string entry = (channel.isOperator(it->first) ? "@" : "")
			+ it->second.getNickname();
		// Split between nicknames, leaving two bytes for the terminating CRLF.
		// The names are split across several replies rather than truncated:
		// a member list must arrive whole, however many members there are.
		if (!names.empty()
			&& namesPrefix.size() + names.size() + 1 + entry.size() > IRC_MESSAGE_MAX_CONTENT)
		{
			queueMessage(client.getFd(), namesPrefix + names);
			names.clear();
		}
		if (!names.empty())
			names += " ";
		names += entry;
	}
	if (!names.empty())
		queueMessage(client.getFd(), namesPrefix + names);
	queueMessage(client.getFd(), std::string(":") + SERVER_NAME
		+ " 366 " + target + " :End of NAMES list");
}
