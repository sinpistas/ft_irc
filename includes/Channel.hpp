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

		const std::string &getName() const;

		void setTopic(const std::string &topic);
		const std::string &getTopic() const;

		void addMode(char mode);
		void removeMode(char mode);
		bool hasMode(char mode) const;
		const std::string &getModes() const;

		void addMember(int fd);
		// Remove membership and any operator privileges for this descriptor.
		void removeMember(int fd);
		bool hasMember(int fd) const;

		void addOperator(int fd);
		void removeOperator(int fd);
		bool isOperator(int fd) const;

	private:

		std::string _channelName;
		std::string _topic;
		std::string _modes;
		std::set<int> _members;
		std::set<int> _operators;
};

#endif
