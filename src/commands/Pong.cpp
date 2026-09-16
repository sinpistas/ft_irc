/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Pong.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/16 22:34:18 by apestana          #+#    #+#             */
/*   Updated: 2026/09/17 00:10:55 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "IrcCaseMapping.hpp"
#include "IrcParameters.hpp"

// PONG is already a reply: validate it without replying again, which would
// create a ping/pong loop. No server-initiated ping deadline is tracked yet.
void Server::handlePong(Client &client, const IrcMessage &msg)
{
	const std::string target = client.getNickname().empty() ? "*" : client.getNickname();
	const std::string prefix = std::string(":") + SERVER_NAME + " ";
	if (msg.params.empty() || msg.params[0].empty())
	{
		queueMessage(client.getFd(), prefix + "409 " + target + " :No origin specified");
		return;
	}
	if (msg.params.size() > 1 && !areSameNicknames(msg.params[1], SERVER_NAME))
	{
		queueMessage(client.getFd(), prefix + "402 " + target + " "
			+ IrcParameters::safeParameter(msg.params[1]) + " :No such server");
	}
}
