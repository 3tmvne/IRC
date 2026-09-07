#ifndef PARSER_HPP
#define PARSER_HPP

#include <string>
#include <vector>
#include <sstream>

// One parsed IRC line:  [":" prefix SPACE] command *( SPACE param ) [SPACE ":" trailing]
// The trailing parameter is merged into args.back(), so handlers never have to
// care whether the last argument was written with a colon or not.
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
		static std::string	toUpper(const std::string &str);
};

#endif
