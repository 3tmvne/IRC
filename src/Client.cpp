#include "Client.hpp"

Client::Client()
	: _fd(-1), _nickname(""), _username(""), _realname(""), _hostname("localhost"),
	  _inBuf(""), _outBuf(""), _passOk(false), _registered(false), _quitting(false)
{
}

Client::Client(int fd)
	: _fd(fd), _nickname(""), _username(""), _realname(""), _hostname("localhost"),
	  _inBuf(""), _outBuf(""), _passOk(false), _registered(false), _quitting(false)
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
		_fd = other._fd;
		_nickname = other._nickname;
		_username = other._username;
		_realname = other._realname;
		_hostname = other._hostname;
		_inBuf = other._inBuf;
		_outBuf = other._outBuf;
		_passOk = other._passOk;
		_registered = other._registered;
		_quitting = other._quitting;
	}
	return (*this);
}

Client::~Client()
{
}

int Client::getFd() const
{
	return (_fd);
}

const std::string &Client::getNickname() const
{
	return (_nickname);
}

void Client::setNickname(const std::string &nickname)
{
	_nickname = nickname;
}

const std::string &Client::getUsername() const
{
	return (_username);
}

void Client::setUsername(const std::string &username)
{
	_username = username;
}

const std::string &Client::getRealname() const
{
	return (_realname);
}

void Client::setRealname(const std::string &realname)
{
	_realname = realname;
}

const std::string &Client::getHostname() const
{
	return (_hostname);
}

void Client::setHostname(const std::string &hostname)
{
	_hostname = hostname;
}

void Client::appendIn(const std::string &data)
{
	// Phase 1: enforce the 512-byte RFC line limit here so a peer that never
	// sends a newline cannot grow this buffer without bound.
	_inBuf += data;
}

// Pops one complete line off the input buffer, stripping CR and LF.
// Returns false when no full line has arrived yet -- partial commands simply
// stay buffered until the rest of the bytes show up.
bool Client::extractLine(std::string &line)
{
	size_t	position = _inBuf.find('\n');

	if (position == std::string::npos)
		return (false);
	line = _inBuf.substr(0, position);
	_inBuf.erase(0, position + 1);
	if (!line.empty() && line[line.size() - 1] == '\r')
		line.erase(line.size() - 1);
	return (true);
}

size_t Client::inSize() const
{
	return (_inBuf.size());
}

void Client::appendOut(const std::string &data)
{
	_outBuf += data;
}

const std::string &Client::getOut() const
{
	return (_outBuf);
}

void Client::consumeOut(size_t n)
{
	_outBuf.erase(0, n);
}

bool Client::hasOut() const
{
	return (!_outBuf.empty());
}

bool Client::hasPass() const
{
	return (_passOk);
}

void Client::setPass(bool value)
{
	_passOk = value;
}

bool Client::isRegistered() const
{
	return (_registered);
}

void Client::setRegistered(bool value)
{
	_registered = value;
}

bool Client::isQuitting() const
{
	return (_quitting);
}

void Client::setQuitting(bool value)
{
	_quitting = value;
}

std::string Client::prefix() const
{
	return (":" + _nickname + "!" + _username + "@" + _hostname);
}
