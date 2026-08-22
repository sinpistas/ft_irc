/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/22 14:17:58 by apestana          #+#    #+#             */
/*   Updated: 2026/08/22 14:20:13 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CHANNEL_HPP
# define CHANNEL_HPP

# include <string>
# include <set>

/*
Esquema:
Server
 ├── es propietario de los Client
 └── es propietario de los Channel

Channel
 └── guarda los FDs de sus miembros

Para que:

Server
│
├── map<int, Client>
│
└── map<string, Channel>
         │
         ├── members: {4, 5, 8}
         └── operators: {4}
*/
class Channel
{
	public:

		Channel();
		Channel(const std::string &name);

		const std::string &getName() const;

		// Users
		void addMember(int clientFd);
		void removeMember(int clientFd);
		bool hasMember(int clientFd) const;

		// Operators
		void addOperator(int clientFd);
		void removeOperator(int clientFd);
		bool isOperator(int clientFd) const;

		// Invitations
		void invite(int clientFd);
		bool isInvited(int clientFd) const;
		void removeInvite(int clientFd);

		// Topics
		const std::string &getTopic() const;
		void setTopic(const std::string &topic);

		// Modes
		void setInviteOnly(bool value);
		bool isInviteOnly() const;

		void setTopicRestricted(bool value);
		bool isTopicRestricted() const;

		void setKey(const std::string &key);
		void removeKey();
		bool hasKey() const;

		void setUserLimit(size_t limit);
		void removeUserLimit();
		bool hasUserLimit() const;

	private:

		std::string _name;
		std::string _topic;

		// Possible solution:
		// store clients’ FDs
		std::set<int> _members;
		std::set<int> _operators;
		std::set<int> _invited;

		// Modes
		bool _inviteOnly;
		bool _topicRestricted;

		std::string _key;

		bool _hasUserLimit;
		size_t _userLimit;
};

#endif