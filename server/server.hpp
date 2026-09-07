#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <cstring>
#include <cerrno>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>

class Server
{
public:
    struct Client
    {
        int fd;
        std::string input;
        std::string output;
        bool authenticated;

        Client();
        Client(int socketFd);
    };

    Server(int port, const std::string& password);
    ~Server();

    void run();

private:
    int _listenFd;
    int _port;
    std::string _password;
    bool _running;

    std::vector<struct pollfd> _pollFds;
    std::map<int, Client> _clients;

    Server(const Server& other);
    Server& operator=(const Server& other);

    void initSocket();
    void acceptClient();
    void receiveData(size_t index);
    void sendData(size_t index);
    void processInput(Client& client);
    void processCommand(Client& client, const std::string& line);
    void closeClient(size_t index);
    void updatePollFds();

    void queueMessage(Client& client, const std::string& message);
    static std::string trimLine(const std::string& line);
};

#endif
