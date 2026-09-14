/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   IrcParameters.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vbullock <vbullock@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 23:32:11 by apestana          #+#    #+#             */
/*   Updated: 2026/09/14 14:01:17 by vbullock         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef IRCPARAMETERS_HPP
#define IRCPARAMETERS_HPP

#include <string>
#include <vector>

namespace IrcParameters
{
	// Split channel/recipient lists, ignoring empty comma-separated entries.
	std::vector<std::string> splitOnCommas(const std::string &value);
	// Produce one bounded reply parameter, replacing unusable values with "*".
	std::string safeParameter(const std::string &value);
}

#endif
