#include "Comands.hpp"
#include "Server.hpp"

#include <iostream>
#include <cctype>


CmdHandler::CmdHandler()
{
}

CmdHandler::~CmdHandler()
{
}


/* ************************************************************************** */
/*                                EXECUTE                                     */
/* ************************************************************************** */

void CmdHandler::execute(Command &cmd, Client &client, Server &server)
{
    if (cmd.cmd == "PASS")
        handlePass(cmd, client, server);
    else if (cmd.cmd == "NICK")
        handleNick(cmd, client, server);
    else if (cmd.cmd == "USER")
        handleUser(cmd, client, server);
    else if (cmd.cmd == "PING")
        handlePing(cmd, client, server);
    else if (cmd.cmd == "PRIVMSG")
        handlePrivMsg(cmd, client, server);
    else
        sendNumeric(server, client, "421",
            cmd.cmd + " :Unknown command");
}


/* ************************************************************************** */
/*                                  PASS                                      */
/* ************************************************************************** */

void CmdHandler::handlePass(Command &cmd, Client &client, Server &server)
{
    if (client.isRegistered())
    {
        sendNumeric(server, client, "462",
            "You may not reregister");
        return;
    }

    if (cmd.args.empty())
    {
        sendNumeric(server, client, "461",
            "PASS :Not enough parameters");
        return;
    }

    if (cmd.args[0] != server.getPassword())
    {
        sendNumeric(server, client, "464",
            "Password incorrect");
        return;
    }

    client.setAuthenticated(true);

    std::cout << "Password correct" << std::endl;

    checkRegistration(client, server);
}


/* ************************************************************************** */
/*                                  NICK                                      */
/* ************************************************************************** */

void CmdHandler::handleNick(Command &cmd, Client &client, Server &server)
{
    if (cmd.args.empty())
    {
        sendNumeric(server, client, "431",
            "No nickname given");
        return;
    }

    if (!isValidNick(cmd.args[0]))
    {
        sendNumeric(server, client, "432",
            cmd.args[0] + " :Erroneous nickname");
        return;
    }

    if (server.nicknameExists(cmd.args[0], client.getFd()))
    {
        sendNumeric(server, client, "433",
            cmd.args[0] + " :Nickname is already in use");
        return;
    }

    client.setNickname(cmd.args[0]);

    checkRegistration(client, server);
}


bool CmdHandler::isValidNick(const std::string &nick)
{
    if (nick.empty())
        return false;

    if (std::isdigit(nick[0]))
        return false;

    for (size_t i = 0; i < nick.size(); i++)
    {
        if (!std::isalnum(nick[i])
            && nick[i] != '_'
            && nick[i] != '-')
        {
            return false;
        }
    }

    return true;
}


/* ************************************************************************** */
/*                                  USER                                      */
/* ************************************************************************** */

void CmdHandler::handleUser(Command &cmd, Client &client, Server &server)
{
    if (client.isRegistered())
    {
        sendNumeric(server, client, "462",
            "You may not reregister");
        return;
    }

    if (cmd.args.size() != 4)
    {
        sendNumeric(server, client, "461",
            "USER :Not enough parameters");
        return;
    }

    client.setUsername(cmd.args[0]);
    client.setRealname(cmd.args[3]);

    checkRegistration(client, server);
}


/* ************************************************************************** */
/*                           CHECK REGISTRATION                               */
/* ************************************************************************** */

void CmdHandler::checkRegistration(Client &client, Server &server)
{
    if (client.isRegistered())
        return;

    if (client.isAuthenticated()
        && !client.getNickname().empty()
        && !client.getUsername().empty())
    {
        client.setRegistered(true);

        sendNumeric(server, client, "001",
            "Welcome to the IRC Network " + client.getNickname());
    }
}


/* ************************************************************************** */
/*                                  PING                                      */
/* ************************************************************************** */

void CmdHandler::handlePing(Command &cmd, Client &client, Server &server)
{
    if (cmd.args.empty())
    {
        sendNumeric(server, client, "461",
            "PING :Not enough parameters");
        return;
    }

    std::string response = "PONG :" + cmd.args[0];

    server.sendMessage(client, response);
}


/* ************************************************************************** */
/*                                PRIVMSG                                     */
/* ************************************************************************** */

void CmdHandler::handlePrivMsg(Command &cmd, Client &client, Server &server)
{
    if (!client.isRegistered())
    {
        sendNumeric(server, client, "451",
            "You have not registered");
        return;
    }

    if (cmd.args.empty())
    {
        sendNumeric(server, client, "411",
            "No recipient given (PRIVMSG)");
        return;
    }

    if (cmd.args.size() < 2 || cmd.args[1].empty())
    {
        sendNumeric(server, client, "412",
            "No text to send");
        return;
    }

    std::string target = cmd.args[0];
    std::string message = cmd.args[1];

    if (!target.empty() && target[0] == '#')
    {
        // Partie Channel
        // Le collegue qui gere les channels va traiter ce cas.
        return;
    }

    Client *receiver = server.findClientByNickname(target);

    if (receiver == NULL)
    {
        sendNumeric(server, client, "401",
            target + " :No such nick");
        return;
    }

    std::string response =
        ":" + client.getNickname()
        + "!" + client.getUsername()
        + " PRIVMSG "
        + target
        + " :" + message;

    server.sendMessage(*receiver, response);
}


/* ************************************************************************** */
/*                              NUMERIC REPLY                                 */
/* ************************************************************************** */

void CmdHandler::sendNumeric(Server &server,
                             Client &client,
                             const std::string &code,
                             const std::string &message)
{
    std::string nick = client.getNickname();

    if (nick.empty())
        nick = "*";

    std::string response =
        ":ircserv " + code + " " + nick + " :" + message;

    server.sendMessage(client, response);
}