#include "Server.hpp"
#include <iostream>

Server::Server(int port, const std::string &password)
	: _port(port), _password(password)
{
	// Store the configuration needed by the server during its lifetime.
}

Server::~Server()
{
}

void Server::run()
{
	// This is the current startup point; networking and client handling will be added here.
	std::cout << "Server ready to start on port " << _port << std::endl;
}
