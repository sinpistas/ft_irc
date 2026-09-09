/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   IrcMessage.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vbullock <vbullock@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/28 14:44:29 by apestana          #+#    #+#             */
/*   Updated: 2026/09/09 14:45:58 by vbullock         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef IRCMESSAGE_HPP
#define IRCMESSAGE_HPP

#include <string>
#include <vector>

// A syntactically parsed IRC line: optional prefix, mandatory command and
// an ordered list of parameters. Carries no protocol semantics: it does not
// know which commands exist or what their parameters mean.
struct IrcMessage
{
	std::string              prefix;
	std::string              command;
	std::vector<std::string> params;

	// Parse a single line (without the trailing "\r\n") into `out`.
	// Returns false for syntactically invalid input (missing command, empty
	// prefix, empty line...) without throwing; `out` is left cleared.
	static bool parse(const std::string &line, IrcMessage &out);
};

#endif
