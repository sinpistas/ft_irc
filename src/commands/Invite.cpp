/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Invite.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 23:31:18 by apestana          #+#    #+#             */
/*   Updated: 2026/09/13 23:31:20 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "IrcCaseMapping.hpp"
#include "IrcParameters.hpp"
#include "IrcLimits.hpp"

void Server::handleInvite(Client &client, const IrcMessage &msg)
{
	if (msg.params.size() < 2 || msg.params[0].empty() || msg.params[1].empty())
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 461 " + client.getNickname() + " INVITE :Not enough parameters");
		return;
	}

	const std::string &targetNick = msg.params[0];
	const std::string &channelName = msg.params[1];

	std::map<int, Client>::iterator target = findClientByNickname(targetNick);
	// NICK reserves the name before USER completes registration. The sender
	// is checked by processMessage(), but the recipient must be registered too.
	if (target == _clients.end() || !target->second.isRegistered())
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 401 " + client.getNickname() + " " + IrcParameters::safeParameter(targetNick)
			+ " :No such nick/channel");
		return;
	}

	// RFC 2812 3.2.7: the channel need not exist. When it does, its rules
	// apply -- only its members may invite anyone into it, and only its
	// operators may do so while it is invite-only.
	std::map<std::string, Channel>::iterator channel =
		_channels.find(normalizeIrcName(channelName));
	std::string shownChannel = channelName;

	if (channel != _channels.end())
	{
		shownChannel = channel->second.getName();

		if (!channel->second.hasMember(client.getFd()))
		{
			queueMessage(client.getFd(), std::string(":") + SERVER_NAME
				+ " 442 " + client.getNickname() + " " + shownChannel
				+ " :You're not on that channel");
			return;
		}

		if (channel->second.hasMember(target->first))
		{
			queueMessage(client.getFd(), std::string(":") + SERVER_NAME
				+ " 443 " + client.getNickname() + " "
				+ target->second.getNickname() + " " + shownChannel
				+ " :is already on channel");
			return;
		}

		if (channel->second.hasMode('i') && !channel->second.isOperator(client.getFd()))
		{
			queueMessage(client.getFd(), std::string(":") + SERVER_NAME
				+ " 482 " + client.getNickname() + " " + shownChannel
				+ " :You're not channel operator");
			return;
		}
	}

	// Preserve the requested destination, even when it is not a valid channel.
	// A final parameter containing spaces or starting with ':' needs a colon
	// in the numeric reply too; ordinary channel replies keep their format.
	const std::string separator = shownChannel[0] == ':'
		|| shownChannel.find(' ') != std::string::npos ? " :" : " ";
	const std::string confirmation = std::string(":") + SERVER_NAME + " 341 "
		+ client.getNickname() + " " + target->second.getNickname()
		+ separator + shownChannel;
	const std::string invitation = ":" + client.getPrefix() + " INVITE "
		+ target->second.getNickname() + " :" + shownChannel;
	// A destination cannot be truncated. Check both messages before storing
	// an invitation or confirming success (RFC 2812, RPL_TRYAGAIN on a drop).
	if (confirmation.size() > IRC_MESSAGE_MAX_CONTENT
		|| invitation.size() > IRC_MESSAGE_MAX_CONTENT)
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME + " 263 "
			+ client.getNickname() + " INVITE :Invitation destination is too long");
		return;
	}
	if (channel != _channels.end())
		channel->second.addInvite(target->first);

	// Only these two hear about it: an invitation is not channel news.
	queueMessage(client.getFd(), confirmation);
	queueMessage(target->first, invitation);
}
