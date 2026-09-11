#include "Server.hpp"
#include <algorithm>
#include <cstdlib>
#include <sstream>

std::vector<std::string> Server::splitlist(const std::string &str, char sep)
{
	std::vector<std::string>	result;
	std::stringstream			ss(str);
	std::string					item;

	while (std::getline(ss, item, sep))
		result.push_back(item);
	return (result);
}

std::string Server::userprefix(int fd)
{
	return (clients[fd].prefix());
}

void Server::sendnumeric(int fd, const std::string &code, const std::string &args)
{
	std::string	nick = clients[fd].getnickname();
	if (nick.empty())
		nick = "*";
	clients[fd].appendout(":" + serverName + " " + code + " " + nick + " " + args + "\r\n");
}

int Server::getfdbynick(const std::string &nick)
{
	std::map<int, Client>::iterator	it;

	for (it = clients.begin(); it != clients.end(); ++it)
	{
		if (it->second.getnickname() == nick)
			return (it->first);
	}
	return (-1);
}

std::string Server::buildnameslist(Channel &channel)
{
	std::string					list = "";
	const std::vector<int>		&members = channel.getmembers();

	for (size_t i = 0; i < members.size(); i++)
	{
		if (i > 0)
			list += " ";
		if (channel.isoperator(members[i]))
			list += "@";
		list += clients[members[i]].getnickname();
	}
	return (list);
}

void Server::removefromchannel(int fd, const std::string &name)
{
	std::map<std::string, Channel>::iterator	it = channels.find(name);

	if (it == channels.end())
		return ;
	it->second.removemember(fd);
	if (it->second.isempty())
		channels.erase(it);
}

void Server::broadcasttopeers(int fd, const std::string &message, bool includeSelf)
{
	std::vector<int>							targets;
	std::map<std::string, Channel>::iterator	it;

	for (it = channels.begin(); it != channels.end(); ++it)
	{
		if (!it->second.ismember(fd))
			continue ;

		const std::vector<int>	&members = it->second.getmembers();

		for (size_t i = 0; i < members.size(); i++)
		{
			if (members[i] != fd
				&& std::find(targets.begin(), targets.end(), members[i]) == targets.end())
				targets.push_back(members[i]);
		}
	}
	if (includeSelf)
		targets.push_back(fd);

	for (size_t i = 0; i < targets.size(); i++)
	{
		std::map<int, Client>::iterator	client = clients.find(targets[i]);

		if (client != clients.end())
			client->second.appendout(message + "\r\n");
	}
}

void Server::handlejoincommand(int fd, const std::vector<std::string> &params)
{
	if (params.empty())
	{
		sendnumeric(fd, ERR_NEEDMOREPARAMS, "JOIN :Not enough parameters");
		return ;
	}

	std::vector<std::string>	names = splitlist(params[0], ',');
	std::vector<std::string>	keys;

	if (params.size() > 1)
		keys = splitlist(params[1], ',');

	for (size_t i = 0; i < names.size(); i++)
	{
		std::string	name = names[i];
		std::string	key = "";

		if (i < keys.size())
			key = keys[i];

		if (name.empty() || (name[0] != '#' && name[0] != '&'))
		{
			sendnumeric(fd, ERR_BADCHANMASK, name + " :Bad Channel Mask");
			continue ;
		}

		if (channels.find(name) == channels.end())
		{
			channels[name] = Channel(name);
			channels[name].addmember(fd);
			channels[name].addoperator(fd);
		}
		else
		{
			Channel	&target = channels[name];

			if (target.ismember(fd))
				continue ;
			if (target.isinviteonly() && !target.isinvited(fd))
			{
				sendnumeric(fd, ERR_INVITEONLYCHAN, name + " :Cannot join channel (+i)");
				continue ;
			}
			if (target.haskey() && target.getkey() != key)
			{
				sendnumeric(fd, ERR_BADCHANNELKEY, name + " :Cannot join channel (+k)");
				continue ;
			}
			if (target.isfull())
			{
				sendnumeric(fd, ERR_CHANNELISFULL, name + " :Cannot join channel (+l)");
				continue ;
			}
			target.addmember(fd);
			target.removeinvite(fd);
		}

		Channel	&channel = channels[name];

		channel.broadcast(clients, userprefix(fd) + " JOIN " + name, -1);
		if (channel.gettopic().empty())
			sendnumeric(fd, RPL_NOTOPIC, name + " :No topic is set");
		else
			sendnumeric(fd, RPL_TOPIC, name + " :" + channel.gettopic());
		sendnumeric(fd, RPL_NAMREPLY, "= " + name + " :" + buildnameslist(channel));
		sendnumeric(fd, RPL_ENDOFNAMES, name + " :End of /NAMES list");
	}
}

void Server::handleinvitecommand(int fd, const std::vector<std::string> &params)
{
	if (params.size() < 2)
	{
		sendnumeric(fd, ERR_NEEDMOREPARAMS, "INVITE :Not enough parameters");
		return ;
	}

	std::string	nick = params[0];
	std::string	name = params[1];

	if (channels.find(name) == channels.end())
	{
		sendnumeric(fd, ERR_NOSUCHCHANNEL, name + " :No such channel");
		return ;
	}

	Channel	&channel = channels[name];

	if (!channel.ismember(fd))
	{
		sendnumeric(fd, ERR_NOTONCHANNEL, name + " :You're not on that channel");
		return ;
	}
	if (!channel.isoperator(fd))
	{
		sendnumeric(fd, ERR_CHANOPRIVSNEEDED, name + " :You're not channel operator");
		return ;
	}

	int	targetFd = getfdbynick(nick);

	if (targetFd == -1)
	{
		sendnumeric(fd, ERR_NOSUCHNICK, nick + " :No such nick/channel");
		return ;
	}
	if (channel.ismember(targetFd))
	{
		sendnumeric(fd, ERR_USERONCHANNEL, nick + " " + name + " :is already on channel");
		return ;
	}

	channel.addinvite(targetFd);
	sendnumeric(fd, RPL_INVITING, nick + " " + name);
	clients[targetFd].appendout(userprefix(fd) + " INVITE " + nick + " :" + name + "\r\n");
}

void Server::handlekickcommand(int fd, const std::vector<std::string> &params)
{
	if (params.size() < 2)
	{
		sendnumeric(fd, ERR_NEEDMOREPARAMS, "KICK :Not enough parameters");
		return ;
	}

	std::string	name = params[0];
	std::string	nick = params[1];
	std::string	reason = clients[fd].getnickname();

	if (params.size() > 2)
		reason = params[2];

	if (channels.find(name) == channels.end())
	{
		sendnumeric(fd, ERR_NOSUCHCHANNEL, name + " :No such channel");
		return ;
	}

	Channel	&channel = channels[name];

	if (!channel.ismember(fd))
	{
		sendnumeric(fd, ERR_NOTONCHANNEL, name + " :You're not on that channel");
		return ;
	}
	if (!channel.isoperator(fd))
	{
		sendnumeric(fd, ERR_CHANOPRIVSNEEDED, name + " :You're not channel operator");
		return ;
	}

	int	targetFd = getfdbynick(nick);

	if (targetFd == -1 || !channel.ismember(targetFd))
	{
		sendnumeric(fd, ERR_USERNOTINCHANNEL, nick + " " + name + " :They aren't on that channel");
		return ;
	}

	channel.broadcast(clients, userprefix(fd) + " KICK " + name + " " + nick + " :" + reason, -1);
	channel.removemember(targetFd);
	if (channel.isempty())
		channels.erase(name);
}

void Server::handletopiccommand(int fd, const std::vector<std::string> &params)
{
	if (params.empty())
	{
		sendnumeric(fd, ERR_NEEDMOREPARAMS, "TOPIC :Not enough parameters");
		return ;
	}

	std::string	name = params[0];

	if (channels.find(name) == channels.end())
	{
		sendnumeric(fd, ERR_NOSUCHCHANNEL, name + " :No such channel");
		return ;
	}

	Channel	&channel = channels[name];

	if (!channel.ismember(fd))
	{
		sendnumeric(fd, ERR_NOTONCHANNEL, name + " :You're not on that channel");
		return ;
	}

	if (params.size() < 2)
	{
		if (channel.gettopic().empty())
			sendnumeric(fd, RPL_NOTOPIC, name + " :No topic is set");
		else
			sendnumeric(fd, RPL_TOPIC, name + " :" + channel.gettopic());
		return ;
	}

	if (channel.istopiclocked() && !channel.isoperator(fd))
	{
		sendnumeric(fd, ERR_CHANOPRIVSNEEDED, name + " :You're not channel operator");
		return ;
	}

	channel.settopic(params[1]);
	channel.broadcast(clients, userprefix(fd) + " TOPIC " + name + " :" + params[1], -1);
}

void Server::handlemodecommand(int fd, const std::vector<std::string> &params)
{
	if (params.empty())
	{
		sendnumeric(fd, ERR_NEEDMOREPARAMS, "MODE :Not enough parameters");
		return ;
	}

	std::string	name = params[0];

	if (channels.find(name) == channels.end())
	{
		sendnumeric(fd, ERR_NOSUCHCHANNEL, name + " :No such channel");
		return ;
	}

	Channel	&channel = channels[name];

	if (params.size() < 2)
	{
		sendnumeric(fd, RPL_CHANNELMODEIS, name + " " + channel.getmodestring());
		return ;
	}
	if (!channel.ismember(fd))
	{
		sendnumeric(fd, ERR_NOTONCHANNEL, name + " :You're not on that channel");
		return ;
	}
	if (!channel.isoperator(fd))
	{
		sendnumeric(fd, ERR_CHANOPRIVSNEEDED, name + " :You're not channel operator");
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
			channel.setinviteonly(adding);
		}
		else if (c == 't')
		{
			channel.settopiclocked(adding);
		}
		else if (c == 'k')
		{
			if (adding)
			{
				if (argIndex >= params.size())
				{
					sendnumeric(fd, ERR_NEEDMOREPARAMS, "MODE :Not enough parameters");
					continue ;
				}
				channel.setkey(params[argIndex]);
				appliedArgs += " " + params[argIndex];
				argIndex++;
			}
			else
				channel.unsetkey();
		}
		else if (c == 'o')
		{
			if (argIndex >= params.size())
			{
				sendnumeric(fd, ERR_NEEDMOREPARAMS, "MODE :Not enough parameters");
				continue ;
			}
			int	targetFd = getfdbynick(params[argIndex]);

			if (targetFd == -1 || !channel.ismember(targetFd))
			{
				sendnumeric(fd, ERR_USERNOTINCHANNEL, params[argIndex] + " " + name + " :They aren't on that channel");
				argIndex++;
				continue ;
			}
			if (adding)
				channel.addoperator(targetFd);
			else
				channel.removeoperator(targetFd);
			appliedArgs += " " + params[argIndex];
			argIndex++;
		}
		else if (c == 'l')
		{
			if (adding)
			{
				if (argIndex >= params.size())
				{
					sendnumeric(fd, ERR_NEEDMOREPARAMS, "MODE :Not enough parameters");
					continue ;
				}
				int	limit = std::atoi(params[argIndex].c_str());

				if (limit <= 0)
				{
					argIndex++;
					continue ;
				}
				channel.setlimit(static_cast<size_t>(limit));
				appliedArgs += " " + params[argIndex];
				argIndex++;
			}
			else
				channel.setlimit(0);
		}
		else
		{
			sendnumeric(fd, ERR_UNKNOWNMODE, std::string(1, c) + " :is unknown mode char to me");
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
		channel.broadcast(clients, userprefix(fd) + " MODE " + name + " " + appliedModes + appliedArgs, -1);
}
