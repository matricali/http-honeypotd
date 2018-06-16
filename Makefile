CC	= gcc

CFLAGS	+= -Wall -g -std=gnu99 -O3
LDFLAGS	+= -lcurl

NAME	= http-honeypotd
SRCS	= http-honeypotd.c
OBJS	= $(SRCS:.c=.o)

SERVER_HEADER ?= "jorgee"

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) -D SERVER_HEADER=$(SERVER_HEADER) -o $(NAME) $(OBJS) $(LDFLAGS)

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all
