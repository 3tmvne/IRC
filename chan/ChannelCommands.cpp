#include "Server.hpp"
#include <sstream>

static std::vector<std::string> splitList(const std::string &str, char sep)
{
	std::vector<std::string> result;
	std::stringstream ss(str);
	std::string item;

	while (std::getline(ss, item, sep))
		result.push_back(item);
	return result;
}

std::string Server::userPrefix(int fd)
{
	Client &client = _clients[fd];
	return ":" + client.getNickname() + "!" + client.getUsername() + "@localhost";
}

void Server::sendNumeric(int fd, const std::string &code, const std::string &args)
{
	_clients[fd].appendOut(":" + _serverName + " " + code + " " + _clients[fd].getNickname() + " " + args + "\r\n");
}

int Server::getFdByNick(const std::string &nick)
{
	std::map<int, Client>::iterator it;

	for (it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (it->second.getNickname() == nick)
			return it->first;
	}
	return -1;
}

std::string Server::buildNamesList(Channel &channel)
{
	std::string list = "";
	const std::vector<int> &members = channel.getMembers();

	for (size_t i = 0; i < members.size(); i++)
	{
		if (i > 0)
			list += " ";
		if (channel.isOperator(members[i]))
			list += "@";
		list += _clients[members[i]].getNickname();
	}
	return list;
}

void Server::removeFromChannel(int fd, const std::string &name)
{
	std::map<std::string, Channel>::iterator it = _channels.find(name);

	if (it == _channels.end())
		return;
	it->second.removeMember(fd);
	if (it->second.isEmpty())
		_channels.erase(it);
}
