#ifndef CLIENT_HPP
#define CLIENT_HPP


#include <string>

class Client
{
private:
    int         _fd;

    std::string _nickname;
    std::string _username;
    std::string _realname;

    std::string _buffer;

    bool        _authenticated;
    bool        _registered;

public:
    Client();
    Client(int fd);
    Client(const Client &other);
    Client &operator=(const Client &other);
    ~Client();

    // FD
    int getFd() const;

    // Nickname
    const std::string &getNickname() const;
    void setNickname(const std::string &nickname);

    // Username
    const std::string &getUsername() const;
    void setUsername(const std::string &username);

    // Realname
    const std::string &getRealname() const;
    void setRealname(const std::string &realname);

    // Buffer
    const std::string &getBuffer() const;
    void appendBuffer(const std::string &data);
    void clearBuffer();
    void setBuffer(const std::string &buffer);

    // Authentication
    bool isAuthenticated() const;
    void setAuthenticated(bool value);

    // Registration
    bool isRegistered() const;
    void setRegistered(bool value);
};

#endif
