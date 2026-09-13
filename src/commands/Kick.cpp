/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Kick.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 23:31:05 by apestana          #+#    #+#             */
/*   Updated: 2026/09/13 23:31:07 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include <iostream>

void Server::handleKick(Client &client, const IrcMessage &msg)
{
	if (msg.params.size() != 1)
	{
		queueMessage(client.getFd(),
		"Only one parameter permitted for kick");
	}
	client.leaveChannel(msg.params[0]);
	std::cout << msg.command << std::endl;
	std::cout << "Kicking user " << client.getNickname() << std::endl;
}
