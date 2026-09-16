/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/27 23:49:05 by apestana          #+#    #+#             */
/*   Updated: 2026/09/13 20:39:13 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Client.hpp"
#include "IrcCaseMapping.hpp"
#include "IrcLimits.hpp"

Client::Client(int fd, const std::string &hostname)
	: _fd(fd), _hostname(hostname),
	  _connectedAt(std::time(NULL)), _closingSince(0), _memoryFailure(false),
	  _discardingLine(false),
	  _quitReason("Connection closed"),
	  _passwordAccepted(false), _isRegistered(false)
{
}

Client::~Client()
{
}

int Client::getFd() const
{
	return _fd;
}

const std::string &Client::getHostname() const
{
	return _hostname;
}

std::time_t Client::getConnectionTime() const
{
	return _connectedAt;
}

void Client::startClosing()
{
	if (_closingSince == 0)
		_closingSince = std::time(NULL);
}

std::time_t Client::getClosingTime() const
{
	return _closingSince;
}

void Client::failForMemory()
{
	_memoryFailure = true;
	startClosing();
	_receiveBuffer.clear();
	_sendBuffer.clear();
}

bool Client::hasMemoryFailure() const
{
	return _memoryFailure;
}

std::string Client::getPrefix() const
{
	return _nickname + "!" + _username + "@" + _hostname;
}

void Client::appendToBuffer(const char *data, size_t len)
{
	size_t start = 0;

	// An over-long line was cut in a previous receive: skip what is left of
	// it, up to and including the LF that ends it, before reading commands
	// again. Only the LF has to be found, so a terminator split between two
	// recv() needs no special care here.
	if (_discardingLine)
	{
		while (start < len)
		{
			if (data[start++] == '\n')
			{
				_discardingLine = false;
				break;
			}
		}
		if (_discardingLine)
			return;
	}

	_receiveBuffer.append(data + start, len - start);

	// Complete lines are left alone here: extractLine() is what decides
	// whether each of them is short enough to be executed. Only the
	// unterminated tail could grow without end, so it is the one cut here,
	// and cutting it is what keeps the buffer bounded.
	std::string::size_type lastEnd = _receiveBuffer.rfind('\n');
	std::string::size_type lineStart = lastEnd == std::string::npos ? 0 : lastEnd + 1;
	size_t lineSize = _receiveBuffer.size() - lineStart;

	// A CR at the very end may still turn out to be the first half of a
	// CRLF terminator, so it does not count as content yet.
	if (lineSize > 0 && _receiveBuffer[_receiveBuffer.size() - 1] == '\r')
		--lineSize;

	if (lineSize > IRC_MESSAGE_MAX_CONTENT)
	{
		_receiveBuffer.erase(lineStart);
		_discardingLine = true;
	}
}

const std::string &Client::getBuffer() const
{
	return _receiveBuffer;
}

bool Client::extractLine(std::string &line)
{
	while (true)
	{
		std::string::size_type pos = _receiveBuffer.find('\n');
		if (pos == std::string::npos)
			return false;

		// The LF ends the line either way; a CRLF terminator just leaves its
		// CR right before it, and that CR is not part of the message.
		std::string::size_type end = pos;
		if (end > 0 && _receiveBuffer[end - 1] == '\r')
			--end;

		if (end > IRC_MESSAGE_MAX_CONTENT)
		{
			// Too long to be a valid IRC message. Drop this line alone and
			// carry on with the next one: the same packet may well hold
			// perfectly valid commands both before and after it.
			_receiveBuffer.erase(0, pos + 1);
			continue;
		}

		line = _receiveBuffer.substr(0, end);
		_receiveBuffer.erase(0, pos + 1);
		return true;
	}
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
void Client::setQuitReason(const std::string &reason)
{
	_quitReason = reason;
}
const std::string &Client::getQuitReason() const
{
	return _quitReason;
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
	_channels.insert(normalizeIrcName(channel));
}
void Client::leaveChannel(const std::string &channel)
{
	const std::string channelName = normalizeIrcName(channel);
	if (_channels.find(channelName) != _channels.end())
	{
		_channels.erase(channelName);
	}
}

bool Client::isInChannel(const std::string &channel) const
{
	if (_channels.find(normalizeIrcName(channel)) != _channels.end())
	{
		return 1;
	}
	return 0;
}
const std::set<std::string> &Client::getChannels() const
{
	return _channels;
}

void Client::clearChannels()
{
	_channels.clear();
}
