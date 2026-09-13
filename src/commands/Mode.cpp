/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Mode.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 23:30:55 by apestana          #+#    #+#             */
/*   Updated: 2026/09/13 23:30:59 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include <iostream>

static void handleChannelMode(Client &client, const IrcMessage &msg)
{
	client.getModes();
	std::cout << msg.command << std::endl;
	std::cout << "Changing channel mode." << std::endl;
}

static void handleUserMode(Client &client, const IrcMessage &msg)
{
	client.getModes();
	std::cout << msg.command << std::endl;
	std::cout << "Changing user mode." << std::endl;
}

void Server::handleMode(Client &client, const IrcMessage &msg)
{
	if (msg.params.empty())
		return;

	const std::string &target = msg.params[0];

	if (target[0] == '#')
		handleChannelMode(client, msg);
	else if (target == client.getNickname())
		handleUserMode(client, msg);
}
