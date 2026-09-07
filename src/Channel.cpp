#include "Channel.hpp"
#include <sstream>

Channel::Channel()
	: _name(""), _topic(""), _key(""), _inviteOnly(false), _topicLocked(false), _hasKey(false), _limit(0)
{
}

Channel::Channel(const std::string &name)
	: _name(name), _topic(""), _key(""), _inviteOnly(false), _topicLocked(false), _hasKey(false), _limit(0)
{
}

Channel::Channel(const Channel &other)
{
	*this = other;
}

Channel &Channel::operator=(const Channel &other)
{
	if (this != &other)
	{
		_name = other._name;
		_topic = other._topic;
		_key = other._key;
		_members = other._members;
		_operators = other._operators;
		_invited = other._invited;
		_inviteOnly = other._inviteOnly;
		_topicLocked = other._topicLocked;
		_hasKey = other._hasKey;
		_limit = other._limit;
	}
	return (*this);
}

Channel::~Channel()
{
}

const std::string &Channel::getName() const
{
	return (_name);
}

const std::string &Channel::getTopic() const
{
	return (_topic);
}

const std::string &Channel::getKey() const
{
	return (_key);
}

const std::vector<int> &Channel::getMembers() const
{
	return (_members);
}

size_t Channel::getLimit() const
{
	return (_limit);
}

bool Channel::isInviteOnly() const
{
	return (_inviteOnly);
}

bool Channel::isTopicLocked() const
{
	return (_topicLocked);
}

bool Channel::hasKey() const
{
	return (_hasKey);
}

void Channel::setTopic(const std::string &topic)
{
	_topic = topic;
}

void Channel::setKey(const std::string &key)
{
	_key = key;
	_hasKey = true;
}

void Channel::unsetKey()
{
	_key = "";
	_hasKey = false;
}

void Channel::setInviteOnly(bool value)
{
	_inviteOnly = value;
}

void Channel::setTopicLocked(bool value)
{
	_topicLocked = value;
}

void Channel::setLimit(size_t limit)
{
	_limit = limit;
}

bool Channel::isMember(int fd) const
{
	for (size_t i = 0; i < _members.size(); i++)
	{
		if (_members[i] == fd)
			return (true);
	}
	return (false);
}

bool Channel::isOperator(int fd) const
{
	for (size_t i = 0; i < _operators.size(); i++)
	{
		if (_operators[i] == fd)
			return (true);
	}
	return (false);
}

bool Channel::isInvited(int fd) const
{
	for (size_t i = 0; i < _invited.size(); i++)
	{
		if (_invited[i] == fd)
			return (true);
	}
	return (false);
}

bool Channel::isFull() const
{
	if (_limit == 0)
		return (false);
	return (_members.size() >= _limit);
}

bool Channel::isEmpty() const
{
	return (_members.empty());
}

void Channel::addMember(int fd)
{
	if (!isMember(fd))
		_members.push_back(fd);
}

void Channel::removeMember(int fd)
{
	for (size_t i = 0; i < _members.size(); i++)
	{
		if (_members[i] == fd)
		{
			_members.erase(_members.begin() + i);
			break ;
		}
	}
	removeOperator(fd);
	removeInvite(fd);
}

void Channel::addOperator(int fd)
{
	if (!isOperator(fd))
		_operators.push_back(fd);
}

void Channel::removeOperator(int fd)
{
	for (size_t i = 0; i < _operators.size(); i++)
	{
		if (_operators[i] == fd)
		{
			_operators.erase(_operators.begin() + i);
			return ;
		}
	}
}

void Channel::addInvite(int fd)
{
	if (!isInvited(fd))
		_invited.push_back(fd);
}

void Channel::removeInvite(int fd)
{
	for (size_t i = 0; i < _invited.size(); i++)
	{
		if (_invited[i] == fd)
		{
			_invited.erase(_invited.begin() + i);
			return ;
		}
	}
}

std::string Channel::getModeString() const
{
	std::string	modes = "+";
	std::string	args = "";

	if (_inviteOnly)
		modes += "i";
	if (_topicLocked)
		modes += "t";
	if (_hasKey)
	{
		modes += "k";
		args += " " + _key;
	}
	if (_limit > 0)
	{
		std::stringstream	ss;
		ss << _limit;
		modes += "l";
		args += " " + ss.str();
	}
	return (modes + args);
}

void Channel::broadcast(std::map<int, Client> &clients, const std::string &message, int except) const
{
	for (size_t i = 0; i < _members.size(); i++)
	{
		if (_members[i] == except)
			continue ;
		std::map<int, Client>::iterator it = clients.find(_members[i]);
		if (it != clients.end())
			it->second.appendOut(message + "\r\n");
	}
}
