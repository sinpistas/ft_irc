/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/27 23:48:58 by apestana          #+#    #+#             */
/*   Updated: 2026/09/13 20:39:15 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <iostream>
#include <string>
#include <cstddef>
#include <ctime>
#include <set>

// Minimal per-connection state: its file descriptor and raw receive buffer.
class Client
{
	public:
		Client(int fd, const std::string &hostname);
		~Client();

		// Return the descriptor associated with this connection.
		int getFd() const;
		// The address this connection came from, which is the host part of
		// every message this client originates.
		const std::string &getHostname() const;
		// When the connection was accepted, to measure how long it is taking
		// to finish registering.
		std::time_t getConnectionTime() const;
		// Start the wait that lets the bytes explaining a disconnection reach
		// the client before its socket is closed. Only the first call counts.
		void startClosing();
		std::time_t getClosingTime() const;
		// Abort this connection without allocating, even if it was already
		// waiting to flush a normal closing reply.
		void failForMemory();
		bool hasMemoryFailure() const;
		// "nick!user@host": the prefix RFC 2812 puts on every message sent by
		// a client, so whoever receives it knows who it came from. Build it
		// only here, never by hand at each call site, and never before the
		// client is registered, when nickname and username are still empty.
		std::string getPrefix() const;

		// Preserve received data. An IRC message may occupy 512 bytes at most,
		// CRLF included; a line longer than that is dropped on its own, without
		// closing the connection and without touching the other lines that
		// arrived in the same packet.
		void appendToBuffer(const char *data, size_t len);
		// Expose the accumulated data without copying it.
		const std::string &getBuffer() const;

		// If the buffer holds a complete line, remove it from the buffer and
		// return it, without its terminator, in `line`. RFC 2812 terminates
		// messages with "\r\n", but a bare "\n" is accepted too: that is what
		// a plain nc (without -C) and a few clients send, and real servers
		// take it as well. Complete lines that are over the IRC length limit
		// are discarded on the way, so `line` only ever holds a message the
		// parser may accept. Returns false, with no line to report, when the
		// buffer holds no full line yet.
		bool extractLine(std::string &line);

		// Queue raw bytes to be flushed to the socket later.
		void appendToSendBuffer(const std::string &data);
		// True while there are bytes left to send.
		bool hasPendingOutput() const;
		// Expose the pending bytes without copying them.
		const std::string &getSendBuffer() const;
		// Drop the first `len` bytes once send() has confirmed they went out.
		void consumeSendBuffer(size_t len);
		
		
		// Registration/Identity
		void setNickname(const std::string &nick);
		const std::string &getNickname() const;
		void setUsername(const std::string &user);
		const std::string &getUsername() const;
		void setRealname(const std::string &name);
		const std::string &getRealname() const;
		// What the other clients are told when this one goes away. Stays at
		// its default unless a QUIT command replaces it with the user's own
		// message, which is why it is read at removal time and not earlier.
		void setQuitReason(const std::string &reason);
		const std::string &getQuitReason() const;


		// PASS acceptance and completed registration are separate states.
		void setPasswordAccepted(bool accepted);
		bool hasAcceptedPassword() const;
		// Mark the client registered only after PASS, NICK and USER succeeded.
		bool tryRegister();
		bool isRegistered() const;
		
		// Channel membership, this side of it: the names are only an index of
		// the channels holding this client's fd, kept so a NICK change can find
		// the peers to notify. Server::addToChannel() and
		// Server::removeFromChannel() are the only callers allowed, because
		// they are what keeps this list and Channel::_members from drifting
		// apart. Calling these two directly from a command handler puts the
		// two sides out of sync.
		void joinChannel(const std::string &channel);
		void leaveChannel(const std::string &channel);
		const std::set<std::string> &getChannels() const;
		// Used by Server when destroying all memberships during disconnect.
		void clearChannels();
		bool isInChannel(const std::string &channel) const;
		
		// Modes (for MODE command)
		void setModes(const std::string &modes);  // Set user modes (e.g., "io")
		const std::string &getModes() const;
		void addMode(char mode);      // Add single mode
		void removeMode(char mode);   // Remove single mode
		bool hasMode(char mode) const; // Check if mode is set

	private:
		Client();

		int         _fd;
		std::string _hostname;
		std::time_t _connectedAt;
		std::time_t _closingSince;
		bool        _memoryFailure;
		std::string _receiveBuffer;
		// Set while the remains of an over-long line are being skipped, up to
		// and including the LF that ends it.
		bool        _discardingLine;
		std::string _sendBuffer;
		
		std::string  _nickname;           // For NICK command
		std::string  _username;           // For USER command
		std::string  _realname;           // For USER command
		std::string  _quitReason;
		std::set<std::string> _channels;  // For JOIN/KICK commands
		std::string  _modes;              // For MODE command (e.g., "io" for invisible+operator)
		bool         _passwordAccepted;
		bool         _isRegistered;
};

#endif
