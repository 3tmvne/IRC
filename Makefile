NAME = ircserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -Iinc 

SRCDIR = src
OBJDIR = obj

SRCS = $(SRCDIR)/main.cpp \
		$(SRCDIR)/Server.cpp \
		$(SRCDIR)/Client.cpp \
		$(SRCDIR)/Channel.cpp \
		$(SRCDIR)/Parser.cpp \
		$(SRCDIR)/commands/Registration.cpp \
		$(SRCDIR)/commands/Messaging.cpp \
		$(SRCDIR)/commands/ChannelCmds.cpp

OBJS = $(SRCS:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
HEADERS = $(wildcard inc/*.hpp)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp $(HEADERS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
