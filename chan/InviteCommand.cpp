#include "Server.hpp"

void Server::handleInvitCommand(int fd, const std::vector<std::string> &params)
{
	if (params.size() < 2)
	{
		sendNumeric(fd, "461", "INVITE :Not enough parameters");
		return;
	}

	std::string nick = params[0];
	std::string name = params[1];

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
	if (targetFd == -1)
	{
		sendNumeric(fd, "401", nick + " :No such nick/channel");
		return;
	}
	if (channel.isMember(targetFd))
	{
		sendNumeric(fd, "443", nick + " " + name + " :is already on channel");
		return;
	}

	channel.addInvite(targetFd);
	sendNumeric(fd, "341", nick + " " + name);
	_clients[targetFd].appendOut(userPrefix(fd) + " INVITE " + nick + " :" + name + "\r\n");
}
