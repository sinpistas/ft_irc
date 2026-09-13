/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Nick.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 23:30:42 by apestana          #+#    #+#             */
/*   Updated: 2026/09/13 23:30:44 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "IrcLimits.hpp"
#include "IrcCaseMapping.hpp"
#include "IrcParameters.hpp"
#include <cctype>

static bool isNicknameSpecial(char character)
{
	return character == '[' || character == ']' || character == '\\'
		|| character == '^' || character == '_' || character == '`'
		|| character == '{' || character == '}' || character == '|';
}

bool Server::isValidNickname(const std::string &nickname) const
{
	// RFC 2812 caps a nickname at nine characters. Without a cap, a long
	// enough nickname alone would push the messages announcing it past the
	// size an IRC message is allowed to have.
	if (nickname.empty() || nickname.size() > IRC_NICKNAME_MAX_LENGTH)
		return false;

	for (std::string::size_type i = 0; i < nickname.size(); ++i)
	{
		unsigned char character = static_cast<unsigned char>(nickname[i]);
		if (std::isalpha(character) || isNicknameSpecial(nickname[i]))
			continue;
		if (i > 0 && (std::isdigit(character) || nickname[i] == '-'))
			continue;
		return false;
	}
	return true;
}

bool Server::isNicknameInUse(const std::string &nickname, int ignoredFd) const
{
	for (std::map<int, Client>::const_iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (it->first != ignoredFd && areSameNicknames(it->second.getNickname(), nickname))
			return true;
	}
	return false;
}

void Server::handleNick(Client &client, const IrcMessage &msg)
{
	std::string target = client.getNickname().empty() ? "*" : client.getNickname();

	if (!client.hasAcceptedPassword())
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 464 " + target + " :Password incorrect");
		return;
	}

	// "NICK" with nothing after it, and "NICK :" with an empty parameter, are
	// the same thing to the user: no nickname was given.
	if (msg.params.empty() || msg.params[0].empty())
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 431 " + target + " :No nickname given");
		return;
	}

	const std::string &nickname = msg.params[0];
	if (msg.params.size() != 1 || !isValidNickname(nickname))
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 432 " + target + " " + IrcParameters::safeParameter(nickname) + " :Erroneous nickname");
		return;
	}

	if (isNicknameInUse(nickname, client.getFd()))
	{
		queueMessage(client.getFd(), std::string(":") + SERVER_NAME
			+ " 433 " + target + " " + IrcParameters::safeParameter(nickname) + " :Nickname is already in use");
		return;
	}

	if (client.isRegistered())
	{
		if (client.getNickname() == nickname)
			return;

		// The prefix carries the name the client had until now: that is how
		// its peers know which of them is the one renaming itself.
		const std::string notification = ":" + client.getPrefix() + " NICK :" + nickname;
		client.setNickname(nickname);
		queueMessage(client.getFd(), notification);

		// Notify each registered peer once, even when several channels are shared.
		const std::set<std::string> &channels = client.getChannels();
		for (std::map<int, Client>::const_iterator it = _clients.begin(); it != _clients.end(); ++it)
		{
			if (it->first == client.getFd() || !it->second.isRegistered())
				continue;
			for (std::set<std::string>::const_iterator channel = channels.begin(); channel != channels.end(); ++channel)
			{
				if (it->second.isInChannel(*channel))
				{
					queueMessage(it->first, notification);
					break;
				}
			}
		}
		return;
	}

	client.setNickname(nickname);
	// NICK and USER may arrive in either order after PASS.
	if (client.tryRegister())
	{
		sendWelcome(client);
	}
}
