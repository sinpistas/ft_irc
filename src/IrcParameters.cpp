/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   IrcParameters.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 23:27:41 by apestana          #+#    #+#             */
/*   Updated: 2026/09/13 23:27:44 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "IrcParameters.hpp"
#include "IrcLimits.hpp"

namespace IrcParameters
{

std::vector<std::string> splitOnCommas(const std::string &value)
{
	std::vector<std::string> parts;
	std::string::size_type start = 0;

	while (start <= value.size())
	{
		std::string::size_type end = value.find(',', start);
		if (end == std::string::npos)
			end = value.size();
		if (end > start)
			parts.push_back(value.substr(start, end - start));
		start = end + 1;
	}
	return parts;
}

std::string safeParameter(const std::string &value)
{
	const std::string token = value.substr(0, value.find(' '));
	if (token.empty() || token[0] == ':'
		|| token.size() > IRC_REPLY_PARAMETER_MAX_LENGTH)
		return "*";
	return token;
}

}
