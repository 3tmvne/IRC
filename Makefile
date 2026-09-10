NAME		:= ircserv

CXX			:= c++
CXXFLAGS	:= -Wall -Wextra -Werror -std=c++98 -Iinc 

SRCDIR		:= src
OBJDIR		:= obj

SRCS		:= $(SRCDIR)/main.cpp \
			   $(SRCDIR)/Server.cpp \
			   $(SRCDIR)/Client.cpp \
			   $(SRCDIR)/Channel.cpp \
			   $(SRCDIR)/Parser.cpp \
			   $(SRCDIR)/commands/Registration.cpp \
			   $(SRCDIR)/commands/Messaging.cpp \
			   $(SRCDIR)/commands/ChannelCmds.cpp

OBJS		:= $(SRCS:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
DEPS		:= $(OBJS:.o=.d)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

# -MMD -MP writes a .d per object so touching a header rebuilds exactly the
# translation units that include it, and nothing else.
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

-include $(DEPS)

.PHONY: all clean fclean re
