#include "Channel.hpp"
#include <sstream>

Channel::Channel()
	: name(""), topic(""), key(""), inviteOnly(false), topicLocked(false), hasKey(false), limit(0)
{
}

Channel::Channel(const std::string &name)
	: name(name), topic(""), key(""), inviteOnly(false), topicLocked(false), hasKey(false), limit(0)
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
		name = other.name;
		topic = other.topic;
		key = other.key;
		members = other.members;
		operators = other.operators;
		invited = other.invited;
		inviteOnly = other.inviteOnly;
		topicLocked = other.topicLocked;
		hasKey = other.hasKey;
		limit = other.limit;
	}
	return (*this);
}

Channel::~Channel()
{
}

const std::string &Channel::getname() const
{
	return (name);
}

const std::string &Channel::gettopic() const
{
	return (topic);
}

const std::string &Channel::getkey() const
{
	return (key);
}

const std::vector<int> &Channel::getmembers() const
{
	return (members);
}

size_t Channel::getlimit() const
{
	return (limit);
}

bool Channel::isinviteonly() const
{
	return (inviteOnly);
}

bool Channel::istopiclocked() const
{
	return (topicLocked);
}

bool Channel::haskey() const
{
	return (hasKey);
}

void Channel::settopic(const std::string &topic)
{
	this->topic = topic;
}

void Channel::setkey(const std::string &key)
{
	this->key = key;
	this->hasKey = true;
}

void Channel::unsetkey()
{
	this->key = "";
	this->hasKey = false;
}

void Channel::setinviteonly(bool value)
{
	this->inviteOnly = value;
}

void Channel::settopiclocked(bool value)
{
	this->topicLocked = value;
}

void Channel::setlimit(size_t limit)
{
	this->limit = limit;
}

bool Channel::ismember(int fd) const
{
	for (size_t i = 0; i < members.size(); i++)
	{
		if (members[i] == fd)
			return (true);
	}
	return (false);
}

bool Channel::isoperator(int fd) const
{
	for (size_t i = 0; i < operators.size(); i++)
	{
		if (operators[i] == fd)
			return (true);
	}
	return (false);
}

bool Channel::isinvited(int fd) const
{
	for (size_t i = 0; i < invited.size(); i++)
	{
		if (invited[i] == fd)
			return (true);
	}
	return (false);
}

bool Channel::isfull() const
{
	if (limit == 0)
		return (false);
	return (members.size() >= limit);
}

bool Channel::isempty() const
{
	return (members.empty());
}

void Channel::addmember(int fd)
{
	if (!ismember(fd))
		members.push_back(fd);
}

void Channel::removemember(int fd)
{
	for (size_t i = 0; i < members.size(); i++)
	{
		if (members[i] == fd)
		{
			members.erase(members.begin() + i);
			break ;
		}
	}
	removeoperator(fd);
	removeinvite(fd);
}

void Channel::addoperator(int fd)
{
	if (!isoperator(fd))
		operators.push_back(fd);
}

void Channel::removeoperator(int fd)
{
	for (size_t i = 0; i < operators.size(); i++)
	{
		if (operators[i] == fd)
		{
			operators.erase(operators.begin() + i);
			return ;
		}
	}
}

void Channel::addinvite(int fd)
{
	if (!isinvited(fd))
		invited.push_back(fd);
}

void Channel::removeinvite(int fd)
{
	for (size_t i = 0; i < invited.size(); i++)
	{
		if (invited[i] == fd)
		{
			invited.erase(invited.begin() + i);
			return ;
		}
	}
}

std::string Channel::getmodestring() const
{
	std::string	modes = "+";
	std::string	args = "";

	if (inviteOnly)
		modes += "i";
	if (topicLocked)
		modes += "t";
	if (hasKey)
	{
		modes += "k";
		args += " " + key;
	}
	if (limit > 0)
	{
		std::stringstream	ss;
		ss << limit;
		modes += "l";
		args += " " + ss.str();
	}
	return (modes + args);
}

void Channel::broadcast(std::map<int, Client> &clients, const std::string &message, int except) const
{
	for (size_t i = 0; i < members.size(); i++)
	{
		if (members[i] == except)
			continue ;
		std::map<int, Client>::iterator it = clients.find(members[i]);
		if (it != clients.end())
			it->second.appendout(message + "\r\n");
	}
}
