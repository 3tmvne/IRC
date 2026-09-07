#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <cstring>
#include <cerrno>
#include <csignal>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>

#include "Client.hpp"
#include "Channel.hpp"
#include "Parser.hpp"
#include "Replies.hpp"

// Set by the SIGINT/SIGTERM handler in main.cpp. The poll loop checks it so
// that Ctrl-C unwinds through the destructors instead of leaking descriptors.
extern volatile sig_atomic_t	g_stop;

class Server
{
	public:
		Server(int port, const std::string &password);
		~Server();

		void	run();

	private:
		int								_listenFd;
		int								_port;
		std::string						_password;
		std::string						_serverName;
		Parser							_parser;

		std::vector<struct pollfd>		_pollFds;
		std::map<int, Client>			_clients;
		std::map<std::string, Channel>	_channels;

		// A server owns a listening socket; copying one would double-close it.
		Server(const Server &other);
		Server &operator=(const Server &other);

		/* ---- event loop (src/Server.cpp) ---- */
		void	initSocket();
		void	acceptClient();
		void	receiveData(size_t index);
		void	sendData(size_t index);
		void	processInput(int fd);
		void	closeClient(size_t index);
		void	updatePollFds();
		int		pollIndexOf(int fd) const;

		/* ---- dispatch (src/Server.cpp) ---- */
		void	executeCommand(int fd, const Command &cmd);

		/* ---- shared helpers (src/commands/ChannelCmds.cpp) ---- */
		static std::vector<std::string>	splitList(const std::string &str, char sep);
		std::string	userPrefix(int fd);
		void		sendNumeric(int fd, const std::string &code, const std::string &args);
		int			getFdByNick(const std::string &nick);
		std::string	buildNamesList(Channel &channel);
		void		removeFromChannel(int fd, const std::string &name);
		void		broadcastToPeers(int fd, const std::string &message, bool includeSelf);

		/* ---- registration (src/commands/Registration.cpp) ---- */
		void	handlePassCommand(int fd, const std::vector<std::string> &params);
		void	handleNickCommand(int fd, const std::vector<std::string> &params);
		void	handleUserCommand(int fd, const std::vector<std::string> &params);
		void	completeRegistration(int fd);

		/* ---- messaging (src/commands/Messaging.cpp) ---- */
		void	handlePingCommand(int fd, const std::vector<std::string> &params);
		void	handleQuitCommand(int fd, const std::vector<std::string> &params);
		void	handlePrivmsgCommand(int fd, const std::vector<std::string> &params);
		void	handleNoticeCommand(int fd, const std::vector<std::string> &params);
		void	handlePartCommand(int fd, const std::vector<std::string> &params);
		void	deliverMessage(int fd, const std::vector<std::string> &params,
				const std::string &verb, bool quiet);

		/* ---- channels (src/commands/ChannelCmds.cpp) ---- */
		void	handleJoinCommand(int fd, const std::vector<std::string> &params);
		void	handleInvitCommand(int fd, const std::vector<std::string> &params);
		void	handleKickCommand(int fd, const std::vector<std::string> &params);
		void	handleTopicCommand(int fd, const std::vector<std::string> &params);
		void	handleModeCommand(int fd, const std::vector<std::string> &params);
};

#endif
