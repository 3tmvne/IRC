#include "Parser.hpp"

Parser::Parser()
{
}

Parser::Parser(const Parser &other)
{
	*this = other;
}

Parser &Parser::operator=(const Parser &other)
{
	(void)other;
	return (*this);
}

Parser::~Parser()
{
}

std::string Parser::toupper(const std::string &str)
{
	std::string	result = str;

	for (size_t i = 0; i < result.size(); i++)
	{
		if (result[i] >= 'a' && result[i] <= 'z')
			result[i] = static_cast<char>(result[i] - 32);
	}
	return (result);
}

Command Parser::parse(const std::string &line) const
{
	Command				command;
	std::istringstream	stream(line);
	std::string			word;

	stream >> word;

	
	
	if (!word.empty() && word[0] == ':')
	{
		command.prefix = word.substr(1);
		word.clear();
		stream >> word;
	}

	
	command.cmd = toupper(word);

	while (stream >> word)
	{
		if (!word.empty() && word[0] == ':')
		{
			
			std::string	rest;

			word.erase(0, 1);
			std::getline(stream, rest);
			if (!rest.empty() && rest[0] == ' ')
				rest.erase(0, 1);
			if (!rest.empty())
				word += " " + rest;
			command.args.push_back(word);
			break ;
		}
		command.args.push_back(word);
	}
	return (command);
}
