// Create a named pipe called myfifo.
// The child process should send a message to the parent process through this named pipe.
// The parent process should read the message from the named pipe and display it on the screen.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

int main () {
    const char* fifoFile = "/tmp/myfifo";
    char *msg = "Hello World!";
    char readMsg [BUFSIZ];


    if (mkfifo(fifoFile, 0666) == -1) {
        perror("Failed to create mkfifo");
        exit(EXIT_FAILURE);
    }
    
    pid_t pid = fork();
    
    if (pid == 0) {
        int fd = open(fifoFile, O_WRONLY);
        if (fd == -1) {
            perror("Failed to open fifo file to write");
            exit(EXIT_FAILURE);
        }

        ssize_t bytesWritten = write(fd, msg, strlen(msg));
        
        if (bytesWritten == -1) {
            perror("Couldn't write in fifo file");
            exit(EXIT_FAILURE);
        }
        
        close(fd);

    } else {      
        int fd = open(fifoFile, O_RDONLY);
        
        if (fd == -1) {
            perror("Failed to open fifo file to read");
            exit(EXIT_FAILURE);
        }

        ssize_t bytesRead = read(fd, readMsg, sizeof(readMsg));
        
        if (bytesRead == -1) {
            perror("Couldn't read fifo file");
            exit(EXIT_FAILURE);
        }
        
        readMsg[bytesRead] = '\0';
        printf("Read message: %s", readMsg);
        
        unlink(fifoFile);
        close(fd);
    }
}