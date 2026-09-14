/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Mode.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vbullock <vbullock@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 23:30:55 by apestana          #+#    #+#             */
/*   Updated: 2026/09/14 17:46:50 by vbullock         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "IrcCaseMapping.hpp"
#include "IrcParameters.hpp"
#include <cerrno>
#include <climits>
#include <cstdlib>

static bool parseLimit(const std::string &value, int &limit)
{
	if (value.empty())
		return false;

	char *end = 0;
	errno = 0;
	long parsed = std::strtol(value.c_str(), &end, 10);
	if (errno == ERANGE || *end != '\0' || parsed <= 0 || parsed > INT_MAX)
		return false;
	limit = static_cast<int>(parsed);
	return true;
}

void Server::handleMode(Client &client, const IrcMessage &msg)
{
	if (msg.params.size() < 2 || msg.params[0].empty() || msg.params[1].empty())
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 461 " + client.getNickname() + " MODE :Not enough parameters");
		return;
	}

	std::map<std::string, Channel>::iterator it =
		_channels.find(normalizeIrcName(msg.params[0]));
	if (it == _channels.end())
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 403 " + client.getNickname() + " "
			+ IrcParameters::safeParameter(msg.params[0]) + " :No such channel");
		return;
	}
	Channel &channel = it->second;

	if (!channel.hasMember(client.getFd()))
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 442 " + client.getNickname() + " " + channel.getName()
			+ " :You're not on that channel");
		return;
	}
	if (!channel.isOperator(client.getFd()))
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 482 " + client.getNickname() + " " + channel.getName()
			+ " :You're not channel operator");
		return;
	}

	bool adding = true;
	char lastSign = 0;
	std::string appliedModes;
	std::string appliedParameters;
	size_t parameter = 2;
	for (size_t i = 0; i < msg.params[1].size(); ++i)
	{
		const char mode = msg.params[1][i];
		if (mode == '+' || mode == '-')
		{
			adding = mode == '+';
			continue;
		}
		if (mode != 'i' && mode != 't' && mode != 'k' && mode != 'o' && mode != 'l')
		{
			queueMessage(client.getFd(), std::string(":") + SERVER_NAME
				+ " 472 " + client.getNickname() + " " + mode
				+ " :is unknown mode char to me");
			continue;
		}

		std::string modeParameter;
		if (mode == 'k' || mode == 'o' || (mode == 'l' && adding))
		{
			if (parameter >= msg.params.size())
			{
				queueMessage(client.getFd(), std::string(":") + SERVER_NAME
					+ " 461 " + client.getNickname() + " MODE :Not enough parameters");
				continue;
			}
			modeParameter = msg.params[parameter++];
		}

		if (mode == 'l' && adding)
		{
			int limit = 0;
			if (!parseLimit(modeParameter, limit))
			{
				queueMessage(client.getFd(), std::string(":") + SERVER_NAME
					+ " 461 " + client.getNickname() + " MODE :Invalid channel limit");
				continue;
			}
			channel.setLimit(limit);
		}
		else if (mode == 'l')
			channel.setLimit(0);
		else if (mode == 'k')
			channel.setChannelKey(adding ? modeParameter : "");
		else if (mode == 'o')
		{
			std::map<int, Client>::iterator target = findClientByNickname(modeParameter);
			if (target == _clients.end() || !channel.hasMember(target->first))
				continue;
			if (adding)
				channel.addOperator(target->first);
			else
				channel.removeOperator(target->first);
		}

		if (adding)
			channel.addMode(mode);
		else
			channel.removeMode(mode);
		if (lastSign != (adding ? '+' : '-'))
		{
			appliedModes += adding ? "+" : "-";
			lastSign = adding ? '+' : '-';
		}
		appliedModes += mode;
		if (mode == 'k' || mode == 'o' || (mode == 'l' && adding))
		{
			if (!appliedParameters.empty())
				appliedParameters += " ";
			appliedParameters += modeParameter;
		}
	}

	if (appliedModes.empty())
		return;
	const std::string message = ":" + client.getPrefix() + " MODE "
		+ channel.getName() + " " + appliedModes
		+ (appliedParameters.empty() ? "" : " " + appliedParameters);
	for (std::set<int>::const_iterator member = channel.getMembers().begin();
		member != channel.getMembers().end(); ++member)
		queueMessage(*member, message);
}
