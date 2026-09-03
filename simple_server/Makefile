NAME     = simple_serv
CXX      = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

all: $(NAME)

$(NAME): simple_serv.cpp
	$(CXX) $(CXXFLAGS) simple_serv.cpp -o $(NAME)

clean:
	rm -f simple_serv.o

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
