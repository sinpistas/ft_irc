/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Kick.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vbullock <vbullock@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 23:31:05 by apestana          #+#    #+#             */
/*   Updated: 2026/09/14 14:51:33 by vbullock         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include <iostream>

void Server::handleKick(Client &client, const IrcMessage &msg)
{
	if (msg.params.size() != 2)
	{
		queueMessage(client.getFd(),
		"Only two parameters permitted for kick");
		return ;
	}
	client.leaveChannel(msg.params[0]);
	std::cout << msg.command << std::endl;
	std::cout << "Kicking user " << client.getNickname() << std::endl;
}
