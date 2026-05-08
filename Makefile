CC = gcc
FLAGS = -pthread -lraylib -lX11 -lXrandr -lXinerama -lXi -lXcursor -lm -ldl -lrt
INC_SHARED = -Ishared/include
INC_MAIN = -Imain_server/include $(INC_SHARED)
INC_REQ = -Irequest_server/include $(INC_SHARED)

all: main_server_bin request_server_bin

main_server_bin:
	$(CC) $(INC_MAIN) main_server/src/*.c -o main_server_bin $(FLAGS)

request_server_bin:
	$(CC) $(INC_REQ) request_server/src/*.c -o request_server_bin $(FLAGS)

clean:
	rm -f main_server_bin request_server_bin
