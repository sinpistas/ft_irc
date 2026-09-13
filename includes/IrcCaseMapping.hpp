/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   IrcCaseMapping.hpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/10 18:52:38 by apestana          #+#    #+#             */
/*   Updated: 2026/09/13 23:32:32 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef IRCCASEMAPPING_HPP
#define IRCCASEMAPPING_HPP

#include <string>

// IRC treats []\~ as equivalent to {}|^, in addition to ASCII letter case.
inline char foldIrcCase(char character)
{
	if (character >= 'A' && character <= 'Z')
		return static_cast<char>(character - 'A' + 'a');
	if (character == '[')
		return '{';
	if (character == ']')
		return '}';
	if (character == '\\')
		return '|';
	if (character == '~')
		return '^';
	return character;
}

inline std::string normalizeIrcName(const std::string &name)
{
	std::string normalized = name;
	for (std::string::size_type i = 0; i < normalized.size(); ++i)
		normalized[i] = foldIrcCase(normalized[i]);
	return normalized;
}

// Compare nicknames without allocating normalized copies.
inline bool areSameNicknames(const std::string &left, const std::string &right)
{
	if (left.size() != right.size())
		return false;
	for (std::string::size_type i = 0; i < left.size(); ++i)
	{
		if (foldIrcCase(left[i]) != foldIrcCase(right[i]))
			return false;
	}
	return true;
}

#endif
