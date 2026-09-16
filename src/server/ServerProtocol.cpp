/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerProtocol.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 23:28:00 by apestana          #+#    #+#             */
/*   Updated: 2026/09/13 23:28:07 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "IrcCaseMapping.hpp"
#include "IrcLimits.hpp"
#include "IrcParameters.hpp"
#include <iostream>
#include <new>

// Bound the bytes queued for a client that stops reading.
static const size_t MAX_OUTPUT_QUEUE = 64 * 1024;

void Server::processMessage(Client &client, const IrcMessage &msg)
{
	// A client-supplied prefix may only be this connection's registered
	// nickname (RFC 2812 2.3). Discard the whole command if it is not;
	// ignoring just the prefix would still execute an invalid request.
	if (!msg.prefix.empty() && (!client.isRegistered()
		|| !areSameNicknames(msg.prefix, client.getNickname())))
		return;

	// Registration commands are the only commands accepted before welcome (001).
	// QUIT is allowed before registration too: a client that gives up half
	// way through should be able to say so and leave, not be told that it
	// has not registered.
	if (!client.isRegistered() && msg.command != "PASS" && msg.command != "NICK"
		&& msg.command != "USER" && msg.command != "QUIT")
	{
		std::string target;

		if (client.getNickname().empty())
			target = "*";
		else
			target = client.getNickname();
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 451 " + target + " :You have not registered");
		return;
	}
	if (msg.command == "PASS")
		handlePass(client, msg);
	else if (msg.command == "NICK")
		handleNick(client, msg);
	else if (msg.command == "USER")
		handleUser(client, msg);
	else if (msg.command == "JOIN")
		handleJoin(client, msg);
	else if (msg.command == "PART")
		handlePart(client, msg);
	else if (msg.command == "INVITE")
		handleInvite(client, msg);
	else if (msg.command == "PRIVMSG")
		handlePrivmsg(client, msg);
	else if (msg.command == "QUIT")
		handleQuit(client, msg);
	else if (msg.command == "KICK")
		handleKick(client, msg);
	else if (msg.command == "MODE")
		handleMode(client, msg);
	else if (msg.command == "TOPIC")
		handleTopic(client, msg);
	else
	{
		// Saying nothing is the worst possible answer: the client cannot
		// tell "the server does not know this command" from "it ran and
		// did nothing". Bound the echoed command too: even a syntactically
		// valid token can be too long to fit in a numeric reply.
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 421 " + client.getNickname() + " " + IrcParameters::safeParameter(msg.command)
			+ " :Unknown command");
	}
}

void Server::extractCompleteLines(int fd)
{
	std::map<int, Client>::iterator it = _clients.find(fd);
	if (it == _clients.end())
		return;

	// A handler may mark this client for removal. Nothing is erased before
	// the end of the poll() pass, so `it` stays valid, but the rest of the
	// lines it had buffered must not be executed on a connection that is
	// already closing.
	std::string line;
	while (!isMarkedForRemoval(fd) && it->second.extractLine(line))
	{
		// A syntax error only discards this one line, the client stays connected.
		IrcMessage msg;
		if (!IrcMessage::parse(line, msg))
		{
			logEvent("WARN", "INVALID COMMAND", fd, "Malformed line discarded");
			continue;
		}
		// Log only the command name: parameters may contain passwords,
		// channel keys, private messages or other personal data.
		logEvent("CMD", "COMMAND", fd, msg.command.c_str());
		processMessage(it->second, msg);
	}
}

void Server::queueMessage(int fd, const std::string &message, bool truncateText)
{
	std::map<int, Client>::iterator it = _clients.find(fd);
	if (it == _clients.end() || isMarkedForRemoval(fd))
		return;

	try
	{
		// Strip the terminator a caller may already have added, so the message
		// is neither measured nor terminated twice.
		std::string line = message;
		if (line.size() >= 2 && line[line.size() - 2] == '\r'
			&& line[line.size() - 1] == '\n')
			line.erase(line.size() - 2);

		// Preserve prefix, command and targets. A trailing parameter is not
		// necessarily free text (JOIN and NICK use it for identities), so
		// shortening requires explicit permission from the caller as well.
		if (line.size() > IRC_MESSAGE_MAX_CONTENT)
		{
			const std::string::size_type trailing = line.find(" :");
			if (!truncateText || trailing == std::string::npos
				|| trailing + 2 >= IRC_MESSAGE_MAX_CONTENT)
			{
				logEvent("WARN", "REPLY DISCARDED", fd, "IRC message exceeds length limit");
				return;
			}
			line.erase(IRC_MESSAGE_MAX_CONTENT);
		}

		// And it is also the one place that can tell when a client has stopped
		// taking what it is sent. The queue only grows when the socket refuses
		// more, so a client that never reads would otherwise make the server
		// hold on to an unbounded amount of memory on its behalf.
		if (it->second.getSendBuffer().size() + line.size() + 2 > MAX_OUTPUT_QUEUE)
		{
			if (!isMarkedForRemoval(fd))
			{
				logEvent("WARN", "OUTPUT QUEUE FULL", fd, "Closing slow reader");
				// Its channels are told why it vanished; the client itself is
				// past being told anything, since it is not reading.
				it->second.setQuitReason("Output queue exceeded");
				markForRemoval(fd);
			}
			return;
		}

		it->second.appendToSendBuffer(line + "\r\n");
		updateClientPollEvents(fd);
	}
	catch (const std::bad_alloc &)
	{
		// The recipient's queue failed, which need not be the client
		// currently issuing the command (e.g. a channel broadcast).
		handleMemoryFailure(fd);
	}
}

void Server::sendWelcome(const Client &client)
{
	logEvent("INFO", "REGISTERED", client.getFd());
	queueMessage(client.getFd(), std::string(":") + SERVER_NAME + " 001 "
		+ client.getNickname() + " :Welcome to the ft_irc server " + client.getPrefix());
	// There is no MOTD configured. This finishes the login sequence for IRC
	// clients; registration has already succeeded and the connection stays open.
	queueMessage(client.getFd(), std::string(":") + SERVER_NAME + " 422 "
		+ client.getNickname() + " :MOTD File is missing");
}
