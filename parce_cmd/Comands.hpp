#ifndef COMANDS_HPP
#define COMANDS_HPP

#include "Parcer.hpp"
#include "client.hpp"

class CmdHandler
{
    private:
        void    handlePass(Command &cmd, Client &client, Server &server);
        
        
        void    handleNick(Command &cmd, Client &client, Server &server);
        bool    isValidNick(const std::string &nick);

        void    handleUser(Command &cmd, Client &client, Server &server);


        void    checkRegistr(Client &client, Server &server);
        void    handlePrivmsg();

        void    handlePing(Command &cmd, Client &client, Server &server);

        void    handlePrivMsg(Command &cmd, Client &client, Server &server);

        void    sendNumeric(Server &server, Client &client, 
                                const std::string &code, 
                                const std::string &message);

    public:
        CmdHandler();
        ~CmdHandler();



        void    execute(Command &cmd, Client &client, Server &server);

    
};

#endif