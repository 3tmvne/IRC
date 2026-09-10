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

extern volatile sig_atomic_t	g_stop;

class Server
{
	public:
		Server(int port, const std::string &password);
		~Server();

		void	run();

	private:
		int								listenFd;
		int								port;
		std::string						password;
		std::string						serverName;
		Parser							parser;

		std::vector<struct pollfd>		pollFds;
		std::map<int, Client>			clients;
		std::map<std::string, Channel>	channels;

		
		Server(const Server &other);
		Server &operator=(const Server &other);

		
		void	initsocket();
		void	acceptclient();
		void	receivedata(size_t index);
		void	senddata(size_t index);
		void	processinput(int fd);
		void	closeclient(size_t index);
		void	updatepollfds();
		int		pollindexof(int fd) const;

		
		void	executecommand(int fd, const Command &cmd);

		
		static std::vector<std::string>	splitlist(const std::string &str, char sep);
		std::string	userprefix(int fd);
		void		sendnumeric(int fd, const std::string &code, const std::string &args);
		int			getfdbynick(const std::string &nick);
		std::string	buildnameslist(Channel &channel);
		void		removefromchannel(int fd, const std::string &name);
		void		broadcasttopeers(int fd, const std::string &message, bool includeSelf);

		
		void	handlepasscommand(int fd, const std::vector<std::string> &params);
		void	handlenickcommand(int fd, const std::vector<std::string> &params);
		void	handleusercommand(int fd, const std::vector<std::string> &params);
		void	completeregistration(int fd);

		
		void	handlepingcommand(int fd, const std::vector<std::string> &params);
		void	handlequitcommand(int fd, const std::vector<std::string> &params);
		void	handleprivmsgcommand(int fd, const std::vector<std::string> &params);
		void	handlenoticecommand(int fd, const std::vector<std::string> &params);
		void	handlepartcommand(int fd, const std::vector<std::string> &params);
		void	delivermessage(int fd, const std::vector<std::string> &params,
				const std::string &verb, bool quiet);

		
		void	handlejoincommand(int fd, const std::vector<std::string> &params);
		void	handleinvitecommand(int fd, const std::vector<std::string> &params);
		void	handlekickcommand(int fd, const std::vector<std::string> &params);
		void	handletopiccommand(int fd, const std::vector<std::string> &params);
		void	handlemodecommand(int fd, const std::vector<std::string> &params);
};

#endif
