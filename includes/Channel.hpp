/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/16 01:07:29 by apestana          #+#    #+#             */
/*   Updated: 2026/09/16 01:07:32 by apestana         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <iostream>
#include <string>
#include <cstddef>
#include <set>
#include <map>

class Channel
{
	public:

		Channel();
		Channel(const std::string name);
		~Channel();

		// The channel's name as it was written when the channel was created.
		// Looking a channel up is case insensitive, but what the clients are
		// shown is this spelling, the same one for everybody.
		const std::string &getName() const;

		void setTopic(const std::string &topic);
		const std::string &getTopic() const;

		void addMode(char mode);
		void removeMode(char mode);
		bool hasMode(char mode) const;
		const std::string &getModes() const;
		// Commit MODE changes prepared on a copy of this channel, without allocating.
		void swapModeState(Channel &prepared);

		void addMember(int fd);
		// Remove membership and any operator privileges for this descriptor.
		void removeMember(int fd);
		bool hasMember(int fd) const;
		// Every member's descriptor, for broadcasting to the whole channel.
		const std::set<int> &getMembers() const;
		bool isEmpty() const;

		void setChannelKey(const std::string &key);
		const std::string &getChannelKey() const;
		void setLimit(int value);
		int getLimit() const;

		// Standing invitations. They only decide anything while the channel
		// is invite-only: on any other channel an invitation is just a
		// message. An invitation is used up when its holder joins, and goes
		// away with them if they disconnect first.
		void addInvite(int fd);
		void removeInvite(int fd);
		bool isInvited(int fd) const;

		void addOperator(int fd);
		void removeOperator(int fd);
		bool isOperator(int fd) const;

	private:

		std::string _channelName;
		std::string _topic;
		std::string _modes;
		std::set<int> _members;
		std::set<int> _operators;
		std::set<int> _invited;
		std::string _channelKey;
		int	limit;
		
};

#endif
