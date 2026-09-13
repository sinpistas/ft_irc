/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   IrcLimits.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 14:50:13 by apestana          #+#    #+#             */
/*   Updated: 2026/09/13 20:56:12 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef IRCLIMITS_HPP
#define IRCLIMITS_HPP

#include <string>

// Protocol sizes and local limits, shared by input and output handling.

// A message occupies 512 bytes at most, its terminating CRLF included...
static const std::string::size_type IRC_MESSAGE_MAX_SIZE = 512;
// ...which leaves 510 for the message itself.
static const std::string::size_type IRC_MESSAGE_MAX_CONTENT = IRC_MESSAGE_MAX_SIZE - 2;

// RFC 2812, section 1.2.1. Should the reference client turn out to need
// longer nicknames, this is the one line to change.
static const std::string::size_type IRC_NICKNAME_MAX_LENGTH = 9;

// Server policy, not an RFC 2812 maximum: keep enough room for a complete
// nick!user@host prefix, a command, its targets and message text.
static const std::string::size_type IRC_USERNAME_MAX_LENGTH = 32;
// Oversized values echoed in diagnostic parameters use "*" instead.
// Valid nicknames, channel names and server names all fit in this budget.
static const std::string::size_type IRC_REPLY_PARAMETER_MAX_LENGTH = 64;

#endif
