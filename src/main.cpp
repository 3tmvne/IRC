#include "Server.hpp"
#include <cstdlib>

volatile sig_atomic_t	g_stop = 0;

static void	handleSignal(int signum)
{
	(void)signum;
	g_stop = 1;
}

// The port must be a plain decimal number in the unprivileged range; anything
// else is rejected up front rather than failing obscurely inside bind().
static bool	parsePort(const std::string &arg, int &port)
{
	if (arg.empty() || arg.size() > 5)
		return (false);
	for (size_t i = 0; i < arg.size(); i++)
	{
		if (arg[i] < '0' || arg[i] > '9')
			return (false);
	}
	port = std::atoi(arg.c_str());
	return (port >= 1024 && port <= 65535);
}

int	main(int argc, char **argv)
{
	if (argc != 3)
	{
		std::cerr << "usage: " << argv[0] << " <port> <password>" << std::endl;
		return (1);
	}

	int	port = 0;
	if (!parsePort(argv[1], port))
	{
		std::cerr << "error: port must be a number between 1024 and 65535" << std::endl;
		return (1);
	}

	std::string	password(argv[2]);
	if (password.empty())
	{
		std::cerr << "error: password must not be empty" << std::endl;
		return (1);
	}

	// Writing to a socket whose peer has vanished raises SIGPIPE, whose default
	// action kills the process. Ignoring it turns that into a send() error the
	// event loop already knows how to handle.
	std::signal(SIGPIPE, SIG_IGN);
	std::signal(SIGINT, handleSignal);
	std::signal(SIGTERM, handleSignal);

	try
	{
		Server	server(port, password);
		server.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << "error: " << e.what() << std::endl;
		return (1);
	}
	return (0);
}
