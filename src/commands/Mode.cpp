/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Mode.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 23:30:55 by apestana          #+#    #+#             */
/*   Updated: 2026/09/16 01:07:04 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "IrcCaseMapping.hpp"
#include "IrcParameters.hpp"
#include "IrcLimits.hpp"
#include <cerrno>
#include <climits>
#include <cstdlib>

// Parse +l as a positive decimal int. Reject signs, whitespace and overflow;
// leave the output argument unchanged when validation fails.
static bool parseLimit(const std::string &value, int &limit)
{
	if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
		return false;
	char *end = 0;
	errno = 0;
	long parsed = std::strtol(value.c_str(), &end, 10);
	if (errno == ERANGE || *end != '\0' || parsed <= 0 || parsed > INT_MAX)
		return false;
	limit = static_cast<int>(parsed);
	return true;
}

// Serialize a nonnegative limit in decimal, so inputs such as "0003" are
// announced as "3". String allocation failures propagate to the caller.
static std::string limitText(int limit)
{
	std::string text;
	do
	{
		text.insert(text.begin(), static_cast<char>('0' + limit % 10));
		limit /= 10;
	} while (limit != 0);
	return text;
}

// Validate the key's byte length and character ranges using the original
// RFC 2812 2.3.1 ABNF (erratum 4836, option 1). BEL is allowed by this grammar.
// Commas are additionally excluded locally: JOIN uses them to split keys.
static bool isValidChannelKey(const std::string &key)
{
	if (key.empty() || key.size() > 23)
		return false;
	for (std::string::size_type i = 0; i < key.size(); ++i)
	{
		const unsigned char c = static_cast<unsigned char>(key[i]);
		if (!((c >= 1 && c <= 5) || c == 7 || c == 8 || c == 12
			|| (c >= 14 && c <= 31) || (c >= 33 && c <= 127)) || c == ',')
			return false;
	}
	return true;
}

// Build the channel, flags and values used after the recipient in numeric 324.
// Values follow their flags' order; outsiders see flags without key/limit values.
// Operator status is per member, so it does not appear in this channel summary.
static std::string channelModeReply(const Channel &channel, bool member)
{
	std::string modes = "+";
	std::string parameters;
	if (channel.hasMode('i'))
		modes += 'i';
	if (channel.hasMode('t'))
		modes += 't';
	if (channel.hasMode('l'))
	{
		modes += 'l';
		if (member)
			parameters += " " + limitText(channel.getLimit());
	}
	// Keep the key last: a key beginning with ':' needs a trailing parameter.
	if (channel.hasMode('k'))
	{
		modes += 'k';
		if (member)
		{
			const std::string &key = channel.getChannelKey();
			parameters += (key.empty() || key[0] == ':' ? " :" : " ") + key;
		}
	}
	return channel.getName() + " " + modes + parameters;
}

// Finish the pending group of changes and reset its accumulators. This only
// stores a complete announcement; nothing is queued to clients here.
static void flushModeChanges(std::vector<std::string> &messages,
	const std::string &prefix, std::string &modes, std::string &parameters)
{
	if (!modes.empty())
		messages.push_back(prefix + modes + parameters);
	modes.clear();
	parameters.clear();
}

// Add one validated change to the pending announcement. Keep mode arguments
// in order and start a new message when needed to respect the IRC size limit.
// Each individual change fits because keys, nicknames and limits are bounded.
static void appendModeChange(std::vector<std::string> &messages,
	const std::string &prefix, std::string &modes, std::string &parameters,
	bool adding, char mode, const std::string &parameter)
{
	const char sign = adding ? '+' : '-';
	// A trailing parameter must end its own message, even if more modes follow.
	if (!parameter.empty() && parameter[0] == ':')
	{
		flushModeChanges(messages, prefix, modes, parameters);
		messages.push_back(prefix + sign + mode + " :" + parameter);
		return;
	}
	std::string nextModes = modes;
	// Consecutive changes share a sign: +i followed by +t is written as +it.
	const std::string::size_type lastSign = modes.find_last_of("+-");
	if (lastSign == std::string::npos || modes[lastSign] != sign)
		nextModes += sign;
	nextModes += mode;
	const std::string argument = parameter.empty() ? "" : " " + parameter;
	// Include the sender prefix and all arguments in the size calculation.
	// After a flush, repeat the sign so the next message stands on its own.
	if (prefix.size() + nextModes.size() + parameters.size() + argument.size()
		> IRC_MESSAGE_MAX_CONTENT)
	{
		flushModeChanges(messages, prefix, modes, parameters);
		nextModes = sign;
		nextModes += mode;
	}
	modes = nextModes;
	parameters += argument;
}

// Handle channel mode queries and changes to i, t, k, o and l. Registration
// is checked by the command dispatcher before this handler is called.
void Server::handleMode(Client &client, const IrcMessage &msg)
{
	const std::string replyPrefix = std::string(":") + SERVER_NAME + " ";
	const std::string nick = client.getNickname();
	if (msg.params.empty() || msg.params[0].empty()
		|| (msg.params.size() > 1 && msg.params[1].empty()))
	{
		queueMessage(client.getFd(), replyPrefix + "461 " + nick
			+ " MODE :Not enough parameters");
		return;
	}

	std::map<std::string, Channel>::iterator it =
		_channels.find(normalizeIrcName(msg.params[0]));
	if (it == _channels.end())
	{
		queueMessage(client.getFd(), replyPrefix + "403 " + nick + " "
			+ IrcParameters::safeParameter(msg.params[0]) + " :No such channel");
		return;
	}
	Channel &channel = it->second;
	// No mode block, or a block containing only signs, requests the current state.
	if (msg.params.size() == 1 || (msg.params.size() == 2
		&& msg.params[1].find_first_not_of("+-") == std::string::npos))
	{
		// Querying needs no operator privileges. Key and limit values are
		// visible only to members (RFC 2811 4.2.9 and 4.2.10).
		queueMessage(client.getFd(), replyPrefix + "324 " + nick + " "
			+ channelModeReply(channel, channel.hasMember(client.getFd())));
		return;
	}
	// Only changes require membership and operator privileges. Queries above
	// have already returned, including those made by ordinary members.
	if (!channel.hasMember(client.getFd()))
	{
		queueMessage(client.getFd(), replyPrefix + "442 " + nick + " "
			+ channel.getName() + " :You're not on that channel");
		return;
	}
	if (!channel.isOperator(client.getFd()))
	{
		queueMessage(client.getFd(), replyPrefix + "482 " + nick + " "
			+ channel.getName() + " :You're not channel operator");
		return;
	}

	// Prepare both state and complete, bounded announcements before committing.
	// A failed allocation must not leave surviving members with stale modes.
	// Work on a copy: if preparation throws, the real channel stays unchanged.
	Channel prepared(channel);
	std::vector<std::string> messages;
	const std::string prefix = ":" + client.getPrefix() + " MODE " + channel.getName() + " ";
	std::string appliedModes;
	std::string appliedParameters;
	bool adding = true;
	size_t parameter = 1;
	size_t parameterChanges = 0;
	bool limitReported = false;
	// One cursor walks both mode blocks and their arguments. For example,
	// "+o bob -t" consumes bob inside the first block, then processes -t.
	// An invalid change is skipped without discarding later valid changes.
	while (parameter < msg.params.size())
	{
		const std::string &block = msg.params[parameter++];
		for (size_t i = 0; i < block.size(); ++i)
		{
			const char mode = block[i];
			if (mode == '+' || mode == '-')
			{
				adding = mode == '+';
				continue;
			}
			if (mode != 'i' && mode != 't' && mode != 'k' && mode != 'o' && mode != 'l')
			{
				queueMessage(client.getFd(), replyPrefix + "472 " + nick + " "
					+ IrcParameters::safeParameter(std::string(1, mode))
					+ " :is unknown mode char to me");
				continue;
			}

			std::string modeParameter;
			// Both signs of k/o take an argument; l needs one only when enabled.
			const bool needsParameter = mode == 'k' || mode == 'o' || (mode == 'l' && adding);
			if (needsParameter)
			{
				if (parameter >= msg.params.size())
				{
					queueMessage(client.getFd(), replyPrefix + "461 " + nick
						+ " MODE :Not enough parameters");
					continue;
				}
				modeParameter = msg.params[parameter++];
				// Count successful changes with arguments across the whole command.
				// Consume excess arguments too, so they cannot become mode blocks;
				// later changes without arguments can still be processed.
				if (parameterChanges == 3)
				{
					if (!limitReported)
						queueMessage(client.getFd(), replyPrefix + "263 " + nick
							+ " MODE :At most three parameterized mode changes per command");
					limitReported = true;
					continue;
				}
			}

			if (mode == 'l' && adding)
			{
				int limit = 0;
				if (!parseLimit(modeParameter, limit))
				{
					queueMessage(client.getFd(), replyPrefix + "461 " + nick
						+ " MODE :Invalid channel limit");
					continue;
				}
				prepared.setLimit(limit);
				modeParameter = limitText(limit);
			}
			else if (mode == 'l')
				prepared.setLimit(0);
			else if (mode == 'k')
			{
				if (!isValidChannelKey(modeParameter))
				{
					queueMessage(client.getFd(), replyPrefix + "461 " + nick
						+ " MODE :Invalid channel key");
					continue;
				}
				// Replace an existing key explicitly with -k followed by +k.
				if (adding && prepared.hasMode('k'))
				{
					queueMessage(client.getFd(), replyPrefix + "467 " + nick + " "
						+ channel.getName() + " :Channel key already set");
					continue;
				}
				prepared.setChannelKey(adding ? modeParameter : "");
			}
			else if (mode == 'o')
			{
				std::map<int, Client>::iterator target = findClientByNickname(modeParameter);
				if (target == _clients.end() || !target->second.isRegistered()
					|| isMarkedForRemoval(target->first))
				{
					queueMessage(client.getFd(), replyPrefix + "401 " + nick + " "
						+ IrcParameters::safeParameter(modeParameter) + " :No such nick/channel");
					continue;
				}
				// Lookup accepts IRC case equivalents; announce the stored nickname.
				modeParameter = target->second.getNickname();
				if (!prepared.hasMember(target->first))
				{
					queueMessage(client.getFd(), replyPrefix + "441 " + nick + " "
						+ modeParameter + " " + channel.getName() + " :They aren't on that channel");
					continue;
				}
				if (adding)
					prepared.addOperator(target->first);
				else
					prepared.removeOperator(target->first);
			}

			// 'o' belongs to a member, not to the channel's flag string.
			if (mode != 'o')
			{
				if (adding)
					prepared.addMode(mode);
				else
					prepared.removeMode(mode);
			}
			if (needsParameter)
				++parameterChanges;
			appendModeChange(messages, prefix, appliedModes, appliedParameters,
				adding, mode, modeParameter);
		}
	}
	flushModeChanges(messages, prefix, appliedModes, appliedParameters);
	// All announcements now exist. Commit only mode-related state without
	// allocating; membership, invitations and the topic stay on the real channel.
	channel.swapModeState(prepared);
	// Send the same ordered changes to every member, including the issuer.
	// queueMessage handles allocation failures in each recipient's output queue.
	for (size_t i = 0; i < messages.size(); ++i)
		for (std::set<int>::const_iterator member = channel.getMembers().begin();
			member != channel.getMembers().end(); ++member)
			queueMessage(*member, messages[i]);
}
