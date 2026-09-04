#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <vector>
#include <map>
// #include "Client.hpp"

class Channel
{
	private:
		std::string			_name;
		std::string			_topic;
		std::string			_key;
		std::vector<int>	_members;
		std::vector<int>	_operators;
		std::vector<int>	_invited;
		bool				_inviteOnly;
		bool				_topicLocked;
		bool				_hasKey;
		size_t				_limit;

	public:
		Channel();
		Channel(const std::string &name);
		Channel(const Channel &other);
		Channel &operator=(const Channel &other);
		~Channel();

		const std::string		&getName() const;
		const std::string		&getTopic() const;
		const std::string		&getKey() const;
		const std::vector<int>	&getMembers() const;
		size_t					getLimit() const;
		bool					isInviteOnly() const;
		bool					isTopicLocked() const;
		bool					hasKey() const;

		void	setTopic(const std::string &topic);
		void	setKey(const std::string &key);
		void	unsetKey();
		void	setInviteOnly(bool value);
		void	setTopicLocked(bool value);
		void	setLimit(size_t limit);

		bool	isMember(int fd) const;
		bool	isOperator(int fd) const;
		bool	isInvited(int fd) const;
		bool	isFull() const;
		bool	isEmpty() const;

		void	addMember(int fd);
		void	removeMember(int fd);
		void	addOperator(int fd);
		void	removeOperator(int fd);
		void	addInvite(int fd);
		void	removeInvite(int fd);

		std::string	getModeString() const;
		// void		broadcast(std::map<int, Client> &clients, const std::string &message, int except) const;
};

#endif
