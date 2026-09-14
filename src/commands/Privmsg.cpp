/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Privmsg.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 23:30:20 by apestana          #+#    #+#             */
/*   Updated: 2026/09/13 23:30:21 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "IrcCaseMapping.hpp"
#include "IrcParameters.hpp"

void Server::handlePrivmsg(Client &client, const IrcMessage &msg)
{
	// RFC 2812 3.3.1: PRIVMSG <msgtarget> <text to be sent>. Each of the two
	// missing has its own answer.
	const std::vector<std::string> targets = msg.params.empty()
		? std::vector<std::string>() : IrcParameters::splitOnCommas(msg.params[0]);
	if (targets.empty())
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 411 " + client.getNickname() + " :No recipient given (PRIVMSG)");
		return;
	}
	if (msg.params.size() < 2 || msg.params[1].empty())
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 412 " + client.getNickname() + " :No text to send");
		return;
	}

	// The text travels as the trailing parameter, behind its own colon. That
	// colon is what tells the receiving client where the message begins and
	// what lets it hold spaces: without it, everything past the first word
	// arrives as separate parameters and the client shows a single word.
	const std::string text = " :" + msg.params[1];

	// A message may be addressed to several targets at once, separated by
	// commas. Resolve each distinct IRC name once, preserving request order.
	// Deduplicate targets, not recipient fds: a user may legitimately receive
	// one message for each of two different channels they share with us.
	std::set<std::string> processedTargets;
	for (std::vector<std::string>::const_iterator it = targets.begin();
		it != targets.end(); ++it)
	{
		const std::string key = normalizeIrcName(*it);
		if (!processedTargets.insert(key).second)
			continue;
		if ((*it)[0] == '#' || (*it)[0] == '&')
		{
			std::map<std::string, Channel>::iterator channel =
				_channels.find(key);
			if (channel == _channels.end())
			{
				queueMessage(client.getFd(), std::string(":") + SERVER_NAME
					+ " 401 " + client.getNickname() + " " + IrcParameters::safeParameter(*it)
					+ " :No such nick/channel");
				continue;
			}

			const std::string line = ":" + client.getPrefix() + " PRIVMSG "
				+ channel->second.getName() + text;
			const std::set<int> &members = channel->second.getMembers();
			for (std::set<int>::const_iterator member = members.begin();
				member != members.end(); ++member)
			{
				// Nobody is sent back what they just said.
				if (*member != client.getFd())
					queueMessage(*member, line, true);
			}
		}
		else
		{
			// A client that has not finished registering has no name to be
			// reached by, so as far as anyone else is concerned it is not there.
			std::map<int, Client>::iterator target = findClientByNickname(*it);
			if (target == _clients.end() || !target->second.isRegistered())
			{
				queueMessage(client.getFd(), std::string(":") + SERVER_NAME
					+ " 401 " + client.getNickname() + " " + IrcParameters::safeParameter(*it)
					+ " :No such nick/channel");
				continue;
			}

			queueMessage(target->first, ":" + client.getPrefix() + " PRIVMSG "
				+ target->second.getNickname() + text, true);
		}
	}
}
