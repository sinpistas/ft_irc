# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: vbullock <vbullock@student.42.fr>          +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2026/08/22 13:50:13 by apestana          #+#    #+#              #
#    Updated: 2026/09/04 19:19:28 by vbullock         ###   ########.fr        #
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
				Server.cpp \
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

re: fclean all

##### Dependencies ##########################

# Include generated dependency files so modified headers trigger recompilation
-include $(DEP)

.PHONY: all clean fclean re