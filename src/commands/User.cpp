/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   User.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 23:29:58 by apestana          #+#    #+#             */
/*   Updated: 2026/09/13 23:30:00 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "IrcLimits.hpp"

static bool isValidUsername(const std::string &username)
{
	if (username.empty())
		return false;
	// RFC 2812 2.3.1 <user>. Validate the whole input before shortening it,
	// so an invalid byte beyond the stored portion is not hidden.
	for (std::string::size_type i = 0; i < username.size(); ++i)
	{
		if (username[i] == '\0' || username[i] == '\r' || username[i] == '\n'
			|| username[i] == ' ' || username[i] == '@')
			return false;
	}
	return true;
}

void Server::handleUser(Client &client, const IrcMessage &msg)
{
	std::string target;

	if (client.getNickname().empty())
		target = "*";
	else
		target = client.getNickname();

	if (!client.hasAcceptedPassword())
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 464 " + target + " :Password incorrect");
		return;
	}

	if (client.isRegistered())
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 462 " + target + " :You may not reregister");
		return;
	}

	if (msg.params.size() != 4)
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 461 " + target + " USER :Not enough parameters");
		return;
	}

	if (!isValidUsername(msg.params[0]))
	{
		queueMessage(client.getFd(), "ERROR :Closing Link: "
			+ client.getHostname() + " (Invalid username)");
		markForRemoval(client.getFd());
		return;
	}

	client.setUsername(msg.params[0].substr(0, IRC_USERNAME_MAX_LENGTH));
	client.setRealname(msg.params[3]);

	// NICK and USER may arrive in either order after PASS.
	if (client.tryRegister())
	{
		sendWelcome(client);
	}
}
