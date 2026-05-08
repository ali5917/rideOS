// Create a C program named numbercopy.c that transfers data between processes using a pipe. 
// The program takes two arguments: 
//      an input file containing a list of numbers 
//      an output file where the numbers will be copied. 
// The parent process reads the numbers from the input file and writes them to the pipe
// The child process retrieves the numbers from the pipe and writes them to the output file. 
// Ensure proper error handling.

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

int main (int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: ./%s source_file, destination_file.\n", argv[0]);
        fprintf(stderr, "Usage: %s source_file destination_file\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    const char* fifoFile = "/tmp/myfifo";
    ssize_t bytesRead;

    if (mkfifo(fifoFile, 0666) == -1) {
        perror("mkfifo");
        exit(EXIT_FAILURE);
    } 

    pid_t pid = fork();

    if (pid == 0) {
        // writing in output file
        int fd = open(fifoFile, O_RDONLY);
        int outFd = open(argv[2], O_CREAT | O_WRONLY, 0644);
        if (outFd == -1) {
            perror("output file open");
            exit(EXIT_FAILURE);
        }

        char buffer[BUFSIZ];
        while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0) {
            write(outFd, buffer, bytesRead);
        }

        close(outFd);
        close(fd);
    } else {
        // reading input from file
        int fd = open(fifoFile, O_WRONLY);
        int inFd = open(argv[1], O_RDONLY);
        if (inFd == -1) {
            perror("input file open");
            exit(EXIT_FAILURE);
        }

        char buffer[BUFSIZ];
        while ((bytesRead = read(inFd, buffer, sizeof(buffer))) > 0) {
            write(fd, buffer, bytesRead);
        }

        close(inFd);
        close(fd);
        
        wait(NULL);
        unlink(fifoFile);
    }
    return 0;
}