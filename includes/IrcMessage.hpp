/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   IrcMessage.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/28 14:44:29 by apestana          #+#    #+#             */
/*   Updated: 2026/09/13 21:42:56 by apestana         ###   ########.fr       */
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
	// Syntax errors return false and leave `out` cleared. NUL, CR and LF
	// are forbidden inside this already framed line. After 14 parameters,
	// the remainder is the 15th, with or without a leading colon.
	// Allocation failures propagate to the server's per-client handler;
	// `out` also stays cleared if parsing cannot finish for that reason.
	static bool parse(const std::string &line, IrcMessage &out);
};

#endif
