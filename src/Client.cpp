/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/27 23:49:05 by apestana          #+#    #+#             */
/*   Updated: 2026/09/10 18:35:21 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Client.hpp"

static const size_t MAX_IRC_LINE_SIZE = 512;

Client::Client(int fd)
	: _fd(fd), _passwordAccepted(false), _isRegistered(false)
{
}

Client::~Client()
{
}

int Client::getFd() const
{
	return _fd;
}

bool Client::appendToBuffer(const char *data, size_t len)
{
	// Continue counting the unfinished line from the previous receive.
	std::string::size_type lastEnd = _receiveBuffer.rfind("\r\n");
	size_t lineSize = lastEnd == std::string::npos
		? _receiveBuffer.size() : _receiveBuffer.size() - lastEnd - 2;
	bool previousWasCR = !_receiveBuffer.empty()
		&& _receiveBuffer[_receiveBuffer.size() - 1] == '\r';

	// Validate before allocating. Each complete line gets its own limit,
	// even when several commands arrive in the same recv().
	for (size_t i = 0; i < len; ++i)
	{
		++lineSize;
		if (previousWasCR && data[i] == '\n')
			lineSize = 0;
		else if (lineSize > MAX_IRC_LINE_SIZE - 2)
		{
			// At byte 511 only CR is allowed: LF may arrive in the next recv().
			if (lineSize != MAX_IRC_LINE_SIZE - 1 || data[i] != '\r')
				return false;
		}
		previousWasCR = data[i] == '\r';
	}

	_receiveBuffer.append(data, len);
	return true;
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
void Client::setPasswordAccepted(bool accepted)
{
	_passwordAccepted = accepted;
}

bool Client::hasAcceptedPassword() const
{
	return _passwordAccepted;
}

bool Client::tryRegister()
{
	if (_isRegistered || !_passwordAccepted || _nickname.empty() || _username.empty())
		return false;
	_isRegistered = true;
	return true;
}

bool Client::isRegistered() const
{
	return _isRegistered;
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
