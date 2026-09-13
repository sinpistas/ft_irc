/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Quit.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 23:30:14 by apestana          #+#    #+#             */
/*   Updated: 2026/09/13 23:30:15 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

void Server::handleQuit(Client &client, const IrcMessage &msg)
{
	// QUIT takes one optional parameter, the message to leave behind. It
	// arrives as the trailing parameter, so it may hold spaces and there is
	// never more than one of it.
	const std::string reason = (msg.params.empty() || msg.params[0].empty())
		? "Client Quit" : msg.params[0];

	// Kept on the client because the announcement does not go out from here:
	// it goes out when the client is actually removed, at the end of the
	// pass, from the one place that knows who to tell.
	client.setQuitReason(reason);

	// RFC 2812 3.1.7: the server answers a QUIT with an ERROR message. This
	// is the last thing this client will be sent, and it does arrive: a
	// client on its way out is held on to until its queue has drained.
	queueMessage(client.getFd(), "ERROR :Closing Link: " + client.getHostname()
		+ " (Quit: " + reason + ")", true);

	// Everything that is left -- telling the channels, leaving them, closing
	// the socket -- is what removing a client does anyway, so it is done
	// through the single path that does it properly. Marking also stops any
	// further command this client may have sent from being executed.
	markForRemoval(client.getFd());
}
