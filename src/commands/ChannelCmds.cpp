#include "Server.hpp"
#include <cstdlib>
#include <sstream>

static std::vector<std::string> splitList(const std::string &str, char sep)
{
	std::vector<std::string>	result;
	std::stringstream			ss(str);
	std::string					item;

	while (std::getline(ss, item, sep))
		result.push_back(item);
	return (result);
}

std::string Server::userPrefix(int fd)
{
	return (_clients[fd].prefix());
}

void Server::sendNumeric(int fd, const std::string &code, const std::string &args)
{
	std::string	nick = _clients[fd].getNickname();

	// Numerics are addressed to a nick. Before registration the RFC
	// placeholder "*" stands in; without it clients mis-parse the reply.
	if (nick.empty())
		nick = "*";
	_clients[fd].appendOut(":" + _serverName + " " + code + " " + nick + " " + args + "\r\n");
}

int Server::getFdByNick(const std::string &nick)
{
	std::map<int, Client>::iterator	it;

	for (it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (it->second.getNickname() == nick)
			return (it->first);
	}
	return (-1);
}

std::string Server::buildNamesList(Channel &channel)
{
	std::string					list = "";
	const std::vector<int>		&members = channel.getMembers();

	for (size_t i = 0; i < members.size(); i++)
	{
		if (i > 0)
			list += " ";
		if (channel.isOperator(members[i]))
			list += "@";
		list += _clients[members[i]].getNickname();
	}
	return (list);
}

void Server::removeFromChannel(int fd, const std::string &name)
{
	std::map<std::string, Channel>::iterator	it = _channels.find(name);

	if (it == _channels.end())
		return ;
	it->second.removeMember(fd);
	if (it->second.isEmpty())
		_channels.erase(it);
}

void Server::handleJoinCommand(int fd, const std::vector<std::string> &params)
{
	if (params.empty())
	{
		sendNumeric(fd, ERR_NEEDMOREPARAMS, "JOIN :Not enough parameters");
		return ;
	}

	std::vector<std::string>	names = splitList(params[0], ',');
	std::vector<std::string>	keys;

	if (params.size() > 1)
		keys = splitList(params[1], ',');

	for (size_t i = 0; i < names.size(); i++)
	{
		std::string	name = names[i];
		std::string	key = "";

		if (i < keys.size())
			key = keys[i];

		if (name.empty() || (name[0] != '#' && name[0] != '&'))
		{
			sendNumeric(fd, ERR_BADCHANMASK, name + " :Bad Channel Mask");
			continue ;
		}

		if (_channels.find(name) == _channels.end())
		{
			_channels[name] = Channel(name);
			_channels[name].addMember(fd);
			_channels[name].addOperator(fd);
		}
		else
		{
			Channel	&target = _channels[name];

			if (target.isMember(fd))
				continue ;
			if (target.isInviteOnly() && !target.isInvited(fd))
			{
				sendNumeric(fd, ERR_INVITEONLYCHAN, name + " :Cannot join channel (+i)");
				continue ;
			}
			if (target.hasKey() && target.getKey() != key)
			{
				sendNumeric(fd, ERR_BADCHANNELKEY, name + " :Cannot join channel (+k)");
				continue ;
			}
			if (target.isFull())
			{
				sendNumeric(fd, ERR_CHANNELISFULL, name + " :Cannot join channel (+l)");
				continue ;
			}
			target.addMember(fd);
			target.removeInvite(fd);
		}

		Channel	&channel = _channels[name];

		channel.broadcast(_clients, userPrefix(fd) + " JOIN " + name, -1);
		if (channel.getTopic().empty())
			sendNumeric(fd, RPL_NOTOPIC, name + " :No topic is set");
		else
			sendNumeric(fd, RPL_TOPIC, name + " :" + channel.getTopic());
		sendNumeric(fd, RPL_NAMREPLY, "= " + name + " :" + buildNamesList(channel));
		sendNumeric(fd, RPL_ENDOFNAMES, name + " :End of /NAMES list");
	}
}

void Server::handleInvitCommand(int fd, const std::vector<std::string> &params)
{
	if (params.size() < 2)
	{
		sendNumeric(fd, ERR_NEEDMOREPARAMS, "INVITE :Not enough parameters");
		return ;
	}

	std::string	nick = params[0];
	std::string	name = params[1];

	if (_channels.find(name) == _channels.end())
	{
		sendNumeric(fd, ERR_NOSUCHCHANNEL, name + " :No such channel");
		return ;
	}

	Channel	&channel = _channels[name];

	if (!channel.isMember(fd))
	{
		sendNumeric(fd, ERR_NOTONCHANNEL, name + " :You're not on that channel");
		return ;
	}
	if (!channel.isOperator(fd))
	{
		sendNumeric(fd, ERR_CHANOPRIVSNEEDED, name + " :You're not channel operator");
		return ;
	}

	int	targetFd = getFdByNick(nick);

	if (targetFd == -1)
	{
		sendNumeric(fd, ERR_NOSUCHNICK, nick + " :No such nick/channel");
		return ;
	}
	if (channel.isMember(targetFd))
	{
		sendNumeric(fd, ERR_USERONCHANNEL, nick + " " + name + " :is already on channel");
		return ;
	}

	channel.addInvite(targetFd);
	sendNumeric(fd, RPL_INVITING, nick + " " + name);
	_clients[targetFd].appendOut(userPrefix(fd) + " INVITE " + nick + " :" + name + "\r\n");
}

void Server::handleKickCommand(int fd, const std::vector<std::string> &params)
{
	if (params.size() < 2)
	{
		sendNumeric(fd, ERR_NEEDMOREPARAMS, "KICK :Not enough parameters");
		return ;
	}

	std::string	name = params[0];
	std::string	nick = params[1];
	std::string	reason = _clients[fd].getNickname();

	if (params.size() > 2)
		reason = params[2];

	if (_channels.find(name) == _channels.end())
	{
		sendNumeric(fd, ERR_NOSUCHCHANNEL, name + " :No such channel");
		return ;
	}

	Channel	&channel = _channels[name];

	if (!channel.isMember(fd))
	{
		sendNumeric(fd, ERR_NOTONCHANNEL, name + " :You're not on that channel");
		return ;
	}
	if (!channel.isOperator(fd))
	{
		sendNumeric(fd, ERR_CHANOPRIVSNEEDED, name + " :You're not channel operator");
		return ;
	}

	int	targetFd = getFdByNick(nick);

	if (targetFd == -1 || !channel.isMember(targetFd))
	{
		sendNumeric(fd, ERR_USERNOTINCHANNEL, nick + " " + name + " :They aren't on that channel");
		return ;
	}

	channel.broadcast(_clients, userPrefix(fd) + " KICK " + name + " " + nick + " :" + reason, -1);
	channel.removeMember(targetFd);
	if (channel.isEmpty())
		_channels.erase(name);
}

void Server::handleTopicCommand(int fd, const std::vector<std::string> &params)
{
	if (params.empty())
	{
		sendNumeric(fd, ERR_NEEDMOREPARAMS, "TOPIC :Not enough parameters");
		return ;
	}

	std::string	name = params[0];

	if (_channels.find(name) == _channels.end())
	{
		sendNumeric(fd, ERR_NOSUCHCHANNEL, name + " :No such channel");
		return ;
	}

	Channel	&channel = _channels[name];

	if (!channel.isMember(fd))
	{
		sendNumeric(fd, ERR_NOTONCHANNEL, name + " :You're not on that channel");
		return ;
	}

	if (params.size() < 2)
	{
		if (channel.getTopic().empty())
			sendNumeric(fd, RPL_NOTOPIC, name + " :No topic is set");
		else
			sendNumeric(fd, RPL_TOPIC, name + " :" + channel.getTopic());
		return ;
	}

	if (channel.isTopicLocked() && !channel.isOperator(fd))
	{
		sendNumeric(fd, ERR_CHANOPRIVSNEEDED, name + " :You're not channel operator");
		return ;
	}

	channel.setTopic(params[1]);
	channel.broadcast(_clients, userPrefix(fd) + " TOPIC " + name + " :" + params[1], -1);
}

void Server::handleModeCommand(int fd, const std::vector<std::string> &params)
{
	if (params.empty())
	{
		sendNumeric(fd, ERR_NEEDMOREPARAMS, "MODE :Not enough parameters");
		return ;
	}

	std::string	name = params[0];

	if (_channels.find(name) == _channels.end())
	{
		sendNumeric(fd, ERR_NOSUCHCHANNEL, name + " :No such channel");
		return ;
	}

	Channel	&channel = _channels[name];

	if (params.size() < 2)
	{
		sendNumeric(fd, RPL_CHANNELMODEIS, name + " " + channel.getModeString());
		return ;
	}
	if (!channel.isMember(fd))
	{
		sendNumeric(fd, ERR_NOTONCHANNEL, name + " :You're not on that channel");
		return ;
	}
	if (!channel.isOperator(fd))
	{
		sendNumeric(fd, ERR_CHANOPRIVSNEEDED, name + " :You're not channel operator");
		return ;
	}

	std::string	modes = params[1];
	size_t		argIndex = 2;
	bool		adding = true;
	std::string	appliedModes = "";
	std::string	appliedArgs = "";
	char		lastSign = 0;

	for (size_t i = 0; i < modes.size(); i++)
	{
		char	c = modes[i];

		if (c == '+')
		{
			adding = true;
			continue ;
		}
		if (c == '-')
		{
			adding = false;
			continue ;
		}

		if (c == 'i')
		{
			channel.setInviteOnly(adding);
		}
		else if (c == 't')
		{
			channel.setTopicLocked(adding);
		}
		else if (c == 'k')
		{
			if (adding)
			{
				if (argIndex >= params.size())
				{
					sendNumeric(fd, ERR_NEEDMOREPARAMS, "MODE :Not enough parameters");
					continue ;
				}
				channel.setKey(params[argIndex]);
				appliedArgs += " " + params[argIndex];
				argIndex++;
			}
			else
				channel.unsetKey();
		}
		else if (c == 'o')
		{
			if (argIndex >= params.size())
			{
				sendNumeric(fd, ERR_NEEDMOREPARAMS, "MODE :Not enough parameters");
				continue ;
			}
			int	targetFd = getFdByNick(params[argIndex]);

			if (targetFd == -1 || !channel.isMember(targetFd))
			{
				sendNumeric(fd, ERR_USERNOTINCHANNEL, params[argIndex] + " " + name + " :They aren't on that channel");
				argIndex++;
				continue ;
			}
			if (adding)
				channel.addOperator(targetFd);
			else
				channel.removeOperator(targetFd);
			appliedArgs += " " + params[argIndex];
			argIndex++;
		}
		else if (c == 'l')
		{
			if (adding)
			{
				if (argIndex >= params.size())
				{
					sendNumeric(fd, ERR_NEEDMOREPARAMS, "MODE :Not enough parameters");
					continue ;
				}
				int	limit = std::atoi(params[argIndex].c_str());

				if (limit <= 0)
				{
					argIndex++;
					continue ;
				}
				channel.setLimit(static_cast<size_t>(limit));
				appliedArgs += " " + params[argIndex];
				argIndex++;
			}
			else
				channel.setLimit(0);
		}
		else
		{
			sendNumeric(fd, ERR_UNKNOWNMODE, std::string(1, c) + " :is unknown mode char to me");
			continue ;
		}

		char	sign = adding ? '+' : '-';

		if (sign != lastSign)
		{
			appliedModes += sign;
			lastSign = sign;
		}
		appliedModes += c;
	}

	if (!appliedModes.empty())
		channel.broadcast(_clients, userPrefix(fd) + " MODE " + name + " " + appliedModes + appliedArgs, -1);
}
