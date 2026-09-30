
CC=gcc
FLAGS= -Wall -Wextra -Werror -g -fsanitize=address

all:
	$(CC) $(FLAGS) train.c -o train

CC=gcc
CFLAGS= -Wall -Wextra -Werror -g -fsanitize=address

INC= -I.
HEADERS=linreg.h


%.o:%.c  $(HEADERS) Makefile
	$(CC) $(CFLAGS) $(INC)  -c $< $(LDFLAGS) -o $@


train: train.o graph.o
	$(CC) $(CFLAGS) $(INC) $(LDFLAGS) train.o graph.o -o train

predict: predict.o
	$(CC) $(CFLAGS) $(INC) $(LDFLAGS) -o $@

all: train predict

clean:
	$(RM) $(OBJ)

fclean: clean
	$(RM) $(NAME)

re: fclean all

.PHONY: all fclean clean re test
