/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerLog.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/17 00:10:38 by apestana          #+#    #+#             */
/*   Updated: 2026/09/17 00:10:40 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ServerLog.hpp"
#include "Server.hpp"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>

ServerLog::ServerLog() : _enabled(false), _dropped(false), _size(0)
{
}

void ServerLog::initialize()
{
	// If stdout is unavailable, serving clients does not depend on logging.
	struct stat destination;
	_enabled = fstat(STDOUT_FILENO, &destination) == 0;
	// O_NONBLOCK has no effect on regular files. Preserve their O_APPEND
	// flag when the server is started with >> instead of replacing flags.
	if (_enabled && !S_ISREG(destination.st_mode))
		_enabled = fcntl(STDOUT_FILENO, F_SETFL, O_NONBLOCK) == 0;
}

int ServerLog::getFd() const
{
	return _enabled ? STDOUT_FILENO : -1;
}

bool ServerLog::hasPending() const
{
	return _size != 0;
}

// Keep entries on one line and prevent terminal control sequences in user data.
static void cleanField(char *out, size_t capacity, const char *in)
{
	size_t i = 0;
	if (in != NULL)
		for (; i + 1 < capacity && in[i] != '\0'; ++i)
		{
			const unsigned char c = static_cast<unsigned char>(in[i]);
			out[i] = (c >= 32 && c < 127) ? static_cast<char>(c) : '?';
		}
	out[i] = '\0';
}

void ServerLog::append(const char *level, const char *event, int fd,
	const char *nickname, const char *detail)
{
	if (!_enabled)
		return;
	char timeText[20] = "unknown-time";
	const std::time_t now = std::time(NULL);
	const std::tm *local = std::localtime(&now);
	if (local != NULL)
		std::strftime(timeText, sizeof(timeText), "%H:%M:%S", local);
	char safeLevel[12], safeEvent[48], safeNick[32], safeDetail[240];
	cleanField(safeLevel, sizeof(safeLevel), level);
	cleanField(safeEvent, sizeof(safeEvent), event);
	cleanField(safeNick, sizeof(safeNick), nickname);
	cleanField(safeDetail, sizeof(safeDetail), detail);
	char line[512];
	// Every string above is bounded; even a full-width int fits in line.
	int length;
	if (fd >= 0)
		length = std::sprintf(line, "%s [%-4s] %-18s fd=%d nick=%s %s\n",
			timeText, safeLevel, safeEvent, fd,
			safeNick[0] ? safeNick : "*", safeDetail);
	else
		length = std::sprintf(line, "%s [%-4s] %-18s %s\n",
			timeText, safeLevel, safeEvent, safeDetail);
	if (_size + static_cast<size_t>(length) > sizeof(_buffer))
	{
		_dropped = true;
		return;
	}
	std::memcpy(_buffer + _size, line, length);
	_size += length;
}

void ServerLog::flush()
{
	if (_size == 0)
		return;
	// Exactly one write per POLLOUT; a partial result waits for the next poll.
	const ssize_t sent = write(STDOUT_FILENO, _buffer, _size);
	if (sent <= 0)
	{
		// Best-effort logging: disable a failing sink without errno retries
		// or a busy loop, while the server keeps serving its clients.
		_enabled = false;
		return;
	}
	_size -= static_cast<size_t>(sent);
	std::memmove(_buffer, _buffer + sent, _size);
	if (_dropped && sizeof(_buffer) - _size >= 512)
	{
		_dropped = false;
		append("WARN", "LOGS DROPPED", -1, NULL,
			"Console queue was full; some diagnostic entries were discarded");
	}
}

void ServerLog::handlePoll(short events)
{
	if (events & (POLLERR | POLLHUP | POLLNVAL))
		_enabled = false;
	else if (events & POLLOUT)
		flush();
}

void Server::logEvent(const char *level, const char *event, int fd, const char *detail)
{
	std::map<int, Client>::const_iterator it = _clients.find(fd);
	_logger.append(level, event, fd,
		it == _clients.end() ? NULL : it->second.getNickname().c_str(), detail);
}
