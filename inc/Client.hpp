#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>

class Client
{
	private:
		int			fd;

		std::string	nickname;
		std::string	username;
		std::string	realname;
		std::string	hostname;

		std::string	inbuf;
		std::string	outbuf;

		bool		passok;
		bool		registered;
		bool		quitting;

	public:
		Client();
		Client(int fd);
		Client(const Client &other);
		Client &operator=(const Client &other);
		~Client();

		int					getfd() const;

		const std::string	&getnickname() const;
		void				setnickname(const std::string &nickname);
		const std::string	&getusername() const;
		void				setusername(const std::string &username);
		const std::string	&getrealname() const;
		void				setrealname(const std::string &realname);
		const std::string	&gethostname() const;
		void				sethostname(const std::string &hostname);

		
		void				appendin(const std::string &data);
		bool				extractline(std::string &line);
		size_t				insize() const;

		
		void				appendout(const std::string &data);
		const std::string	&getout() const;
		void				consumeout(size_t n);
		bool				hasout() const;

		bool				haspass() const;
		void				setpass(bool value);
		bool				isregistered() const;
		void				setregistered(bool value);
		bool				isquitting() const;
		void				setquitting(bool value);

		
		std::string			prefix() const;
};

#endif
