CC = gcc
FLAGS = -pthread -lraylib -lm
SRC = src/*.c
INC = -Iinclude

all:
	$(CC) $(INC) $(FLAGS) $(SRC) -o rideshare
