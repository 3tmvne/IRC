#include "client.hpp"

#include "client.hpp"

Client::Client()
    : _fd(-1),
      _nickname(""),
      _username(""),
      _realname(""),
      _buffer(""),
      _authenticated(false),
      _registered(false)
{
}

Client::Client(int fd)
    : _fd(fd),
      _nickname(""),
      _username(""),
      _realname(""),
      _buffer(""),
      _authenticated(false),
      _registered(false)
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
        _buffer = other._buffer;
        _authenticated = other._authenticated;
        _registered = other._registered;
    }

    return *this;
}

Client::~Client()
{
}


int Client::getFd() const
{
    return _fd;
}

const std::string &Client::getNickname() const
{
    return _nickname;
}

void Client::setNickname(const std::string &nickname)
{
    _nickname = nickname;
}

const std::string &Client::getUsername() const
{
    return _username;
}

void Client::setUsername(const std::string &username)
{
    _username = username;
}

const std::string &Client::getRealname() const
{
    return _realname;
}

void Client::setRealname(const std::string &realname)
{
    _realname = realname;
}

const std::string &Client::getBuffer() const
{
    return _buffer;
}

void Client::appendBuffer(const std::string &data)
{
    _buffer += data;
}

void Client::clearBuffer()
{
    _buffer.clear();
}

void Client::setBuffer(const std::string &buffer)
{
    _buffer = buffer;
}

bool Client::isAuthenticated() const
{
    return _authenticated;
}

void Client::setAuthenticated(bool value)
{
    _authenticated = value;
}

bool Client::isRegistered() const
{
    return _registered;
}

void Client::setRegistered(bool value)
{
    _registered = value;
}