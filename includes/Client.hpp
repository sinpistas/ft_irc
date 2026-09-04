/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: vbullock <vbullock@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/27 23:48:58 by apestana          #+#    #+#             */
/*   Updated: 2026/09/04 19:16:26 by vbullock         ###   ########.fr       */
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

		// Preserve received data until a complete IRC message can be processed.
		void appendToBuffer(const char *data, size_t len);
		// Expose the accumulated data without copying it.
		const std::string &getBuffer() const;

		// If the buffer holds a complete "\r\n"-terminated line, remove it
		// from the buffer and return it (without the "\r\n") in `line`.
		// Returns false, leaving the buffer untouched, when no full line
		// is available yet.
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
		bool isAuthenticated() const;  // Returns true when NICK + USER both set
		
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
		std::string _sendBuffer;
		
		std::string  _nickname;           // For NICK command
		std::string  _username;           // For USER command
		std::string  _realname;           // For USER command
		std::set<std::string> _channels;  // For JOIN/KICK commands
		std::string  _modes;              // For MODE command (e.g., "io" for invisible+operator)
		bool         _isAuthenticated;    // Registration complete (NICK + USER received)
		std::map<std::string, std::string> _channelModes;  // Per-channel modes (e.g., channel -> operator status)
};

#endif
