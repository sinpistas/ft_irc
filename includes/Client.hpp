/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/22 14:13:33 by apestana          #+#    #+#             */
/*   Updated: 2026/08/22 14:19:12 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef __CLIENT_HPP__
# define __CLIENT_HPP__

# include <string>

class Client
{
	public:
		Client();
		Client(int fd);

		int getFd() const;

		const std::string &getNickname() const;
		const std::string &getUsername() const;

		void setNickname(const std::string &nickname);
		void setUsername(const std::string &username);

		bool hasPassword() const;
		bool hasNickname() const;
		bool hasUsername() const;
		bool isRegistered() const;

		void setPasswordAccepted(bool value);
		void updateRegistrationState();

		// Buffer
		std::string &getInputBuffer();
		void appendToBuffer(const std::string &data);

	private:

		/*****attributes*****/
		int         _fd;

		std::string _nickname;
		std::string _username;

		bool        _hasPassword;
		bool        _hasNickname;
		bool        _hasUsername;
		bool        _registered;

		// Data received that does not yet
        // necessarily form a complete command
		std::string _inputBuffer;

};

#endif