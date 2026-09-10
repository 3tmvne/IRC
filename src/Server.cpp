#include "Server.hpp"

Server::Server(int port, const std::string &password)
	: listenFd(-1), port(port), password(password), serverName("ft_irc")
{
	initsocket();
}

Server::~Server()
{
	for (std::map<int, Client>::iterator it = clients.begin(); it != clients.end(); ++it)
		close(it->first);
	if (listenFd != -1)
		close(listenFd);
}

void Server::initsocket()
{
	listenFd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenFd == -1)
		throw std::runtime_error("socket() failed");

	
	int	option = 1;
	if (setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) == -1)
		throw std::runtime_error("setsockopt() failed");

	if (fcntl(listenFd, F_SETFL, O_NONBLOCK) == -1)
		throw std::runtime_error("fcntl() failed");

	sockaddr_in	address;
	std::memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons(static_cast<uint16_t>(port));
	address.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(listenFd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == -1)
		throw std::runtime_error("bind() failed");
	if (listen(listenFd, SOMAXCONN) == -1)
		throw std::runtime_error("listen() failed");

	pollfd	serverPoll;
	serverPoll.fd = listenFd;
	serverPoll.events = POLLIN;
	serverPoll.revents = 0;
	pollFds.push_back(serverPoll);
}

void Server::run()
{
	std::cout << "ft_irc listening on port " << port << std::endl;

	while (!g_stop)
	{
		updatepollfds();

		int	result = poll(&pollFds[0], pollFds.size(), -1);
		if (result == -1)
		{
			if (errno == EINTR)
				continue ;
			break ;
		}

		for (size_t i = 0; i < pollFds.size(); ++i)
		{
			if (pollFds[i].revents == 0)
				continue ;

			if (pollFds[i].fd == listenFd && (pollFds[i].revents & POLLIN))
			{
				acceptclient();
				continue ;
			}

			
			
			if (pollFds[i].revents & (POLLHUP | POLLERR | POLLNVAL))
			{
				closeclient(i);
				--i;
				continue ;
			}

			if (pollFds[i].revents & POLLIN)
			{
				size_t	oldSize = pollFds.size();

				receivedata(i);
				if (pollFds.size() < oldSize)
				{
					--i;
					continue ;
				}
			}

			if (i < pollFds.size() && (pollFds[i].revents & POLLOUT))
				senddata(i);
		}
	}
	std::cout << "shutting down" << std::endl;
}

void Server::acceptclient()
{
	int	clientFd = accept(listenFd, NULL, NULL);

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
	pollFds.push_back(clientPoll);
	clients.insert(std::make_pair(clientFd, Client(clientFd)));

	std::cout << "client connected: fd " << clientFd << std::endl;
}

void Server::receivedata(size_t index)
{
	int		fd = pollFds[index].fd;
	char	buffer[512];

	ssize_t	bytes = recv(fd, buffer, sizeof(buffer), 0);

	
	
	if (bytes <= 0)
	{
		closeclient(index);
		return ;
	}

	clients[fd].appendin(std::string(buffer, static_cast<size_t>(bytes)));
	processinput(fd);
}

void Server::processinput(int fd)
{
	std::map<int, Client>::iterator	it = clients.find(fd);
	std::string						line;

	if (it == clients.end())
		return ;
	while (it->second.extractline(line))
	{
		if (!line.empty())
			executecommand(fd, parser.parse(line));
		it = clients.find(fd);
		if (it == clients.end())
			return ;
	}
}

void Server::executecommand(int fd, const Command &cmd)
{
	const std::string	&name = cmd.cmd;

	if (name.empty())
		return ;

	
	
	
	if (!clients[fd].isregistered() && name != "PASS" && name != "NICK"
		&& name != "USER" && name != "QUIT" && name != "PING")
	{
		sendnumeric(fd, ERR_NOTREGISTERED, ":You have not registered");
		return ;
	}

	if (name == "PASS")
		handlepasscommand(fd, cmd.args);
	else if (name == "NICK")
		handlenickcommand(fd, cmd.args);
	else if (name == "USER")
		handleusercommand(fd, cmd.args);
	else if (name == "PING")
		handlepingcommand(fd, cmd.args);
	
		
	else if (name == "JOIN")
		handlejoincommand(fd, cmd.args);
	else if (name == "INVITE")
		handleinvitecommand(fd, cmd.args);
	else if (name == "KICK")
		handlekickcommand(fd, cmd.args);
	else if (name == "TOPIC")
		handletopiccommand(fd, cmd.args);
	else if (name == "MODE")
		handlemodecommand(fd, cmd.args);
	else if (name == "PRIVMSG")
		handleprivmsgcommand(fd, cmd.args);
	
	
	
	
	else
		sendnumeric(fd, ERR_UNKNOWNCOMMAND, name + " :Unknown command");
}

void Server::senddata(size_t index)
{
	int		fd = pollFds[index].fd;
	Client	&client = clients[fd];

	if (!client.hasout())
		return ;

	
	
	ssize_t	bytes = send(fd, client.getout().c_str(), client.getout().size(), 0);

	if (bytes <= 0)
	{
		closeclient(index);
		return ;
	}
	client.consumeout(static_cast<size_t>(bytes));
}

void Server::closeclient(size_t index)
{
	int	fd = pollFds[index].fd;

	
	
	
	close(fd);
	clients.erase(fd);
	pollFds.erase(pollFds.begin() + static_cast<long>(index));

	std::cout << "client disconnected: fd " << fd << std::endl;
}

void Server::updatepollfds()
{
	for (size_t i = 1; i < pollFds.size(); ++i)
	{
		std::map<int, Client>::iterator	it = clients.find(pollFds[i].fd);

		pollFds[i].events = POLLIN;
		if (it != clients.end() && it->second.hasout())
			pollFds[i].events |= POLLOUT;
		pollFds[i].revents = 0;
	}
}

int Server::pollindexof(int fd) const
{
	for (size_t i = 0; i < pollFds.size(); ++i)
	{
		if (pollFds[i].fd == fd)
			return (static_cast<int>(i));
	}
	return (-1);
}
