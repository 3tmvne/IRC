#include "Server.hpp"

void Server::handlepingcommand(int fd, const std::vector<std::string> &params)
{
	std::string	token = params.empty() ? serverName : params[0];

	clients[fd].appendout(":" + serverName + " PONG " + serverName + " :" + token + "\r\n");
}

void Server::handlequitcommand(int fd, const std::vector<std::string> &params)
{
	std::string									reason = params.empty() ? "Client Quit" : params[0];
	std::vector<std::string>					names;
	std::map<std::string, Channel>::iterator	it;

	
	
	broadcasttopeers(fd, userprefix(fd) + " QUIT :" + reason, false);

	
	
	for (it = channels.begin(); it != channels.end(); ++it)
	{
		if (it->second.ismember(fd))
			names.push_back(it->first);
	}
	for (size_t i = 0; i < names.size(); i++)
		removefromchannel(fd, names[i]);

	clients[fd].appendout("ERROR :Closing Link: " + clients[fd].gethostname()
		+ " (" + reason + ")\r\n");
	clients[fd].setquitting(true);
}

void Server::delivermessage(int fd, const std::vector<std::string> &params,
	const std::string &verb, bool quiet)
{
	if (params.empty())
	{
		if (!quiet)
			sendnumeric(fd, ERR_NORECIPIENT, ":No recipient given (" + verb + ")");
		return ;
	}
	if (params.size() < 2 || params[1].empty())
	{
		if (!quiet)
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
				if (!quiet)
					sendnumeric(fd, ERR_NOSUCHCHANNEL, target + " :No such channel");
				continue ;
			}
			
			
			if (!channel->second.ismember(fd))
			{
				if (!quiet)
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
				if (!quiet)
					sendnumeric(fd, ERR_NOSUCHNICK, target + " :No such nick/channel");
				continue ;
			}
			clients[targetFd].appendout(line + "\r\n");
		}
	}
}

void Server::handleprivmsgcommand(int fd, const std::vector<std::string> &params)
{
	delivermessage(fd, params, "PRIVMSG", false);
}

void Server::handlenoticecommand(int fd, const std::vector<std::string> &params)
{
	delivermessage(fd, params, "NOTICE", true);
}

void Server::handlepartcommand(int fd, const std::vector<std::string> &params)
{
	if (params.empty())
	{
		sendnumeric(fd, ERR_NEEDMOREPARAMS, "PART :Not enough parameters");
		return ;
	}

	std::vector<std::string>	names = splitlist(params[0], ',');
	std::string					reason = "";

	if (params.size() > 1)
		reason = params[1];

	for (size_t i = 0; i < names.size(); i++)
	{
		std::map<std::string, Channel>::iterator	channel = channels.find(names[i]);

		if (channel == channels.end())
		{
			sendnumeric(fd, ERR_NOSUCHCHANNEL, names[i] + " :No such channel");
			continue ;
		}
		if (!channel->second.ismember(fd))
		{
			sendnumeric(fd, ERR_NOTONCHANNEL, names[i] + " :You're not on that channel");
			continue ;
		}

		std::string	line = userprefix(fd) + " PART " + names[i];

		if (!reason.empty())
			line += " :" + reason;
		
		
		channel->second.broadcast(clients, line, -1);
		removefromchannel(fd, names[i]);
	}
}
