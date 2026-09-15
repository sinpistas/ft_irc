/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Topic.cpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vbullock <vbullock@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 23:30:06 by apestana          #+#    #+#             */
/*   Updated: 2026/09/15 20:15:48 by vbullock         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "IrcCaseMapping.hpp"
#include "IrcParameters.hpp"
#include "IrcLimits.hpp"

void Server::handleTopic(Client &client, const IrcMessage &msg)
{
	if (msg.params.empty())
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 461 " + client.getNickname() + " TOPIC :Not enough parameters");
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

	if (msg.params.size() == 1)
	{
		// Send 331 if empty, otherwise 332 with the topic.
		if (channel.getTopic().empty())
			queueMessage(client.getFd(), std::string(":") + SERVER_NAME
				+ " 331 " + client.getNickname() + " " + channel.getName() + " :No topic is set");
		else
			queueMessage(client.getFd(), std::string(":") + SERVER_NAME
				+ " 332 " + client.getNickname() + " " + channel.getName()
				+ " :" + channel.getTopic());
		return;
	}

	if (channel.hasMode('t') && !channel.isOperator(client.getFd()))
	{
		// Send 482: channel operator privileges needed.
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
				+ " 482 " + client.getNickname() + " " + channel.getName() + " :You're not channel operator");
		return;
	}

	const std::string topicPrefix = ":" + client.getPrefix()
		+ " TOPIC " + channel.getName() + " :";
	// Store a topic that fits both this announcement and every later 332,
	// including JOIN replies and clients with the longest allowed nickname.
	const size_t replyPrefixSize = (std::string(":") + SERVER_NAME + " 332 ").size()
		+ IRC_NICKNAME_MAX_LENGTH + 1 + channel.getName().size() + 2;
	const size_t prefixSize = topicPrefix.size() > replyPrefixSize
		? topicPrefix.size() : replyPrefixSize;
	const size_t topicSpace = prefixSize < IRC_MESSAGE_MAX_CONTENT
		? IRC_MESSAGE_MAX_CONTENT - prefixSize : 0;
	const std::string topic = msg.params[1].substr(0, topicSpace);
	// Construct the announcement before committing the new state, so an
	// allocation failure here leaves the previous topic intact.
	const std::string notification = topicPrefix + topic;
	channel.setTopic(topic);

	// Broadcast the exact stored topic to every channel member.
	const std::set<int> &members = channel.getMembers();
	for (std::set<int>::const_iterator member = members.begin();
		member != members.end(); ++member)
		queueMessage(*member, notification);
}
