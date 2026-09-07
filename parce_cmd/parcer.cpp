#include "client.hpp"
#include "Parcer.hpp"


Parcer::Parcer()
{
}

Parcer::~Parcer()
{

}

Command Parcer::parcing(const std::string &line)
{
    Command command;
    std::istringstream stream(line);
    std::string word;

    stream >> word;

    if(!word.empty() && word[0] == ':')
    {
        command.prefix = word.substr(1);
        stream >> command.cmd;
    }

    else
        command.cmd = word;

    while (stream >> word)
    {
        if (!word.empty() && word[0] == ':')
        {
            std::string rest;

            word.erase(0, 1);
            std::getline(stream, rest);

            if (!rest.empty() && rest[0] == ' ')
                rest.erase(0, 1);

            if (!rest.empty())
                word += " " + rest;

            command.args.push_back(word);
            break;
        }

        command.args.push_back(word);
    }

    return command;
}