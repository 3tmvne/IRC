/* mini_serv.cpp
**
** A minimal TCP chat server built around ONE poll() loop, in C++98.
** Every complete line received from a client is broadcast to the others.
**
** This is the skeleton of ft_irc: everything here (non-blocking sockets,
** a single poll(), per-client read/write buffers, line reassembly) is
** exactly what the real project needs — read README.md for the full story.
*/

#include <iostream>      // std::cout, std::cerr
#include <sstream>       // std::ostringstream (int -> string in C++98)
#include <string>
#include <vector>
#include <map>
#include <cstdlib>       // std::atoi, std::exit
#include <cstring>       // std::memset
#include <cerrno>        // errno, EINTR
#include <csignal>       // std::signal
#include <unistd.h>      // close
#include <fcntl.h>       // fcntl, F_SETFL, O_NONBLOCK
#include <poll.h>        // poll, struct pollfd
#include <sys/types.h>
#include <sys/socket.h>  // socket, setsockopt, bind, listen, accept, recv, send
#include <netinet/in.h>  // sockaddr_in, htons, htonl
#include <arpa/inet.h>   // inet_ntoa

// One instance per connected client.
struct Client
{
    std::string inBuf;  // bytes received but not yet forming a full line
    std::string outBuf; // bytes waiting until the socket is ready for writing
};

// volatile sig_atomic_t is the ONLY type C++98 allows a signal handler
// to write to; a plain bool would be undefined behavior.
static volatile sig_atomic_t g_running = 1;

static void onSignal(int)
{
    g_running = 0; // poll() gets interrupted, the main loop exits cleanly
}

static void fatal(const std::string &msg)
{
    std::cerr << "Fatal: " << msg << std::endl;
    std::exit(1);
}

static std::string toString(int n)
{
    std::ostringstream oss;
    oss << n;
    return oss.str();
}

static void setNonBlocking(int fd)
{
    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
        fatal("fcntl(O_NONBLOCK) failed");
}

static int createListenSocket(int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
        fatal("socket() failed");

    // Without this, restarting the server right away fails with
    // "Address already in use" while the old socket is in TIME_WAIT.
    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
        fatal("setsockopt(SO_REUSEADDR) failed");

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // listen on every local interface
    addr.sin_port = htons(port);              // port must be in network byte order

    if (bind(fd, (sockaddr *)&addr, sizeof(addr)) == -1)
        fatal("bind() failed (port already in use?)");
    if (listen(fd, SOMAXCONN) == -1)
        fatal("listen() failed");

    setNonBlocking(fd);
    return fd;
}

static void acceptNewClient(int listenFd, std::vector<pollfd> &fds,
                            std::map<int, Client> &clients)
{
    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int fd = accept(listenFd, (sockaddr *)&addr, &len);
    if (fd == -1)
        return; // the connection vanished between poll() and accept()
    setNonBlocking(fd);

    pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    fds.push_back(pfd);

    clients[fd] = Client();
    clients[fd].outBuf += "Welcome, you are client " + toString(fd) + "\r\n";

    std::cout << "client " << fd << " connected ("
              << inet_ntoa(addr.sin_addr) << ")" << std::endl;
}

static void disconnectClient(size_t i, std::vector<pollfd> &fds,
                             std::map<int, Client> &clients)
{
    int fd = fds[i].fd;
    std::cout << "client " << fd << " disconnected" << std::endl;
    close(fd);
    clients.erase(fd);
    fds.erase(fds.begin() + i);
}

// Queue a message for every client except the sender.
// We never send() here: the data waits in outBuf until poll() says POLLOUT.
static void broadcast(int fromFd, const std::string &text,
                      std::map<int, Client> &clients)
{
    std::string msg = "client " + toString(fromFd) + ": " + text + "\r\n";
    for (std::map<int, Client>::iterator it = clients.begin();
         it != clients.end(); ++it)
    {
        if (it->first != fromFd)
            it->second.outBuf += msg;
    }
}

// Returns false if the client was disconnected (fds[i] is gone).
static bool readFromClient(size_t i, std::vector<pollfd> &fds,
                           std::map<int, Client> &clients)
{
    int fd = fds[i].fd;
    char buf[4096];
    ssize_t r = recv(fd, buf, sizeof(buf), 0);
    if (r <= 0) // 0 = client closed the connection, -1 = error
    {
        disconnectClient(i, fds, clients);
        return false;
    }

    Client &c = clients[fd];
    c.inBuf.append(buf, r);

    // Extract every COMPLETE line; the incomplete tail stays in inBuf
    // and will be finished by a future recv(). This is how you pass
    // the subject's "com / man / d" partial-data test.
    std::string::size_type pos;
    while ((pos = c.inBuf.find('\n')) != std::string::npos)
    {
        std::string line = c.inBuf.substr(0, pos);
        c.inBuf.erase(0, pos + 1);
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1); // IRC lines end with "\r\n"
        if (!line.empty())
            broadcast(fd, line, clients);
    }
    return true;
}

static void writeToClient(size_t i, std::vector<pollfd> &fds,
                          std::map<int, Client> &clients)
{
    int fd = fds[i].fd;
    Client &c = clients[fd];
    ssize_t s = send(fd, c.outBuf.c_str(), c.outBuf.size(), 0);
    if (s == -1)
    {
        disconnectClient(i, fds, clients);
        return;
    }
    c.outBuf.erase(0, s); // keep whatever send() could not take (partial write)
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: ./mini_serv <port>" << std::endl;
        return 1;
    }
    int port = std::atoi(argv[1]);
    if (port < 1 || port > 65535)
        fatal("invalid port");

    std::signal(SIGINT, onSignal);  // ctrl+C -> clean shutdown
    std::signal(SIGPIPE, SIG_IGN);  // never die on a write to a closed socket

    int listenFd = createListenSocket(port);
    std::cout << "listening on port " << port
              << "  (try: nc 127.0.0.1 " << port << ")" << std::endl;

    std::vector<pollfd> fds;      // everything poll() watches; fds[0] = listen socket
    std::map<int, Client> clients; // fd -> buffers

    pollfd lpfd;
    lpfd.fd = listenFd;
    lpfd.events = POLLIN;
    lpfd.revents = 0;
    fds.push_back(lpfd);

    while (g_running)
    {
        // Ask for POLLOUT only when a client actually has data queued,
        // otherwise poll() would wake up constantly for nothing.
        for (size_t i = 1; i < fds.size(); ++i)
        {
            fds[i].events = POLLIN;
            if (!clients[fds[i].fd].outBuf.empty())
                fds[i].events |= POLLOUT;
        }

        // THE one and only blocking call of the whole program.
        if (poll(&fds[0], fds.size(), -1) == -1)
        {
            if (errno == EINTR)
                continue; // a signal (ctrl+C) interrupted poll, not an error
            fatal("poll() failed");
        }

        if (fds[0].revents & POLLIN)
            acceptNewClient(listenFd, fds, clients);

        // Walk backwards so disconnectClient() can erase fds[i] safely.
        for (size_t i = fds.size(); i-- > 1; )
        {
            // POLLIN is handled FIRST, even when POLLHUP is also set:
            // a client that sent a last line and closed right away still
            // has readable bytes in the kernel buffer — disconnecting
            // before reading would silently lose them. recv() returning
            // 0 (or -1) is what tells us the client is really gone.
            if (fds[i].revents & POLLIN)
            {
                if (!readFromClient(i, fds, clients))
                    continue; // client is gone, fds[i] is no longer valid
            }
            else if (fds[i].revents & (POLLHUP | POLLERR | POLLNVAL))
            {
                disconnectClient(i, fds, clients);
                continue;
            }
            if (fds[i].revents & POLLOUT)
                writeToClient(i, fds, clients);
        }
    }

    for (size_t i = 0; i < fds.size(); ++i)
        close(fds[i].fd);
    std::cout << "\nserver stopped, bye" << std::endl;
    return 0;
}
