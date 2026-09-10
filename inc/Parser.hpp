#ifndef PARSER_HPP
#define PARSER_HPP

#include <string>
#include <vector>
#include <sstream>

struct Command
{
	std::string					prefix;
	std::string					cmd;
	std::vector<std::string>	args;
};

class Parser
{
	public:
		Parser();
		Parser(const Parser &other);
		Parser &operator=(const Parser &other);
		~Parser();

		Command	parse(const std::string &line) const;

	private:
		static std::string	toupper(const std::string &str);
};

#endif
