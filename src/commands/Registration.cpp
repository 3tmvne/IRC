#include "Server.hpp"

// PASS <password>
// Must arrive before the connection is registered. Phase 2 completes this
// lane with NICK and USER and the 001-004 welcome burst.
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
