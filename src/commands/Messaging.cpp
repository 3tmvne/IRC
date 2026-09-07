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
// Phase 1 wires setQuitting() into the poll loop so the ERROR line is flushed
// before the socket closes, and Phase 3 broadcasts the quit to shared channels.
void Server::handleQuitCommand(int fd, const std::vector<std::string> &params)
{
	std::string	reason = params.empty() ? "Client Quit" : params[0];

	_clients[fd].appendOut("ERROR :Closing Link: " + _clients[fd].getHostname()
		+ " (" + reason + ")\r\n");
	_clients[fd].setQuitting(true);
}
