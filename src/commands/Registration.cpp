#include "Server.hpp"

// RFC 2812 counts these punctuation marks as letters in a nickname.
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

// nickname = ( letter / special ) *8( letter / digit / special / "-" )
// A digit may not lead, otherwise a nick could be mistaken for a numeric.
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

// Nicknames are case-insensitive, and RFC 1459 additionally folds "[]\" onto
// "{}|" -- they are the same letters in the Scandinavian alphabet it was
// written for. Comparing this way is what keeps every other lookup in the
// server free to use a plain ==: no two clients can hold equivalent nicks.
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

// PASS <password>
// Must arrive before the connection is registered; it only records the
// verdict, and completeRegistration() is what acts on it.
void Server::handlePassCommand(int fd, const std::vector<std::string> &params)
{
	Client	&client = _clients[fd];

	if (client.isRegistered())
	{
		sendNumeric(fd, ERR_ALREADYREGISTRED, ":You may not reregister");
		return ;
	}
	if (params.empty())
	{
		sendNumeric(fd, ERR_NEEDMOREPARAMS, "PASS :Not enough parameters");
		return ;
	}
	if (params[0] != _password)
	{
		sendNumeric(fd, ERR_PASSWDMISMATCH, ":Password incorrect");
		return ;
	}
	client.setPass(true);
}

// NICK <nickname>
// Valid both during the handshake and afterwards, when it renames a live user.
void Server::handleNickCommand(int fd, const std::vector<std::string> &params)
{
	Client	&client = _clients[fd];

	if (params.empty() || params[0].empty())
	{
		sendNumeric(fd, ERR_NONICKNAMEGIVEN, ":No nickname given");
		return ;
	}

	std::string						nick = params[0];
	std::map<int, Client>::iterator	it;

	if (!isValidNickname(nick))
	{
		sendNumeric(fd, ERR_ERRONEUSNICKNAME, nick + " :Erroneous nickname");
		return ;
	}
	for (it = _clients.begin(); it != _clients.end(); ++it)
	{
		// Re-sending your own nick, or only changing its case, is not a clash.
		if (it->first != fd && sameNick(it->second.getNickname(), nick))
		{
			sendNumeric(fd, ERR_NICKNAMEINUSE, nick + " :Nickname is already in use");
			return ;
		}
	}

	// Announced with the OLD prefix so peers can match the change against the
	// nick they currently know; the sender is included to confirm the rename.
	if (client.isRegistered())
		broadcastToPeers(fd, userPrefix(fd) + " NICK :" + nick, true);

	client.setNickname(nick);
	completeRegistration(fd);
}

// USER <username> <hostname> <servername> :<realname>
// The hostname the client claims is deliberately dropped: letting a client
// choose how it appears to others would make every prefix unverifiable.
void Server::handleUserCommand(int fd, const std::vector<std::string> &params)
{
	Client	&client = _clients[fd];

	if (client.isRegistered())
	{
		sendNumeric(fd, ERR_ALREADYREGISTRED, ":You may not reregister");
		return ;
	}
	if (params.size() < 4 || params[0].empty())
	{
		sendNumeric(fd, ERR_NEEDMOREPARAMS, "USER :Not enough parameters");
		return ;
	}

	client.setUsername(params[0]);
	client.setRealname(params[3]);
	completeRegistration(fd);
}

// PASS, NICK and USER may arrive in any order, so this runs after each of them
// and does nothing until the last piece lands.
void Server::completeRegistration(int fd)
{
	Client	&client = _clients[fd];

	if (client.isRegistered())
		return ;
	if (client.getNickname().empty() || client.getUsername().empty())
		return ;
	if (!client.hasPass())
	{
		// The identity is complete but the password never was, so the
		// handshake can no longer succeed: refuse the link instead of
		// leaving a half-open connection sitting in the poll set forever.
		sendNumeric(fd, ERR_PASSWDMISMATCH, ":Password incorrect");
		client.appendOut("ERROR :Closing Link: " + client.getHostname()
			+ " (Bad password)\r\n");
		client.setQuitting(true);
		return ;
	}

	client.setRegistered(true);

	// The 001-004 burst is what tells a client the session is usable; most
	// clients stay stuck on "connecting" until 001 arrives, and read the
	// supported modes out of 004/005 before they offer them in their UI.
	sendNumeric(fd, RPL_WELCOME, ":Welcome to the " + _serverName + " Network, "
		+ client.getNickname() + "!" + client.getUsername() + "@" + client.getHostname());
	sendNumeric(fd, RPL_YOURHOST, ":Your host is " + _serverName + ", running version 1.0");
	sendNumeric(fd, RPL_CREATED, ":This server was created " __DATE__ " " __TIME__);
	sendNumeric(fd, RPL_MYINFO, _serverName + " 1.0 o itkol");
	sendNumeric(fd, RPL_ISUPPORT, "CHANTYPES=#& CHANMODES=,k,l,it PREFIX=(o)@ NICKLEN=9"
		" :are supported by this server");
	sendNumeric(fd, RPL_MOTDSTART, ":- " + _serverName + " Message of the Day -");
	sendNumeric(fd, RPL_MOTD, ":- Commands: JOIN PART PRIVMSG NOTICE TOPIC MODE KICK INVITE");
	sendNumeric(fd, RPL_MOTD, ":- Channel modes: +i +t +k +l +o");
	sendNumeric(fd, RPL_ENDOFMOTD, ":End of /MOTD command");

	std::cout << "client registered: " << client.getNickname()
		<< " (fd " << fd << ")" << std::endl;
}
