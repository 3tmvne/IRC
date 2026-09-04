#include "Server.hpp"

void Server::handleTopicCommand(int fd, const std::vector<std::string> &params)
{
	if (params.empty())
	{
		sendNumeric(fd, "461", "TOPIC :Not enough parameters");
		return;
	}

	std::string name = params[0];
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

	if (params.size() < 2)
	{
		if (channel.getTopic().empty())
			sendNumeric(fd, "331", name + " :No topic is set");
		else
			sendNumeric(fd, "332", name + " :" + channel.getTopic());
		return;
	}

	if (channel.isTopicLocked() && !channel.isOperator(fd))
	{
		sendNumeric(fd, "482", name + " :You're not channel operator");
		return;
	}

	channel.setTopic(params[1]);
	channel.broadcast(_clients, userPrefix(fd) + " TOPIC " + name + " :" + params[1], -1);
}
