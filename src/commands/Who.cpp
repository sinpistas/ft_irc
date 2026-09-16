/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Who.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/16 22:34:10 by apestana          #+#    #+#             */
/*   Updated: 2026/09/17 00:11:01 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "IrcCaseMapping.hpp"
#include "IrcParameters.hpp"

// Match IRC masks without recursion or allocations. '*' consumes any number
// of bytes, '?' consumes one, and a backslash quotes the following character.
// Remember the last star so a failed suffix can retry with a longer match.
static bool matchesWhoMask(const std::string &mask, const std::string &text)
{
	size_t pattern = 0;
	size_t position = 0;
	size_t star = std::string::npos;
	size_t retry = 0;
	while (position < text.size())
	{
		if (pattern < mask.size() && mask[pattern] == '*')
		{
			star = ++pattern;
			retry = position;
			continue;
		}
		const bool escaped = pattern + 1 < mask.size() && mask[pattern] == '\\';
		const size_t literal = pattern + (escaped ? 1 : 0);
		if (literal < mask.size() && ((!escaped && mask[literal] == '?')
			|| foldIrcCase(mask[literal]) == foldIrcCase(text[position])))
		{
			pattern = literal + 1;
			++position;
		}
		else if (star != std::string::npos)
		{
			pattern = star;
			position = ++retry;
		}
		else
			return false;
	}
	while (pattern < mask.size() && mask[pattern] == '*')
		++pattern;
	return pattern == mask.size();
}

static bool shareWhoChannel(const Client &left, const Client &right)
{
	for (std::set<std::string>::const_iterator it = left.getChannels().begin();
		it != left.getChannels().end(); ++it)
		if (right.isInChannel(*it))
			return true;
	return false;
}

// 352 fields have a fixed order that HexChat uses to fill its user list.
// There are no away states or IRC operators here: H means present, and @
// means operator of this particular channel. Every user is on this server.
void Server::sendWhoReply(const Client &requester, const Client &target,
	const Channel *channel)
{
	const std::string name = channel ? channel->getName() : "*";
	const std::string flags = channel && channel->isOperator(target.getFd()) ? "H@" : "H";
	// USER permits a leading colon, which cannot be echoed as a middle field.
	// Keep the remaining fields aligned even for such an unusual username.
	queueMessage(requester.getFd(), std::string(":") + SERVER_NAME + " 352 "
		+ requester.getNickname() + " " + name + " "
		+ IrcParameters::safeParameter(target.getUsername()) + " " + target.getHostname()
		+ " " + SERVER_NAME + " " + target.getNickname() + " " + flags
		+ " :0 " + target.getRealname(), true);
}

void Server::handleWho(Client &client, const IrcMessage &msg)
{
	const std::string prefix = std::string(":") + SERVER_NAME;
	// No mask is a valid WHO query. Only the optional IRC-operator filter
	// 'o' is supported; WHOX has not been advertised or implemented.
	if (msg.params.size() > 2 || (msg.params.size() == 2 && msg.params[1] != "o"))
	{
		queueMessage(client.getFd(), prefix + " 461 " + client.getNickname()
			+ " WHO :Syntax: WHO [<mask> [o]]");
		return;
	}
	std::vector<std::string> masks;
	if (!msg.params.empty())
		masks = IrcParameters::splitOnCommas(msg.params[0]);
	if (masks.empty())
		masks.push_back("0");
	const bool operatorsOnly = msg.params.size() == 2;
	for (size_t i = 0; i < masks.size() && !isMarkedForRemoval(client.getFd()); ++i)
	{
		const std::string &mask = masks[i];
		std::map<std::string, Channel>::const_iterator found =
			_channels.find(normalizeIrcName(mask));
		const Channel *requestedChannel = found == _channels.end() ? NULL : &found->second;
		const bool allUsers = mask == "0" || mask.find_first_not_of('*') == std::string::npos;
		// 'o' filters IRC operators, not channel operators. No OPER command or
		// server-operator role exists here, so this filter produces an empty list.
		for (std::map<int, Client>::const_iterator it = _clients.begin();
			!operatorsOnly && it != _clients.end() && !isMarkedForRemoval(client.getFd()); ++it)
		{
			const Client &target = it->second;
			if (!target.isRegistered() || isMarkedForRemoval(it->first))
				continue;
			const Channel *shownChannel = requestedChannel;
			if (requestedChannel)
			{
				if (!requestedChannel->hasMember(it->first))
					continue;
			}
			else
			{
				// RFC 2812 3.6.1: the default/0/all-wildcard query excludes users
				// sharing a channel with the requester. User mode +i is absent.
				if (allUsers && shareWhoChannel(client, target))
					continue;
				if (!allUsers && !matchesWhoMask(mask, target.getHostname())
					&& !matchesWhoMask(mask, SERVER_NAME)
					&& !matchesWhoMask(mask, target.getRealname())
					&& !matchesWhoMask(mask, target.getNickname()))
					continue;
				// Outside a channel query, show one of the user's public channels
				// or '*' if they have none. Channel +i restricts JOIN, not WHO.
				for (std::set<std::string>::const_iterator ch = target.getChannels().begin();
					ch != target.getChannels().end(); ++ch)
				{
					found = _channels.find(*ch);
					if (found != _channels.end())
					{
						shownChannel = &found->second;
						break;
					}
				}
			}
			sendWhoReply(client, target, shownChannel);
		}
		// Even no matches (including an unknown channel/nick) must terminate
		// with 315. The mask is not a server target and does not imply error 402.
		queueMessage(client.getFd(), prefix + " 315 " + client.getNickname() + " "
			+ IrcParameters::safeParameter(mask) + " :End of WHO list");
	}
}
