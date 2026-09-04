/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   IrcMessage.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/28 14:44:21 by apestana          #+#    #+#             */
/*   Updated: 2026/08/28 15:20:07 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "IrcMessage.hpp"
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
		out.prefix = line.substr(start, pos - start);

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
		out.command = line.substr(start, pos - start);
	}

	if (!isValidCommand(out.command))
	{
		// The prefix, if any, was already captured above; the contract
		// requires `out` to stay empty on failure.
		out.prefix.clear();
		out.command.clear();
		return false;
	}

	// Normalize so command dispatch can later do a plain string match.
	// std::toupper leaves digits untouched, so numeric commands pass through as-is.
	for (size_t i = 0; i < out.command.size(); ++i)
		out.command[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(out.command[i])));

	// Parameters: space-separated tokens, except the last one may start with
	// ':' to mean "everything after this colon, including spaces".
	while (pos < len)
	{
		while (pos < len && line[pos] == ' ')
			++pos;
		if (pos >= len)
			break;

		if (line[pos] == ':')
		{
			out.params.push_back(line.substr(pos + 1));
			break;
		}

		size_t start = pos;
		while (pos < len && line[pos] != ' ')
			++pos;
		out.params.push_back(line.substr(start, pos - start));
	}

	return true;
}
