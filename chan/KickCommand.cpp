#include "Server.hpp"

void Server::handleKickCommand(int fd, const std::vector<std::string> &params)
{
	if (params.size() < 2)
	{
		sendNumeric(fd, "461", "KICK :Not enough parameters");
		return;
	}

	std::string name = params[0];
	std::string nick = params[1];
	std::string reason = _clients[fd].getNickname();

	if (params.size() > 2)
		reason = params[2];

	if (_channels.find(name) == _channels.end())
	{
		sendNumeric(fd, "403", name + " :No such channel");
		return;
	}

	Channel &channel = _channels[name];

	if (!channel.isMember(fd))
	{
		sendNumeric(fd, "442", name + " :You're not on that channel");
		return;
	}
	if (!channel.isOperator(fd))
	{
		sendNumeric(fd, "482", name + " :You're not channel operator");
		return;
	}

	int targetFd = getFdByNick(nick);
	if (targetFd == -1 || !channel.isMember(targetFd))
	{
		sendNumeric(fd, "441", nick + " " + name + " :They aren't on that channel");
		return;
	}

	channel.broadcast(_clients, userPrefix(fd) + " KICK " + name + " " + nick + " :" + reason, -1);
	channel.removeMember(targetFd);
	if (channel.isEmpty())
		_channels.erase(name);
}
