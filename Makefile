CC = gcc
FLAGS = -pthread -lraylib -lm -lrt
INC_MAIN = -Imain_server/include
INC_REQ = -Irequest_server/include

all: main_server_bin request_server_bin

main_server_bin:
	$(CC) $(INC_MAIN) main_server/src/*.c -o main_server_bin $(FLAGS)

request_server_bin:
	$(CC) $(INC_REQ) request_server/src/*.c -o request_server_bin $(FLAGS)

clean:
	rm -f main_server_bin request_server_bin
