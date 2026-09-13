/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   IrcLimits.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 14:50:13 by apestana          #+#    #+#             */
/*   Updated: 2026/09/13 14:50:14 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef IRCLIMITS_HPP
#define IRCLIMITS_HPP

#include <string>

// The sizes RFC 2812 puts on the protocol, kept together so the side that
// reads messages and the side that writes them cannot drift apart.

// A message occupies 512 bytes at most, its terminating CRLF included...
static const std::string::size_type IRC_MESSAGE_MAX_SIZE = 512;
// ...which leaves 510 for the message itself.
static const std::string::size_type IRC_MESSAGE_MAX_CONTENT = IRC_MESSAGE_MAX_SIZE - 2;

// RFC 2812, section 1.2.1. Should the reference client turn out to need
// longer nicknames, this is the one line to change.
static const std::string::size_type IRC_NICKNAME_MAX_LENGTH = 9;

#endif
