/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/27 23:16:18 by apestana          #+#    #+#             */
/*   Updated: 2026/08/27 23:49:14 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include <iostream>
#include <cstdlib>
#include <cctype>

// Validate the port argument and convert it to an integer.
static bool parsePort(const std::string &arg, int &port)
{
	if (arg.empty())
		return false;

	// Accept only decimal digits so the conversion cannot ignore invalid input.
	for (std::string::size_type i = 0; i < arg.size(); ++i)
	{
		if (!std::isdigit(static_cast<unsigned char>(arg[i])))
			return false;
	}

	long value = std::atol(arg.c_str());
	if (value < 1 || value > 65535)
		return false;
	port = static_cast<int>(value);
	return true;
}

int main(int argc, char **argv)
{
	// The server expects exactly one port and one password from the command line.
	if (argc != 3)
	{
		std::cerr << "Usage: " << argv[0] << " <port> <password>" << std::endl;
		return 1;
	}

	// Validate the port before constructing the server.
	int port;
	if (!parsePort(argv[1], port))
	{
		std::cerr << "Error: invalid port '" << argv[1] << "'" << std::endl;
		return 1;
	}

	try
	{
		// Let Server own the startup and runtime logic.
		Server server(port, argv[2]);
		server.run();
	}
	catch (const std::exception &e)
	{
		// Report startup or runtime failures and return a failure status.
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
