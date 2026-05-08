#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main() {
    int fd[2];                          // fd[0] = read end, fd[1] = write end
    pid_t pid;
    char write_msg[] = "Hello from parent!";
    char read_msg[100];

    // Create the pipe
    if (pipe(fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // Create child process
    pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        // Child process: read from pipe
        close(fd[1]);                          // Close unused write end
        int n = read(fd[0], read_msg, sizeof(read_msg));
        read_msg[n] = '\0'
        printf("Child received: %s\n", read_msg);
        close(fd[0]);                          // Close read end
        exit(EXIT_SUCCESS);
    } else {
        // Parent process: write to pipe
        close(fd[0]);                          // Close unused read end
        write(fd[1], write_msg, strlen(write_msg) + 1);
        close(fd[1]);                          // Close write end
        wait(NULL);                            // Wait for child
    }

    return 0;
}