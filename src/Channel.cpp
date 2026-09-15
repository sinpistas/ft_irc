
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
	: _channelName("Unnamed"), limit(0)
{
}

Channel::Channel(const std::string name)
	: _channelName(name), limit(0)
{
}

Channel::~Channel()
{

}

// Modes (for MODE command)
void Channel::addMode(char mode)
{
	if (_modes.find(mode) == std::string::npos)
		_modes += mode;
}
void Channel::removeMode(char mode)
{
	std::string::size_type pos = _modes.find(mode);
	if (pos != std::string::npos)
		_modes.erase(pos, 1);
}
bool Channel::hasMode(char mode) const
{
	return this->_modes.find(mode) != std::string::npos;
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

void Channel::setChannelKey(const std::string &key)
{
	this->_channelKey = key;
}

const std::string &Channel::getChannelKey() const
{
	return this->_channelKey;
}

void Channel::setLimit(int value)
{
	this->limit = value;
}

int Channel::getLimit() const
{
	return this->limit;
}

void Channel::addInvite(int fd)
{
	this->_invited.insert(fd);
}

void Channel::removeInvite(int fd)
{
	this->_invited.erase(fd);
}

bool Channel::isInvited(int fd) const
{
	return this->_invited.find(fd) != this->_invited.end();
}

void Channel::addOperator(int fd)
{
	if (!isOperator(fd))
		this->_operators.insert(fd);
}

void Channel::removeOperator(int fd)
{
	this->_operators.erase(fd);
}

bool Channel::isOperator(int fd) const
{
	if (this->_operators.find(fd) != this->_operators.end())
		return 1;
	return 0;
	
}

void Channel::addMember(int fd)
{
	if(!hasMember(fd))
		this->_members.insert(fd);
}

void Channel::removeMember(int fd)
{
	if(hasMember(fd))
	{
		this->_members.erase(fd);
		// Channel privileges must not survive the member's departure.
		this->_operators.erase(fd);
	}
}

const std::set<int> &Channel::getMembers() const
{
	return this->_members;
}

bool Channel::isEmpty() const
{
	return this->_members.empty();
}

bool Channel::hasMember(int fd) const
{
	if (this->_members.find(fd) != this->_members.end())
		return 1;
	return 0;
}
