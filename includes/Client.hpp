/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/27 23:48:58 by apestana          #+#    #+#             */
/*   Updated: 2026/09/13 13:51:49 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <iostream>
#include <string>
#include <cstddef>
#include <set>
#include <map>

// Minimal per-connection state: its file descriptor and raw receive buffer.
class Client
{
	public:
		explicit Client(int fd);
		~Client();

		// Return the descriptor associated with this connection.
		int getFd() const;

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


		// PASS acceptance and completed registration are separate states.
		void setPasswordAccepted(bool accepted);
		bool hasAcceptedPassword() const;
		// Mark the client registered only after PASS, NICK and USER succeeded.
		bool tryRegister();
		bool isRegistered() const;
		
		// Channel Management (for JOIN/KICK)
		void joinChannel(const std::string &channel);
		void leaveChannel(const std::string &channel);
		const std::set<std::string> &getChannels() const;
		bool isInChannel(const std::string &channel) const;
		
		// Modes (for MODE command)
		void setModes(const std::string &modes);  // Set user modes (e.g., "io")
		const std::string &getModes() const;
		void addMode(char mode);      // Add single mode
		void removeMode(char mode);   // Remove single mode
		bool hasMode(char mode) const; // Check if mode is set
		
		// Per-channel modes (for KICK/MODE in channels)
		void setChannelMode(const std::string &channel, char mode);
		void removeChannelMode(const std::string &channel, char mode);
		bool isOperatorInChannel(const std::string &channel) const;

	private:
		Client();

		int         _fd;
		std::string _receiveBuffer;
		// Set while the remains of an over-long line are being skipped, up to
		// and including the LF that ends it.
		bool        _discardingLine;
		std::string _sendBuffer;
		
		std::string  _nickname;           // For NICK command
		std::string  _username;           // For USER command
		std::string  _realname;           // For USER command
		std::set<std::string> _channels;  // For JOIN/KICK commands
		std::string  _modes;              // For MODE command (e.g., "io" for invisible+operator)
		bool         _passwordAccepted;
		bool         _isRegistered;
		std::map<std::string, std::string> _channelModes;  // Per-channel modes (e.g., channel -> operator status)
};

#endif
