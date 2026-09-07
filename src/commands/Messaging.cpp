#include "Server.hpp"

// PING <token>  ->  PONG <token>
// Clients use this to prove the link is alive; failing to answer gets the
// server dropped from their connection.
void Server::handlePingCommand(int fd, const std::vector<std::string> &params)
{
	std::string	token = params.empty() ? _serverName : params[0];

	_clients[fd].appendOut(":" + _serverName + " PONG " + _serverName + " :" + token + "\r\n");
}

// QUIT [:reason]
// The socket is not closed here: setQuitting() lets the poll loop flush the
// ERROR line first, so the client sees why it was dropped.
void Server::handleQuitCommand(int fd, const std::vector<std::string> &params)
{
	std::string									reason = params.empty() ? "Client Quit" : params[0];
	std::vector<std::string>					names;
	std::map<std::string, Channel>::iterator	it;

	// Peers are told before the memberships go away -- once removeFromChannel
	// has run there is nobody left to address the QUIT to.
	broadcastToPeers(fd, userPrefix(fd) + " QUIT :" + reason, false);

	// Collect the names first, then erase: removeFromChannel drops a channel
	// that just became empty, which would invalidate a live _channels iterator.
	for (it = _channels.begin(); it != _channels.end(); ++it)
	{
		if (it->second.isMember(fd))
			names.push_back(it->first);
	}
	for (size_t i = 0; i < names.size(); i++)
		removeFromChannel(fd, names[i]);

	_clients[fd].appendOut("ERROR :Closing Link: " + _clients[fd].getHostname()
		+ " (" + reason + ")\r\n");
	_clients[fd].setQuitting(true);
}

// PRIVMSG and NOTICE carry the same payload and differ in exactly one rule:
// a NOTICE must never provoke an automatic reply, or two bots answering each
// other's errors would loop forever. Hence `quiet`, which mutes every numeric.
void Server::deliverMessage(int fd, const std::vector<std::string> &params,
	const std::string &verb, bool quiet)
{
	if (params.empty())
	{
		if (!quiet)
			sendNumeric(fd, ERR_NORECIPIENT, ":No recipient given (" + verb + ")");
		return ;
	}
	if (params.size() < 2 || params[1].empty())
	{
		if (!quiet)
			sendNumeric(fd, ERR_NOTEXTTOSEND, ":No text to send");
		return ;
	}

	std::vector<std::string>	targets = splitList(params[0], ',');
	std::string					text = params[1];

	for (size_t i = 0; i < targets.size(); i++)
	{
		std::string	target = targets[i];

		if (target.empty())
			continue ;

		std::string	line = userPrefix(fd) + " " + verb + " " + target + " :" + text;

		if (target[0] == '#' || target[0] == '&')
		{
			std::map<std::string, Channel>::iterator	channel = _channels.find(target);

			if (channel == _channels.end())
			{
				if (!quiet)
					sendNumeric(fd, ERR_NOSUCHCHANNEL, target + " :No such channel");
				continue ;
			}
			// Only members may speak, otherwise anyone could shout into any
			// channel without ever joining it -- and without being kickable.
			if (!channel->second.isMember(fd))
			{
				if (!quiet)
					sendNumeric(fd, ERR_CANNOTSENDTOCHAN, target + " :Cannot send to channel");
				continue ;
			}
			// The sender is excluded: its own client already displayed the
			// line when it typed it, and echoing it back would double it.
			channel->second.broadcast(_clients, line, fd);
		}
		else
		{
			int	targetFd = getFdByNick(target);

			if (targetFd == -1)
			{
				if (!quiet)
					sendNumeric(fd, ERR_NOSUCHNICK, target + " :No such nick/channel");
				continue ;
			}
			_clients[targetFd].appendOut(line + "\r\n");
		}
	}
}

// PRIVMSG <target>{,<target>} :<text>
void Server::handlePrivmsgCommand(int fd, const std::vector<std::string> &params)
{
	deliverMessage(fd, params, "PRIVMSG", false);
}

// NOTICE <target>{,<target>} :<text>
void Server::handleNoticeCommand(int fd, const std::vector<std::string> &params)
{
	deliverMessage(fd, params, "NOTICE", true);
}

// PART <channel>{,<channel>} [:reason]
void Server::handlePartCommand(int fd, const std::vector<std::string> &params)
{
	if (params.empty())
	{
		sendNumeric(fd, ERR_NEEDMOREPARAMS, "PART :Not enough parameters");
		return ;
	}

	std::vector<std::string>	names = splitList(params[0], ',');
	std::string					reason = "";

	if (params.size() > 1)
		reason = params[1];

	for (size_t i = 0; i < names.size(); i++)
	{
		std::map<std::string, Channel>::iterator	channel = _channels.find(names[i]);

		if (channel == _channels.end())
		{
			sendNumeric(fd, ERR_NOSUCHCHANNEL, names[i] + " :No such channel");
			continue ;
		}
		if (!channel->second.isMember(fd))
		{
			sendNumeric(fd, ERR_NOTONCHANNEL, names[i] + " :You're not on that channel");
			continue ;
		}

		std::string	line = userPrefix(fd) + " PART " + names[i];

		if (!reason.empty())
			line += " :" + reason;
		// Broadcast before the removal, exactly like KICK: the leaver has to
		// see its own PART echoed back or its client stays in the channel.
		channel->second.broadcast(_clients, line, -1);
		removeFromChannel(fd, names[i]);
	}
}
