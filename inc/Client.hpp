#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>

// One client connection. Owns its own input/output buffers so that every
// socket operation in the server stays non-blocking: recv() appends here,
// send() drains from here, and nothing ever blocks waiting on a peer.
class Client
{
	private:
		int			_fd;

		std::string	_nickname;
		std::string	_username;
		std::string	_realname;
		std::string	_hostname;

		std::string	_inBuf;
		std::string	_outBuf;

		bool		_passOk;
		bool		_registered;
		bool		_quitting;

	public:
		Client();
		Client(int fd);
		Client(const Client &other);
		Client &operator=(const Client &other);
		~Client();

		int					getFd() const;

		const std::string	&getNickname() const;
		void				setNickname(const std::string &nickname);
		const std::string	&getUsername() const;
		void				setUsername(const std::string &username);
		const std::string	&getRealname() const;
		void				setRealname(const std::string &realname);
		const std::string	&getHostname() const;
		void				setHostname(const std::string &hostname);

		// Input side: raw bytes in, complete lines out.
		void				appendIn(const std::string &data);
		bool				extractLine(std::string &line);
		size_t				inSize() const;

		// Output side: everything the server wants to say goes through here.
		void				appendOut(const std::string &data);
		const std::string	&getOut() const;
		void				consumeOut(size_t n);
		bool				hasOut() const;

		bool				hasPass() const;
		void				setPass(bool value);
		bool				isRegistered() const;
		void				setRegistered(bool value);
		bool				isQuitting() const;
		void				setQuitting(bool value);

		// ":nick!user@host" -- the RFC source prefix of every relayed message.
		std::string			prefix() const;
};

#endif
