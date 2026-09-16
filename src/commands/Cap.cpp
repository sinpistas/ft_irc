/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Cap.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/17 00:10:47 by apestana          #+#    #+#             */
/*   Updated: 2026/09/17 00:10:49 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "IrcLimits.hpp"
#include "IrcParameters.hpp"

// Basic capability negotiation, with no optional extensions available.
// Accept CAP LS 302 as well: the empty list needs no version-specific format.
void Server::handleCap(Client &client, const IrcMessage &msg)
{
	const std::string target = client.getNickname().empty() ? "*" : client.getNickname();
	const std::string prefix = std::string(":") + SERVER_NAME;
	std::string subcommand = msg.params.empty() ? "" : msg.params[0];
	for (size_t i = 0; i < subcommand.size(); ++i)
		if (subcommand[i] >= 'a' && subcommand[i] <= 'z')
			subcommand[i] = static_cast<char>(subcommand[i] - 'a' + 'A');

	const bool list = subcommand == "LIST" && msg.params.size() == 1;
	const bool end = subcommand == "END" && msg.params.size() == 1;
	const bool request = subcommand == "REQ" && msg.params.size() == 2
		&& msg.params[1].find_first_not_of(' ') != std::string::npos;
	const bool ls = subcommand == "LS" && (msg.params.size() == 1
		|| (msg.params.size() == 2 && !msg.params[1].empty()
			&& msg.params[1].find_first_not_of("0123456789") == std::string::npos));
	if (!list && !end && !request && !ls)
	{
		queueMessage(client.getFd(), prefix + " 410 " + target + " "
			+ IrcParameters::safeParameter(subcommand) + " :Invalid CAP command");
		return;
	}

	// Only LS/REQ start negotiation. LIST must not delay registration, and
	// CAP issued after registration must not undo an existing connection.
	if ((ls || request) && !client.isRegistered())
		client.setCapNegotiating(true);
	if (ls || list)
		queueMessage(client.getFd(), prefix + " CAP " + target + " " + subcommand + " :");
	else if (request)
	{
		// Reject the entire request: advertising/ACKing unimplemented features
		// would make clients expect different message formats or authentication.
		const std::string reply = prefix + " CAP " + target + " NAK :" + msg.params[1];
		if (reply.size() > IRC_MESSAGE_MAX_CONTENT)
			queueMessage(client.getFd(), prefix + " 263 " + target
				+ " CAP :Capability request is too long for a reply");
		else
			queueMessage(client.getFd(), reply);
	}
	else if (!client.isRegistered())
	{
		// END itself has no reply. Welcome is sent once, only if all the
		// normal registration requirements have already been satisfied.
		client.setCapNegotiating(false);
		if (client.tryRegister())
			sendWelcome(client);
	}
}
