/* simple_serv.cpp — the SIMPLEST possible poll() server (C++98).
**
** It does one thing: when a client sends a full line, the server
** sends it back ("you said: ..."). Start here. Read README.md.
**
** Build:  make      Run:  ./simple_serv 6667      Try:  nc 127.0.0.1 6667
*/

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cstring>       // memset
#include <cstdlib>       // atoi
#include <unistd.h>      // close
#include <fcntl.h>       // fcntl (non-blocking)
#include <poll.h>        // poll
#include <sys/types.h>
#include <sys/socket.h>  // socket, bind, listen, accept, recv, send
#include <netinet/in.h>  // sockaddr_in, htons

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: ./simple_serv <port>" << std::endl;
        return 1;
    }

    // ---- STEP 1: create the "door" (the listening socket) ----
    int listenFd = socket(AF_INET, SOCK_STREAM, 0);   // a TCP socket
    int yes = 1;
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);          // any local address
    addr.sin_port = htons(std::atoi(argv[1]));         // the port, network order

    if (bind(listenFd, (sockaddr *)&addr, sizeof(addr)) == -1)
    {
        std::cerr << "bind failed (port busy?)" << std::endl;
        return 1;
    }
    listen(listenFd, 10);
    fcntl(listenFd, F_SETFL, O_NONBLOCK);

    // ---- STEP 2: the list of sockets poll() watches ----
    std::vector<pollfd> fds;                 // fds[0] = the door
    std::map<int, std::string> inBuf;        // what each client typed so far

    pollfd p;
    p.fd = listenFd;
    p.events = POLLIN;                       // "tell me when readable"
    p.revents = 0;
    fds.push_back(p);

    std::cout << "listening on port " << argv[1] << std::endl;

    // ---- STEP 3: the loop. The server only ever waits HERE. ----
    while (true)
    {
        poll(&fds[0], fds.size(), -1);       // sleep until something happens

        // backwards, so removing a client does not break the loop
        for (int i = (int)fds.size() - 1; i >= 0; --i)
        {
            if (!(fds[i].revents & POLLIN))  // nothing happened on this one
                continue;

            if (fds[i].fd == listenFd)       // event on the door = new client
            {
                int fd = accept(listenFd, NULL, NULL);
                if (fd == -1)
                    continue;
                fcntl(fd, F_SETFL, O_NONBLOCK);
                pollfd np;
                np.fd = fd;
                np.events = POLLIN;
                np.revents = 0;
                fds.push_back(np);
                std::cout << "client " << fd << " connected" << std::endl;
            }
            else                             // event on a client = data or goodbye
            {
                char tmp[1024];
                ssize_t n = recv(fds[i].fd, tmp, sizeof(tmp), 0);
                if (n <= 0)                  // 0 = client left, -1 = error
                {
                    std::cout << "client " << fds[i].fd << " left" << std::endl;
                    close(fds[i].fd);
                    inBuf.erase(fds[i].fd);
                    fds.erase(fds.begin() + i);
                    continue;
                }

                inBuf[fds[i].fd].append(tmp, n);   // store the piece

                // do we have at least one FULL line now?
                std::string &buf = inBuf[fds[i].fd];
                size_t pos;
                while ((pos = buf.find('\n')) != std::string::npos)
                {
                    std::string line = buf.substr(0, pos + 1); // take the line
                    buf.erase(0, pos + 1);                     // remove it from buf
                    std::string reply = "you said: " + line;
                    send(fds[i].fd, reply.c_str(), reply.size(), 0);
                }
            }
        }
    }
}
