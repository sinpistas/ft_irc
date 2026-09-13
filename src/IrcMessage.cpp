/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   IrcMessage.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/28 14:44:21 by apestana          #+#    #+#             */
/*   Updated: 2026/09/13 21:42:55 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "IrcMessage.hpp"
#include "IrcLimits.hpp"
#include <cctype>

// A valid IRC command is either letters only, or exactly three ASCII
// digits (the numeric reply codes, e.g. "001", "433"). Anything else,
// including mixed letters/digits or a digit run of the wrong length, is
// a syntax error.
static bool isValidCommand(const std::string &command)
{
	if (command.empty())
		return false;

	if (command.size() == 3
		&& std::isdigit(static_cast<unsigned char>(command[0]))
		&& std::isdigit(static_cast<unsigned char>(command[1]))
		&& std::isdigit(static_cast<unsigned char>(command[2])))
		return true;

	for (size_t i = 0; i < command.size(); ++i)
	{
		if (!std::isalpha(static_cast<unsigned char>(command[i])))
			return false;
	}
	return true;
}

bool IrcMessage::parse(const std::string &line, IrcMessage &out)
{
	out.prefix.clear();
	out.command.clear();
	out.params.clear();

	// Framing has already removed the final CRLF (or the accepted bare LF).
	// Reject the entire line, rather than deleting bytes and changing what
	// the sender asked to execute. Other IRC controls, e.g. CTCP, stay valid.
	if (line.size() > IRC_MESSAGE_MAX_CONTENT
		|| line.find_first_of("\0\r\n", 0, 3) != std::string::npos)
		return false;

	// Publish only a complete parse. Early returns and allocation failures
	// must not expose a partial prefix, command or parameter list in `out`.
	IrcMessage parsed;
	size_t pos = 0;
	size_t len = line.size();

	// Be lenient about stray leading spaces even though the grammar doesn't
	// strictly call for them.
	while (pos < len && line[pos] == ' ')
		++pos;
	if (pos >= len)
		return false;

	// Optional prefix: ":<prefix>" up to the next space.
	if (line[pos] == ':')
	{
		size_t start = ++pos;
		while (pos < len && line[pos] != ' ')
			++pos;
		if (pos == start)
			return false; // ":" with an empty prefix is not valid.
		parsed.prefix = line.substr(start, pos - start);

		while (pos < len && line[pos] == ' ')
			++pos;
		if (pos >= len)
			return false; // Prefix present but no command followed it.
	}

	// Command: mandatory token up to the next space.
	{
		size_t start = pos;
		while (pos < len && line[pos] != ' ')
			++pos;
		if (pos == start)
			return false;
		parsed.command = line.substr(start, pos - start);
	}

	if (!isValidCommand(parsed.command))
		return false;

	// Normalize so command dispatch can later do a plain string match.
	// std::toupper leaves digits untouched, so numeric commands pass through as-is.
	for (size_t i = 0; i < parsed.command.size(); ++i)
		parsed.command[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(parsed.command[i])));

	// Parameters: space-separated tokens, except the last one may start with
	// ':' to mean "everything after this colon, including spaces".
	while (pos < len)
	{
		while (pos < len && line[pos] == ' ')
			++pos;
		if (pos >= len)
			break;

		// RFC 2812 also permits the 15th parameter without a colon: it
		// consumes all remaining text, including spaces, as one parameter.
		if (line[pos] == ':' || parsed.params.size() == IRC_MESSAGE_MAX_PARAMS - 1)
		{
			if (line[pos] == ':')
				++pos;
			parsed.params.push_back(line.substr(pos));
			break;
		}

		size_t start = pos;
		while (pos < len && line[pos] != ' ')
			++pos;
		parsed.params.push_back(line.substr(start, pos - start));
	}

	out.prefix.swap(parsed.prefix);
	out.command.swap(parsed.command);
	out.params.swap(parsed.params);
	return true;
}
