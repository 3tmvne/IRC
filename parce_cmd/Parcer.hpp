#ifndef PARCER_HPP
#define PARCER_HPP

#include<vector>
#include<string>
#include<sstream>

struct Command
{
    std::string prefix;
    std::string cmd;
    std::vector<std::string> args;
};

class   Parcer 
{
    public:
        Parcer();
        ~Parcer();
        Command parcing(const std::string &line);
};

 
#endif