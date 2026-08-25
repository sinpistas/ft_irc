#include "Server.hpp"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <cerrno>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>

Server::Server(int port, const std::string &password)
	: _port(port), _password(password), _serverFd(-1)
{
	// Store the configuration needed by the server during its lifetime.
}

Server::~Server()
{
	// Release the listening socket when the server is destroyed.
	if (_serverFd >= 0)
		close(_serverFd);
}

/*setsockopt config use:
	-sockfd: The socket descriptor you wish to modify.
	-level: SOL_SOCKET for general socket options.
	-optname: SO_REUSEADDR allows binding to a port.
	-optval: A pointer to the value you wish to assign to the option.
	-optlen: The size in bytes of the value pointed to by optval.
*/
void Server::initSocket()
{
	// Create an IPv4 TCP socket for incoming client connections.
	_serverFd = socket(AF_INET, SOCK_STREAM, 0);
	if (_serverFd < 0)
		throw std::runtime_error(std::string("socket: ") + std::strerror(errno));

	// Allow the port to be reused immediately after a previous server shutdown.
	int opt = 1;
	if (setsockopt(_serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
		throw std::runtime_error(std::string("setsockopt: ") + std::strerror(errno));

	// poll() will be added later; the socket must already be non-blocking for it.
	if (fcntl(_serverFd, F_SETFL, O_NONBLOCK) < 0)
		throw std::runtime_error(std::string("fcntl: ") + std::strerror(errno));

	// Configure the server address to listen on every local network interface.
	// htons and htonl convert numbers from processor’s memory format (Host 
	// Byte Order) to the standard network format (Network Byte Order).
	struct sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(static_cast<unsigned short>(_port));
	addr.sin_addr.s_addr = htonl(INADDR_ANY);


	if (bind(_serverFd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0)
		throw std::runtime_error(std::string("bind: ") + std::strerror(errno));

	// Start listening and allow the system to queue pending connections.
	if (listen(_serverFd, SOMAXCONN) < 0)
		throw std::runtime_error(std::string("listen: ") + std::strerror(errno));

	std::cout << "Server listening on port " << _port << std::endl;
}

void Server::run()
{
	initSocket();

	// Keep the process alive until the poll() event loop is implemented.
	while (true)
		pause();
}
