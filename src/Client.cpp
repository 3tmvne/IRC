#include "Client.hpp"

Client::Client()
	: fd(-1), nickname(""), username(""), realname(""), hostname("localhost"),
	  inbuf(""), outbuf(""), passok(false), registered(false), quitting(false)
{
}

Client::Client(int fd)
	: fd(fd), nickname(""), username(""), realname(""), hostname("localhost"),
	  inbuf(""), outbuf(""), passok(false), registered(false), quitting(false)
{
}

Client::Client(const Client &other)
{
	*this = other;
}

Client &Client::operator=(const Client &other)
{
	if (this != &other)
	{
		fd = other.fd;
		nickname = other.nickname;
		username = other.username;
		realname = other.realname;
		hostname = other.hostname;
		inbuf = other.inbuf;
		outbuf = other.outbuf;
		passok = other.passok;
		registered = other.registered;
		quitting = other.quitting;
	}
	return (*this);
}

Client::~Client()
{
}

int Client::getfd() const
{
	return (fd);
}

const std::string &Client::getnickname() const
{
	return (nickname);
}

void Client::setnickname(const std::string &nickname)
{
	this->nickname = nickname;
}

const std::string &Client::getusername() const
{
	return (username);
}

void Client::setusername(const std::string &username)
{
	this->username = username;
}

const std::string &Client::getrealname() const
{
	return (realname);
}

void Client::setrealname(const std::string &realname)
{
	this->realname = realname;
}

const std::string &Client::gethostname() const
{
	return (hostname);
}

void Client::sethostname(const std::string &hostname)
{
	this->hostname = hostname;
}

void Client::appendin(const std::string &data)
{
	inbuf += data;
}

bool Client::extractline(std::string &line)
{
	size_t	position = inbuf.find('\n');

	if (position == std::string::npos)
		return (false);
	line = inbuf.substr(0, position);
	inbuf.erase(0, position + 1);
	if (!line.empty() && line[line.size() - 1] == '\r')
		line.erase(line.size() - 1);
	return (true);
}

size_t Client::insize() const
{
	return (inbuf.size());
}

void Client::appendout(const std::string &data)
{
	outbuf += data;
}

const std::string &Client::getout() const
{
	return (outbuf);
}

void Client::consumeout(size_t n)
{
	outbuf.erase(0, n);
}

bool Client::hasout() const
{
	return (!outbuf.empty());
}

bool Client::haspass() const
{
	return (passok);
}

void Client::setpass(bool value)
{
	this->passok = value;
}

bool Client::isregistered() const
{
	return (registered);
}

void Client::setregistered(bool value)
{
	this->registered = value;
}

bool Client::isquitting() const
{
	return (quitting);
}

void Client::setquitting(bool value)
{
	this->quitting = value;
}

std::string Client::prefix() const
{
	return (":" + nickname + "!" + username + "@" + hostname);
}
