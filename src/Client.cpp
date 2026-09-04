/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vbullock <vbullock@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/27 23:49:05 by apestana          #+#    #+#             */
/*   Updated: 2026/09/04 19:15:55 by vbullock         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Client.hpp"

Client::Client(int fd)
	: _fd(fd), _isAuthenticated(false)
{
}

Client::~Client()
{
}

int Client::getFd() const
{
	return _fd;
}

void Client::appendToBuffer(const char *data, size_t len)
{
	// A single recv() may contain a partial message or several messages.
	_receiveBuffer.append(data, len);
}

const std::string &Client::getBuffer() const
{
	return _receiveBuffer;
}

bool Client::extractLine(std::string &line)
{
	std::string::size_type pos = _receiveBuffer.find("\r\n");
	if (pos == std::string::npos)
		return false;

	line = _receiveBuffer.substr(0, pos);
	_receiveBuffer.erase(0, pos + 2);
	return true;
}

void Client::appendToSendBuffer(const std::string &data)
{
	_sendBuffer.append(data);
}

bool Client::hasPendingOutput() const
{
	return !_sendBuffer.empty();
}

const std::string &Client::getSendBuffer() const
{
	return _sendBuffer;
}

void Client::consumeSendBuffer(size_t len)
{
	_sendBuffer.erase(0, len);
}



// Registration/Identity
void Client::setNickname(const std::string &nick)
{
	_nickname = nick;
}
const std::string &Client::getNickname() const
{
	return _nickname;
}
void Client::setUsername(const std::string &user)
{
	_username = user;
}
const std::string &Client::getUsername() const
{
	return _username;
}
void Client::setRealname(const std::string &name)
{
	_realname = name;
}
const std::string &Client::getRealname() const
{
	return _realname;
}
bool Client::isAuthenticated() const
{
	return _isAuthenticated;
}



// Channel Management (for JOIN/KICK)
void Client::joinChannel(const std::string &channel)
{
	_channels.insert(channel);
}
void Client::leaveChannel(const std::string &channel)
{
	if (_channels.find(channel) != _channels.end())
	{
		_channels.erase(channel);
	}
	else
		std::cout << "User was not found in channel " << channel << std::endl;
		//
		//POSSIBLE ERROR MSG to implement
		//
}

bool Client::isInChannel(const std::string &channel) const
{
	if (_channels.find(channel) != _channels.end())
	{
		return 1;
	}
	return 0;
}
const std::set<std::string> &Client::getChannels() const
{
	return _channels;
}

void Client::addMode(char mode)
{
    if (_modes.find(mode) == std::string::npos)
        _modes += mode;
}

void Client::removeMode(char mode)
{
    std::string::size_type pos = _modes.find(mode);
    if (pos != std::string::npos)
        _modes.erase(pos, 1);
}

bool Client::hasMode(char mode) const
{
    return _modes.find(mode) != std::string::npos;
}

void Client::setModes(const std::string &modes)
{
	std::cout << "Modes set: " << modes << std::endl;

}
const std::string &Client::getModes() const
{
	return this->_modes;
}