/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Ping.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/16 22:34:26 by apestana          #+#    #+#             */
/*   Updated: 2026/09/17 00:10:56 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "IrcCaseMapping.hpp"
#include "IrcParameters.hpp"
#include "IrcLimits.hpp"

// Echo the origin token exactly: HexChat uses it to recognize its own ping
// and measure the delay. PING is also accepted before registration completes.
void Server::handlePing(Client &client, const IrcMessage &msg)
{
	const std::string target = client.getNickname().empty() ? "*" : client.getNickname();
	const std::string prefix = std::string(":") + SERVER_NAME + " ";
	if (msg.params.empty() || msg.params[0].empty())
	{
		queueMessage(client.getFd(), prefix + "409 " + target + " :No origin specified");
		return;
	}
	// This server has no links to other servers, so it cannot forward a ping.
	if (msg.params.size() > 1 && !areSameNicknames(msg.params[1], SERVER_NAME))
	{
		queueMessage(client.getFd(), prefix + "402 " + target + " "
			+ IrcParameters::safeParameter(msg.params[1]) + " :No such server");
		return;
	}
	const std::string reply = prefix + "PONG " + SERVER_NAME + " :" + msg.params[0];
	// Tokens are identifiers, not free text: never silently truncate them.
	if (reply.size() > IRC_MESSAGE_MAX_CONTENT)
	{
		queueMessage(client.getFd(), prefix + "263 " + target
			+ " PING :Origin is too long for a PONG reply");
		return;
	}
	queueMessage(client.getFd(), reply);
}
