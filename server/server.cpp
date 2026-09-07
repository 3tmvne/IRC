#include "server.hpp"

Server::Client::Client() : fd(-1), authenticated(false){}

Server::Client::Client(int socketFd) : fd(socketFd), authenticated(false){}

Server::Server(int port, const std::string& password): _listenFd(-1), _port(port), _password(password), _running(true){
    initSocket();
}

Server::~Server(){
    for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
        close(it->first);

    if (_listenFd != -1)
        close(_listenFd);
}

void Server::initSocket()
{
    _listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (_listenFd == -1)
        throw std::runtime_error("socket failed");

    int option = 1;
    if (setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) == -1)
        throw std::runtime_error("setsockopt failed");

    if (fcntl(_listenFd, F_SETFL, O_NONBLOCK) == -1)
        throw std::runtime_error("fcntl failed");

    sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(_port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(_listenFd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == -1)
        throw std::runtime_error("bind failed");

    if (listen(_listenFd, SOMAXCONN) == -1)
        throw std::runtime_error("listen failed");

    pollfd serverPoll;
    serverPoll.fd = _listenFd;
    serverPoll.events = POLLIN;
    serverPoll.revents = 0;
    _pollFds.push_back(serverPoll);
}

void Server::run(){
    while (_running){
        updatePollFds();

        int result = poll(&_pollFds[0], _pollFds.size(), -1);
        if (result == -1){
            if (errno == EINTR)//!!
                continue;
            break;
        }

        for (size_t i = 0; i < _pollFds.size(); ++i){
            if (_pollFds[i].revents == 0)
                continue;

            if (_pollFds[i].fd == _listenFd && (_pollFds[i].revents & POLLIN)){
                acceptClient();
                continue;
            }

            if (_pollFds[i].revents & (POLLHUP | POLLERR | POLLNVAL)){
                closeClient(i);
                --i;
                continue;
            }

            if (_pollFds[i].revents & POLLIN){
                size_t oldSize = _pollFds.size();
                receiveData(i);
                if (_pollFds.size() < oldSize){
                    --i;
                    continue;
                }
            }

            if (i < _pollFds.size() && (_pollFds[i].revents & POLLOUT))
                sendData(i);
        }
    }
}

void Server::acceptClient(){
    int clientFd = accept(_listenFd, NULL, NULL);
    if (clientFd == -1)
        return;

    if (fcntl(clientFd, F_SETFL, O_NONBLOCK) == -1){
        close(clientFd);
        return;
    }

    pollfd clientPoll;
    clientPoll.fd = clientFd;
    clientPoll.events = POLLIN;
    clientPoll.revents = 0;
    _pollFds.push_back(clientPoll);

    _clients.insert(std::make_pair(clientFd, Client(clientFd)));

    std::cout << "Client connected: " << clientFd << std::endl;
    queueMessage(_clients[clientFd], ":server NOTICE AUTH :Use PASS <password> to connect\r\n");
}

void Server::receiveData(size_t index){
    int fd = _pollFds[index].fd;
    char buffer[512];

    int bytes = recv(fd, buffer, sizeof(buffer), 0);

    if (bytes <= 0){
        closeClient(index);
        return;
    }

    _clients[fd].input.append(buffer, bytes);
    processInput(_clients[fd]);
}

void Server::processInput(Client& client){
    size_t position;

    while ((position = client.input.find('\n')) != std::string::npos){
        std::string line = client.input.substr(0, position);
        client.input.erase(0, position + 1);

        line = trimLine(line);
        if (!line.empty())
            processCommand(client, line);
    }
}

void Server::processCommand(Client& client, const std::string& line){
    size_t space = line.find(' ');
    std::string command;
    std::string argument;

    if (space == std::string::npos)
        command = line;
    else{
        command = line.substr(0, space);
        argument = line.substr(space + 1);
    }

    if (command == "PASS"){
        if (argument == _password){
            client.authenticated = true;
            queueMessage(client, ":server 001 :Welcome to the server\r\n");
        }
        else
            queueMessage(client, ":server 464 :Password incorrect\r\n");
    }
    else if (command == "PING")
        queueMessage(client, ":server PONG :" + argument + "\r\n");
    else if (command == "QUIT"){
        client.output.clear();
        queueMessage(client, ":server ERROR :Goodbye\r\n");
        // The actual close happens when the client disconnects or can be
        // added later as protocol-specific state.
    }
    else if (!client.authenticated)
        queueMessage(client, ":server 451 :You must use PASS first\r\n");
    else
        queueMessage(client, ":server 421 :Unknown command\r\n");
}

void Server::sendData(size_t index){
    int fd = _pollFds[index].fd;
    Client& client = _clients[fd];

    if (client.output.empty())
        return;

    int bytes = send(fd, client.output.c_str(), client.output.size(), 0);

    if (bytes <= 0){
        closeClient(index);
        return;
    }

    client.output.erase(0, bytes);
}

void Server::closeClient(size_t index){
    int fd = _pollFds[index].fd;

    close(fd);
    _clients.erase(fd);
    _pollFds.erase(_pollFds.begin() + index);

    std::cout << "Client disconnected: " << fd << std::endl;
}

void Server::updatePollFds(){
    for (size_t i = 1; i < _pollFds.size(); ++i){
        int fd = _pollFds[i].fd;
        _pollFds[i].events = POLLIN;

        if (!_clients[fd].output.empty())
            _pollFds[i].events |= POLLOUT;

        _pollFds[i].revents = 0;
    }
}

void Server::queueMessage(Client& client, const std::string& message){
    client.output += message;
}

std::string Server::trimLine(const std::string& line){
    if (!line.empty() && line[line.size() - 1] == '\r')
        return line.substr(0, line.size() - 1);
    return line;
}
