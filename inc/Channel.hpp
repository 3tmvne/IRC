#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <vector>
#include <map>
#include "Client.hpp"

class Channel
{
	private:
		std::string			name;
		std::string			topic;
		std::string			key;
		std::vector<int>	members;
		std::vector<int>	operators;
		std::vector<int>	invited;
		bool				inviteOnly;
		bool				topicLocked;
		bool				hasKey;
		size_t				limit;

	public:
		Channel();
		Channel(const std::string &name);
		Channel(const Channel &other);
		Channel &operator=(const Channel &other);
		~Channel();

		const std::string		&getname() const;
		const std::string		&gettopic() const;
		const std::string		&getkey() const;
		const std::vector<int>	&getmembers() const;
		size_t					getlimit() const;
		bool					isinviteonly() const;
		bool					istopiclocked() const;
		bool					haskey() const;

		void	settopic(const std::string &topic);
		void	setkey(const std::string &key);
		void	unsetkey();
		void	setinviteonly(bool value);
		void	settopiclocked(bool value);
		void	setlimit(size_t limit);

		bool	ismember(int fd) const;
		bool	isoperator(int fd) const;
		bool	isinvited(int fd) const;
		bool	isfull() const;
		bool	isempty() const;

		void	addmember(int fd);
		void	removemember(int fd);
		void	addoperator(int fd);
		void	removeoperator(int fd);
		void	addinvite(int fd);
		void	removeinvite(int fd);

		std::string	getmodestring() const;
		void		broadcast(std::map<int, Client> &clients, const std::string &message, int except) const;
};

#endif
