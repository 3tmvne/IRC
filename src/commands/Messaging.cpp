#include "Server.hpp"

void Server::handlepingcommand(int fd, const std::vector<std::string> &params)
{
	std::string	token = params.empty() ? serverName : params[0];

	clients[fd].appendout(":" + serverName + " PONG " + serverName + " :" + token + "\r\n");
}

void Server::delivermessage(int fd, const std::vector<std::string> &params,
	const std::string &verb)
{
	if (params.empty())
	{
		sendnumeric(fd, ERR_NORECIPIENT, ":No recipient given (" + verb + ")");
		return ;
	}
	if (params.size() < 2 || params[1].empty())
	{
		sendnumeric(fd, ERR_NOTEXTTOSEND, ":No text to send");
		return ;
	}

	std::vector<std::string>	targets = splitlist(params[0], ',');
	std::string					text = params[1];

	for (size_t i = 0; i < targets.size(); i++)
	{
		std::string	target = targets[i];

		if (target.empty())
			continue ;

		std::string	line = userprefix(fd) + " " + verb + " " + target + " :" + text;

		if (target[0] == '#' || target[0] == '&')
		{
			std::map<std::string, Channel>::iterator	channel = channels.find(target);

			if (channel == channels.end())
			{
				sendnumeric(fd, ERR_NOSUCHCHANNEL, target + " :No such channel");
				continue ;
			}
			if (!channel->second.ismember(fd))
			{
				sendnumeric(fd, ERR_CANNOTSENDTOCHAN, target + " :Cannot send to channel");
				continue ;
			}
			channel->second.broadcast(clients, line, fd);
		}
		else
		{
			int	targetFd = getfdbynick(target);

			if (targetFd == -1)
			{
				sendnumeric(fd, ERR_NOSUCHNICK, target + " :No such nick/channel");
				continue ;
			}
			clients[targetFd].appendout(line + "\r\n");
		}
	}
}

void Server::handleprivmsgcommand(int fd, const std::vector<std::string> &params)
{
	delivermessage(fd, params, "PRIVMSG");
}
