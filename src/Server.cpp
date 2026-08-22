/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/22 14:07:53 by apestana          #+#    #+#             */
/*   Updated: 2026/08/22 14:17:10 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

/* Esquema para run

        ┌─────────────┐
        │   poll()    │
        └──────┬──────┘
               │
               ▼
      ¿Qué FD tiene evento?
               │
      ┌────────┼────────┐
      │        │        │
      ▼        ▼        ▼
  servidor  cliente  cliente
      │        │        │
   accept    recv      error
      │        │        │
      └────────┼────────┘
               │
               ▼
             poll()

*/
void Server::run()
{
    _running = true;

    while (_running)
    {
		// Wait until some event occurs. 
        int result = poll(&_pollFds[0], _pollFds.size(), -1);

        if (result < 0)
        {
            // Handle errors
            break;
        }

        handlePollEvents();
    }
}

//specify which incidents and cases we will be handling
void Server::handlePollEvents()
{
    for (size_t i = 0; i < _pollFds.size(); i++)
    {
        if (_pollFds[i].revents == 0)
            continue;

        int fd = _pollFds[i].fd;

        // Event on the server socket
		if (fd == _serverFd)
        {
            if (_pollFds[i].revents & POLLIN)
                acceptClient();
        }

        // Event on a client
		else
        {
            if (_pollFds[i].revents & POLLIN)
                receiveFromClient(fd);

            // Handle disconnections/errors
        }
    }
}