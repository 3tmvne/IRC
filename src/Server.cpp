#include "Server.hpp"

Server::Server(int port, const std::string &password)
	: _listenFd(-1), _port(port), _password(password), _serverName("ft_irc")
{
	initSocket();
}

Server::~Server()
{
	for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
		close(it->first);
	if (_listenFd != -1)
		close(_listenFd);
}

void Server::initSocket()
{
	_listenFd = socket(AF_INET, SOCK_STREAM, 0);
	if (_listenFd == -1)
		throw std::runtime_error("socket() failed");

	// Without SO_REUSEADDR the port stays in TIME_WAIT and a restart fails.
	int	option = 1;
	if (setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) == -1)
		throw std::runtime_error("setsockopt() failed");

	if (fcntl(_listenFd, F_SETFL, O_NONBLOCK) == -1)
		throw std::runtime_error("fcntl() failed");

	sockaddr_in	address;
	std::memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons(static_cast<uint16_t>(_port));
	address.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(_listenFd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == -1)
		throw std::runtime_error("bind() failed");
	if (listen(_listenFd, SOMAXCONN) == -1)
		throw std::runtime_error("listen() failed");

	pollfd	serverPoll;
	serverPoll.fd = _listenFd;
	serverPoll.events = POLLIN;
	serverPoll.revents = 0;
	_pollFds.push_back(serverPoll);
}

// Single poll() drives every socket in the process: the listener and every
// client, for both reading and writing. Nothing else touches a descriptor.
void Server::run()
{
	std::cout << "ft_irc listening on port " << _port << std::endl;

	while (!g_stop)
	{
		updatePollFds();

		int	result = poll(&_pollFds[0], _pollFds.size(), -1);
		if (result == -1)
		{
			if (errno == EINTR)
				continue ;
			break ;
		}

		for (size_t i = 0; i < _pollFds.size(); ++i)
		{
			if (_pollFds[i].revents == 0)
				continue ;

			if (_pollFds[i].fd == _listenFd && (_pollFds[i].revents & POLLIN))
			{
				acceptClient();
				continue ;
			}

			// Phase 1: POLLHUP is handled before POLLIN, which discards data
			// that arrived in the same event as the hangup. Reorder there.
			if (_pollFds[i].revents & (POLLHUP | POLLERR | POLLNVAL))
			{
				closeClient(i);
				--i;
				continue ;
			}

			if (_pollFds[i].revents & POLLIN)
			{
				size_t	oldSize = _pollFds.size();

				receiveData(i);
				if (_pollFds.size() < oldSize)
				{
					--i;
					continue ;
				}
			}

			if (i < _pollFds.size() && (_pollFds[i].revents & POLLOUT))
				sendData(i);
		}
	}
	std::cout << "shutting down" << std::endl;
}

void Server::acceptClient()
{
	int	clientFd = accept(_listenFd, NULL, NULL);

	if (clientFd == -1)
		return ;
	if (fcntl(clientFd, F_SETFL, O_NONBLOCK) == -1)
	{
		close(clientFd);
		return ;
	}

	pollfd	clientPoll;
	clientPoll.fd = clientFd;
	clientPoll.events = POLLIN;
	clientPoll.revents = 0;
	_pollFds.push_back(clientPoll);
	_clients.insert(std::make_pair(clientFd, Client(clientFd)));

	std::cout << "client connected: fd " << clientFd << std::endl;
}

void Server::receiveData(size_t index)
{
	int		fd = _pollFds[index].fd;
	char	buffer[512];

	ssize_t	bytes = recv(fd, buffer, sizeof(buffer), 0);

	// 0 means the peer closed cleanly, -1 means the connection is unusable.
	// Either way the only correct move is to drop the client.
	if (bytes <= 0)
	{
		closeClient(index);
		return ;
	}

	_clients[fd].appendIn(std::string(buffer, static_cast<size_t>(bytes)));
	processInput(fd);
}

// Drains every complete line that has arrived. A command split across several
// packets stays in the buffer until its newline shows up.
void Server::processInput(int fd)
{
	std::map<int, Client>::iterator	it = _clients.find(fd);
	std::string						line;

	if (it == _clients.end())
		return ;
	while (it->second.extractLine(line))
	{
		if (!line.empty())
			executeCommand(fd, _parser.parse(line));
		it = _clients.find(fd);
		if (it == _clients.end())
			return ;
	}
}

void Server::executeCommand(int fd, const Command &cmd)
{
	const std::string	&name = cmd.cmd;

	if (name.empty())
		return ;

	// Only the handshake itself is answered before registration completes.
	// Anything else would let an unauthenticated peer join channels and read
	// traffic without ever having presented the server password.
	if (!_clients[fd].isRegistered() && name != "PASS" && name != "NICK"
		&& name != "USER" && name != "QUIT" && name != "PING")
	{
		sendNumeric(fd, ERR_NOTREGISTERED, ":You have not registered");
		return ;
	}

	if (name == "PASS")
		handlePassCommand(fd, cmd.args);
	else if (name == "NICK")
		handleNickCommand(fd, cmd.args);
	else if (name == "USER")
		handleUserCommand(fd, cmd.args);
	else if (name == "PING")
		handlePingCommand(fd, cmd.args);
	else if (name == "QUIT")
		handleQuitCommand(fd, cmd.args);
	else if (name == "JOIN")
		handleJoinCommand(fd, cmd.args);
	else if (name == "INVITE")
		handleInvitCommand(fd, cmd.args);
	else if (name == "KICK")
		handleKickCommand(fd, cmd.args);
	else if (name == "TOPIC")
		handleTopicCommand(fd, cmd.args);
	else if (name == "MODE")
		handleModeCommand(fd, cmd.args);
	else if (name == "PRIVMSG")
		handlePrivmsgCommand(fd, cmd.args);
	else if (name == "NOTICE")
		handleNoticeCommand(fd, cmd.args);
	else if (name == "PART")
		handlePartCommand(fd, cmd.args);
	else
		sendNumeric(fd, ERR_UNKNOWNCOMMAND, name + " :Unknown command");
}

void Server::sendData(size_t index)
{
	int		fd = _pollFds[index].fd;
	Client	&client = _clients[fd];

	if (!client.hasOut())
		return ;

	// send() may accept only part of the buffer; whatever is left stays queued
	// and POLLOUT will bring us back here.
	ssize_t	bytes = send(fd, client.getOut().c_str(), client.getOut().size(), 0);

	if (bytes <= 0)
	{
		closeClient(index);
		return ;
	}
	client.consumeOut(static_cast<size_t>(bytes));
}

void Server::closeClient(size_t index)
{
	int	fd = _pollFds[index].fd;

	// Phase 1: remove this fd from every channel before erasing the client.
	// Descriptors are recycled, so a stale entry would hand a future client
	// somebody else's membership and operator status.
	close(fd);
	_clients.erase(fd);
	_pollFds.erase(_pollFds.begin() + static_cast<long>(index));

	std::cout << "client disconnected: fd " << fd << std::endl;
}

// POLLOUT is requested only while a client actually has bytes waiting,
// otherwise poll() would report writability constantly and spin the loop.
void Server::updatePollFds()
{
	for (size_t i = 1; i < _pollFds.size(); ++i)
	{
		std::map<int, Client>::iterator	it = _clients.find(_pollFds[i].fd);

		_pollFds[i].events = POLLIN;
		if (it != _clients.end() && it->second.hasOut())
			_pollFds[i].events |= POLLOUT;
		_pollFds[i].revents = 0;
	}
}

int Server::pollIndexOf(int fd) const
{
	for (size_t i = 0; i < _pollFds.size(); ++i)
	{
		if (_pollFds[i].fd == fd)
			return (static_cast<int>(i));
	}
	return (-1);
}
