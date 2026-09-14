/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Kick.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 23:31:05 by apestana          #+#    #+#             */
/*   Updated: 2026/09/14 23:23:54 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "IrcCaseMapping.hpp"
#include "IrcParameters.hpp"
#include <new>

void Server::handleKick(Client &client, const IrcMessage &msg)
{
	if (msg.params.size() < 2 || msg.params[0].empty() || msg.params[1].empty())
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 461 " + client.getNickname() + " KICK :Not enough parameters");
		return;
	}

	const std::vector<std::string> channels = IrcParameters::splitOnCommas(msg.params[0]);
	const std::vector<std::string> users = IrcParameters::splitOnCommas(msg.params[1]);
	// RFC 2812: one channel for all users, or one channel per user.
	// Reject mismatched lists before performing any expulsions.
	if (channels.empty() || users.empty()
		|| (channels.size() != 1 && channels.size() != users.size()))
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 461 " + client.getNickname() + " KICK :Invalid channel/user list");
		return;
	}
	const std::string reason = msg.params.size() > 2
		? msg.params[2] : client.getNickname();
	for (size_t i = 0; i < users.size() && !isMarkedForRemoval(client.getFd()); ++i)
		kickFromChannel(client, channels[channels.size() == 1 ? 0 : i], users[i], reason);
}

void Server::kickFromChannel(Client &client, const std::string &channelName,
	const std::string &nickname, const std::string &reason)
{
	const std::string replyPrefix = std::string(":") + SERVER_NAME + " ";
	std::map<std::string, Channel>::iterator channel =
		_channels.find(normalizeIrcName(channelName));
	if (channel == _channels.end())
	{
		queueMessage(client.getFd(), replyPrefix + "403 " + client.getNickname()
			+ " " + IrcParameters::safeParameter(channelName) + " :No such channel");
		return;
	}
	const std::string &shownChannel = channel->second.getName();
	if (!channel->second.hasMember(client.getFd()))
	{
		queueMessage(client.getFd(), replyPrefix + "442 " + client.getNickname()
			+ " " + shownChannel + " :You're not on that channel");
		return;
	}
	if (!channel->second.isOperator(client.getFd()))
	{
		queueMessage(client.getFd(), replyPrefix + "482 " + client.getNickname()
			+ " " + shownChannel + " :You're not channel operator");
		return;
	}
	std::map<int, Client>::iterator target = findClientByNickname(nickname);
	if (target == _clients.end() || !target->second.isRegistered())
	{
		queueMessage(client.getFd(), replyPrefix + "401 " + client.getNickname()
			+ " " + IrcParameters::safeParameter(nickname) + " :No such nick/channel");
		return;
	}
	if (!channel->second.hasMember(target->first))
	{
		queueMessage(client.getFd(), replyPrefix + "441 " + client.getNickname()
			+ " " + target->second.getNickname() + " " + shownChannel
			+ " :They aren't on that channel");
		return;
	}

	// Each notification names one channel and one user. Broadcast before
	// removal so the expelled user receives it too, even for a self-KICK.
	const std::string notification = ":" + client.getPrefix() + " KICK "
		+ shownChannel + " " + target->second.getNickname() + " :" + reason;
	const std::set<int> &members = channel->second.getMembers();
	for (std::set<int>::const_iterator member = members.begin(); member != members.end(); ++member)
		queueMessage(*member, notification, true);
	try
	{
		// Updates both indexes and operator privileges; may erase the channel.
		removeFromChannel(target->second, shownChannel);
	}
	catch (const std::bad_alloc &)
	{
		// Peers have already been notified. If removal cannot allocate its
		// name copies, disconnect the target through allocation-free cleanup.
		handleMemoryFailure(target->first);
	}
}
