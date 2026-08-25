#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>

// Store the configuration and expose the server lifecycle entry point.
class Server
{
	public:
		// Initialize the server with the listening port and client password.
		Server(int port, const std::string &password);
		~Server();

		// Start the server's main runtime logic.
		void run();

	private:
		// Disable default construction and copying for this server instance.
		Server();
		Server(const Server &other);
		Server &operator=(const Server &other);

		int         _port;
		std::string _password;
};

#endif
