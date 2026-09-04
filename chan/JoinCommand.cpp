#include "Server.hpp"

static std::vector<std::string> splitList(const std::string &str, char sep)
{
	std::vector<std::string> result;
	std::stringstream ss(str);
	std::string item;

	while (std::getline(ss, item, sep))
		result.push_back(item);
	return result;
}

void Server::handleJoinCommand(int fd, const std::vector<std::string> &params)
{
	if (params.empty())
	{
		sendNumeric(fd, "461", "JOIN :Not enough parameters");
		return;
	}

	std::vector<std::string> names = splitList(params[0], ',');
	std::vector<std::string> keys;

	if (params.size() > 1)
		keys = splitList(params[1], ',');

	for (size_t i = 0; i < names.size(); i++)
	{
		std::string name = names[i];
		std::string key = "";

		if (i < keys.size())
			key = keys[i];

		if (name.empty() || (name[0] != '#' && name[0] != '&'))
		{
			sendNumeric(fd, "476", name + " :Bad Channel Mask");
			continue;
		}

		if (_channels.find(name) == _channels.end())
		{
			_channels[name] = Channel(name);
			_channels[name].addMember(fd);
			_channels[name].addOperator(fd);
		}
		else
		{
			Channel &target = _channels[name];

			if (target.isMember(fd))
				continue;
			if (target.isInviteOnly() && !target.isInvited(fd))
			{
				sendNumeric(fd, "473", name + " :Cannot join channel (+i)");
				continue;
			}
			if (target.hasKey() && target.getKey() != key)
			{
				sendNumeric(fd, "475", name + " :Cannot join channel (+k)");
				continue;
			}
			if (target.isFull())
			{
				sendNumeric(fd, "471", name + " :Cannot join channel (+l)");
				continue;
			}
			target.addMember(fd);
			target.removeInvite(fd);
		}

		Channel &channel = _channels[name];
		channel.broadcast(_clients, userPrefix(fd) + " JOIN " + name, -1);
		if (channel.getTopic().empty())
			sendNumeric(fd, "331", name + " :No topic is set");
		else
			sendNumeric(fd, "332", name + " :" + channel.getTopic());
		sendNumeric(fd, "353", "= " + name + " :" + buildNamesList(channel));
		sendNumeric(fd, "366", name + " :End of /NAMES list");
	}
}
