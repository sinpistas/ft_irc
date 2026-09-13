/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Pass.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 23:30:27 by apestana          #+#    #+#             */
/*   Updated: 2026/09/13 23:30:29 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

void Server::handlePass(Client &client, const IrcMessage &msg)
{
	// Unregistered clients do not yet have a nickname, so IRC errors use '*'.
	std::string target;

	if (client.getNickname().empty())
		target = "*";
	else
		target = client.getNickname();

	if (client.isRegistered())
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 462 " + target + " :You may not reregister");
		return;
	}

	if (msg.params.size() != 1)
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 461 " + target + " PASS :Not enough parameters");
		return;
	}

	if (msg.params[0] != _password)
	{
		client.setPasswordAccepted(false);
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 464 " + target + " :Password incorrect");
		// Without the password there is nothing this connection can go on to
		// do, so it is told why and shown the door instead of being left to
		// sit there for as long as it likes.
		queueMessage(client.getFd(), "ERROR :Closing Link: "
			+ client.getHostname() + " (Bad password)");
		markForRemoval(client.getFd());
		return;
	}

	client.setPasswordAccepted(true);
}
