# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: apestana <apestana@student.42malaga.com    +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2026/08/22 13:50:13 by apestana          #+#    #+#              #
#    Updated: 2026/09/17 00:10:22 by apestana         ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

##### Name #################################

NAME		= ircserv

##### Compiler and flags ###################

CXX			= c++
CXXFLAGS	= -Wall -Wextra -Werror -std=c++98

# Generate dependency files automatically
CPPFLAGS	= -MMD -MP

##### Directories ##########################

SRCDIR		= src
INCDIR		= includes
OBJDIR		= obj

##### Sources ################################

SRC			=	main.cpp \
				server/Server.cpp \
				server/ServerNetwork.cpp \
				server/ServerLog.cpp \
				server/ServerClients.cpp \
				server/ServerChannels.cpp \
				server/ServerProtocol.cpp \
				commands/Pass.cpp \
				commands/Nick.cpp \
				commands/User.cpp \
				commands/Join.cpp \
				commands/Part.cpp \
				commands/Invite.cpp \
				commands/Privmsg.cpp \
				commands/Quit.cpp \
				commands/Kick.cpp \
				commands/Topic.cpp \
				commands/Mode.cpp \
				commands/Ping.cpp \
				commands/Pong.cpp \
				commands/Who.cpp \
				commands/Cap.cpp \
				IrcParameters.cpp \
				Client.cpp \
				IrcMessage.cpp \
				Channel.cpp

SRCS		= $(addprefix $(SRCDIR)/,$(SRC))
OBJ			= $(addprefix $(OBJDIR)/,$(SRC:.cpp=.o))
DEP			= $(OBJ:.o=.d)

##### Rules #################################

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(NAME)

# Create the required object directories
$(OBJDIR):
	mkdir -p $(OBJDIR)

# Compile source files and preserve the directory structure
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -I$(INCDIR) -c $< -o $@

##### Cleaning ##############################

clean:
	rm -rf $(OBJDIR)

fclean: clean
	rm -f $(NAME)

re: fclean
	$(MAKE) all

##### Dependencies ##########################

# Include generated dependency files so modified headers trigger recompilation
-include $(DEP)

.PHONY: all clean fclean re
