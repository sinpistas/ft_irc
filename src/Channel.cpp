
#include "Channel.hpp"

// ∗ TOPIC - Change or view the channel topic
// ∗ MODE - Change the channel’s mode:
// · i: Set/remove Invite-only channel
// · t: Set/remove the restrictions of the TOPIC command to channel
// operators
// · k: Set/remove the channel key (password)
// · o: Give/take channel operator privilege
// · l: Set/remove the user limit to channel

Channel::Channel()
	: _channelName("Unnamed")
{
}

Channel::Channel(const std::string name)
	: _channelName(name)
{
}

Channel::~Channel()
{

}

// Modes (for MODE command)
void Channel::addMode(char mode)
{
	std::cout << "Mode added." << mode << std::endl;
}
void Channel::removeMode(char mode)
{
	std::cout << "Mode removed." << mode << std::endl;
}
bool Channel::hasMode(char mode) const
{
	if (this->_modes.find(mode) != '\n')
		return 1;
	return 0;	
}

void Channel::setTopic(const std::string &topic)
{
	this->_topic = topic;
}

const std::string &Channel::getName() const
{
	return this->_channelName;
}

const std::string &Channel::getTopic() const
{
	return this->_topic;
}

const std::string &Channel::getModes() const
{
	return this->_modes;
}

void Channel::addOperator(int fd)
{
	std::cout << "Operator added." << fd << std::endl;

}
void Channel::removeOperator(int fd)
{
	std::cout << "Operator removed." << fd << std::endl;
}
bool Channel::isOperator(int fd) const
{
	if (this->_operators.find(fd) != this->_operators.end())
		return 1;
	return 0;
	
}

void Channel::addMember(int fd)
{
	this->_members.insert(fd);
}
void Channel::removeMember(int fd)
{
	this->_members.erase(fd);
	// Channel privileges must not survive the member's departure.
	this->_operators.erase(fd);
}
bool Channel::hasMember(int fd) const
{
	if (this->_members.find(fd) != this->_members.end())
		return 1;
	return 0;
}
