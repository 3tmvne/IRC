#include "Parcer.hpp"
#include <iostream>


//server.getPassword();
//server.nicknameExists(...);
//server.findClientByNickname(...);
//server.sendMessage(...);


int main()
{
    Parcer parser;

    Command cmd = parser.parcing(":uy USER abdel 0 * :Abdel Haq h sjjs sdksdksd sd");

    std::cout << "Prefix: " << cmd.prefix << std::endl;
    std::cout << "Command: " << cmd.cmd << std::endl;

    for (size_t i = 0; i < cmd.args.size(); i++)
        std::cout << "args[" << i << "] = " << cmd.args[i] << std::endl;

    return 0;
}