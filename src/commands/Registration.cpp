#include "Server.hpp"

static bool	isSpecial(char c)
{
	return (c == '[' || c == ']' || c == '\\' || c == '`'
		|| c == '_' || c == '^' || c == '{' || c == '|' || c == '}');
}

static bool	isLetter(char c)
{
	return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

static bool	isDigit(char c)
{
	return (c >= '0' && c <= '9');
}

static bool	isValidNickname(const std::string &nick)
{
	if (nick.empty() || nick.size() > 9)
		return (false);
	if (!isLetter(nick[0]) && !isSpecial(nick[0]))
		return (false);
	for (size_t i = 1; i < nick.size(); i++)
	{
		if (!isLetter(nick[i]) && !isDigit(nick[i])
			&& !isSpecial(nick[i]) && nick[i] != '-')
			return (false);
	}
	return (true);
}

static char	ircLower(char c)
{
	if (c >= 'A' && c <= 'Z')
		return (static_cast<char>(c + 32));
	if (c == '[')
		return ('{');
	if (c == ']')
		return ('}');
	if (c == '\\')
		return ('|');
	return (c);
}

static bool	sameNick(const std::string &left, const std::string &right)
{
	if (left.size() != right.size())
		return (false);
	for (size_t i = 0; i < left.size(); i++)
	{
		if (ircLower(left[i]) != ircLower(right[i]))
			return (false);
	}
	return (true);
}

void Server::handlepasscommand(int fd, const std::vector<std::string> &params)
{
	Client	&client = clients[fd];

	if (client.isregistered())
	{
		sendnumeric(fd, ERR_ALREADYREGISTRED, ":You may not reregister");
		return ;
	}
	if (params.empty())
	{
		sendnumeric(fd, ERR_NEEDMOREPARAMS, "PASS :Not enough parameters");
		return ;
	}
	if (params[0] != password)
	{
		sendnumeric(fd, ERR_PASSWDMISMATCH, ":Password incorrect");
		return ;
	}
	client.setpass(true);
}

void Server::handlenickcommand(int fd, const std::vector<std::string> &params)
{
	Client	&client = clients[fd];

	if (params.empty() || params[0].empty())
	{
		sendnumeric(fd, ERR_NONICKNAMEGIVEN, ":No nickname given");
		return ;
	}

	std::string						nick = params[0];
	std::map<int, Client>::iterator	it;

	if (!isValidNickname(nick))
	{
		sendnumeric(fd, ERR_ERRONEUSNICKNAME, nick + " :Erroneous nickname");
		return ;
	}
	for (it = clients.begin(); it != clients.end(); ++it)
	{
		
		if (it->first != fd && sameNick(it->second.getnickname(), nick))
		{
			sendnumeric(fd, ERR_NICKNAMEINUSE, nick + " :Nickname is already in use");
			return ;
		}
	}

	
	
	if (client.isregistered())
		broadcasttopeers(fd, userprefix(fd) + " NICK :" + nick, true);

	client.setnickname(nick);
	completeregistration(fd);
}

void Server::handleusercommand(int fd, const std::vector<std::string> &params)
{
	Client	&client = clients[fd];

	if (client.isregistered())
	{
		sendnumeric(fd, ERR_ALREADYREGISTRED, ":You may not reregister");
		return ;
	}
	if (params.size() < 4 || params[0].empty())
	{
		sendnumeric(fd, ERR_NEEDMOREPARAMS, "USER :Not enough parameters");
		return ;
	}

	client.setusername(params[0]);
	client.setrealname(params[3]);
	completeregistration(fd);
}

void Server::completeregistration(int fd)
{
	Client	&client = clients[fd];

	if (client.isregistered())
		return ;
	if (client.getnickname().empty() || client.getusername().empty())
		return ;
	if (!client.haspass())
	{
		
		
		
		sendnumeric(fd, ERR_PASSWDMISMATCH, ":Password incorrect");
		client.appendout("ERROR :Closing Link: " + client.gethostname()
			+ " (Bad password)\r\n");
		client.setquitting(true);
		return ;
	}

	client.setregistered(true);

	
	
	
	sendnumeric(fd, RPL_WELCOME, ":Welcome to the " + serverName + " Network, "
		+ client.getnickname() + "!" + client.getusername() + "@" + client.gethostname());
	sendnumeric(fd, RPL_YOURHOST, ":Your host is " + serverName + ", running version 1.0");
	sendnumeric(fd, RPL_CREATED, ":This server was created " __DATE__ " " __TIME__);
	sendnumeric(fd, RPL_MYINFO, serverName + " 1.0 o itkol");
	sendnumeric(fd, RPL_ISUPPORT, "CHANTYPES=#& CHANMODES=,k,l,it PREFIX=(o)@ NICKLEN=9"
		" :are supported by this server");
	sendnumeric(fd, RPL_MOTDSTART, ":- " + serverName + " Message of the Day -");
	sendnumeric(fd, RPL_MOTD, ":- Commands: JOIN PART PRIVMSG NOTICE TOPIC MODE KICK INVITE");
	sendnumeric(fd, RPL_MOTD, ":- Channel modes: +i +t +k +l +o");
	sendnumeric(fd, RPL_ENDOFMOTD, ":End of /MOTD command");

	std::cout << "client registered: " << client.getnickname()
		<< " (fd " << fd << ")" << std::endl;
}
