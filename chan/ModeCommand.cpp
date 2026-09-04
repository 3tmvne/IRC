#include "Server.hpp"
#include <cstdlib>

void Server::handleModeCommand(int fd, const std::vector<std::string> &params)
{
	if (params.empty())
	{
		sendNumeric(fd, "461", "MODE :Not enough parameters");
		return;
	}

	std::string name = params[0];
	if (_channels.find(name) == _channels.end())
	{
		sendNumeric(fd, "403", name + " :No such channel");
		return;
	}

	Channel &channel = _channels[name];
	if (params.size() < 2)
	{
		sendNumeric(fd, "324", name + " " + channel.getModeString());
		return;
	}
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

	std::string modes = params[1];
	size_t argIndex = 2;
	bool adding = true;
	std::string appliedModes = "";
	std::string appliedArgs = "";
	char lastSign = 0;

	for (size_t i = 0; i < modes.size(); i++)
	{
		char c = modes[i];

		if (c == '+')
		{
			adding = true;
			continue;
		}
		if (c == '-')
		{
			adding = false;
			continue;
		}

		if (c == 'i')
			channel.setInviteOnly(adding);
		else if (c == 't')
			channel.setTopicLocked(adding);
		else if (c == 'k')
		{
			if (adding)
			{
				if (argIndex >= params.size())
				{
					sendNumeric(fd, "461", "MODE :Not enough parameters");
					continue;
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
				sendNumeric(fd, "461", "MODE :Not enough parameters");
				continue;
			}
			int targetFd = getFdByNick(params[argIndex]);

			if (targetFd == -1 || !channel.isMember(targetFd))
			{
				sendNumeric(fd, "441", params[argIndex] + " " + name + " :They aren't on that channel");
				argIndex++;
				continue;
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
					sendNumeric(fd, "461", "MODE :Not enough parameters");
					continue;
				}
				int limit = std::atoi(params[argIndex].c_str());
				if (limit <= 0)
				{
					argIndex++;
					continue;
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
			sendNumeric(fd, "472", std::string(1, c) + " :is unknown mode char to me");
			continue;
		}

		char sign = adding ? '+' : '-';
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
