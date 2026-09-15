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
#include <iostream>

void Server::handleTopic(Client &client, const IrcMessage &msg)
{
	if (msg.params.empty())
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 461 " + client.getNickname() + " INVITE :Not enough parameters");
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
		if(msg.params[0] == "")
			std::cout << "Empty parameter. Invalid topic name." << std::endl;
		if(msg.params[0][0] == '#')
		{
			if (channel.getTopic().empty())
				queueMessage(client.getFd(), std::string(":") + SERVER_NAME
						+ " 331 " + client.getNickname() + " " + channel.getName() + " :No topic is set");
			else
			{
				const std::string topicPrefix = std::string(":") + SERVER_NAME + " 332 " + client.getNickname() + " " + channel.getName() + " :";
				const size_t topicSpace = topicPrefix.size() < 510 ? 510 - topicPrefix.size() : 0;
				queueMessage(client.getFd(), topicPrefix
					+ channel.getTopic().substr(0, topicSpace));
			}
		}
		return;
	}

	if (channel.hasMode('t') && !channel.isOperator(client.getFd()))
	{
		// Send 482: channel operator privileges needed.
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
				+ " 482 " + client.getNickname() + " " + channel.getName() + " :You're not channel operator");
		return;
	}

	channel.setTopic(msg.params[1]);
	// Broadcast TOPIC to every channel member.
	
	const std::set<int> &members = channel.getMembers();
	const std::string notification = ":" + client.getPrefix()
    	+ " TOPIC " + channel.getName() + " :" + channel.getTopic();

	for (std::set<int>::const_iterator member = members.begin();
		    member != members.end(); ++member)
	    queueMessage(*member, notification, true);

}
