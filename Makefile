CC=gcc
CFLAGS= -Wall -Wextra -Werror -g -fsanitize=address

INC= -I.
HEADERS=linreg.h


OBJ_TRAIN=train.o graph.o util.o
OBJ_PREDICT=util.o predict.o

%.o:%.c  $(HEADERS) Makefile
	$(CC) $(CFLAGS) $(INC)  -c $< $(LDFLAGS) -o $@


train: $(OBJ_TRAIN)
	$(CC) $(CFLAGS) $(INC) $(LDFLAGS) $(OBJ_TRAIN) -o train

predict: $(OBJ_PREDICT)
	$(CC) $(CFLAGS) $(INC) $(LDFLAGS) $(OBJ_PREDICT) -o predict

all: train predict

clean:
	$(RM) $(OBJ_TRAIN) $(OBJ_PREDICT)

fclean: clean
	$(RM) ./train ./predict ./vars.csv

re: fclean all

.PHONY: all fclean clean re test
