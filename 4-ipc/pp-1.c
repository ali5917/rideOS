// Implement a program using pipes for parent and child processes. 
// The parent sends an integer array to the child, which computes the median and sends it back. 
// The parent then computes the factorial of the median and displays it. 
// Use proper synchronization

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

int n = 5;
ssize_t bytesSent, bytesRead;

int main () {
    int fd1[2]; 
    int fd2[2];

    if (pipe(fd1) == -1) {
        perror("Pipe 1 Failed");
        exit(EXIT_FAILURE);
    }

    if (pipe(fd2) == -1) {
        perror("Pipe 2 Failed");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();

    if (pid == 0) {
        close(fd1[1]);
        close(fd2[0]);

        int arr[5];
        bytesRead = read(fd1[0], arr, n * sizeof(int));
        if (bytesRead == -1) {
            perror("read");
        }

        close(fd1[0]);

        // sorting then taking median
        for (int i = 0; i < n - 1; i++) {
            for (int j = 0; j < n - i - 1; j++) {
                if (arr[j] > arr[j + 1]) {
                    int temp = arr[j];
                    arr[j] = arr[j + 1];
                    arr[j + 1] = temp;
                }
            }
        }
        
        int median = arr[n/2];

        bytesSent = write(fd2[1], &median, sizeof(int));
        if (bytesSent == -1) {
            perror("write");
        }

        close(fd2[1]);

    } else {
        // sends integer array
        close (fd1[0]);
        close (fd2[1]);

        int arr[5] = {2,4,6,8,9};
        bytesSent = write(fd1[1], arr, n * sizeof(int));
        if (bytesSent == -1) {
            perror("write");
            exit(EXIT_FAILURE);
        }

        close(fd1[1]);

        // receives median
        int median;
        bytesRead = read(fd2[0], &median, sizeof(int));
        if (bytesRead == -1) {
            perror("read");
        }
        printf("Median: %d\n", median);

        // computes factorial
        int factorial = 1;
        if (median == 0) {
            factorial = 1;
        } else {
            for (int i = 1; i <= median; i++) {
                factorial *= i;
            }
        }

        printf("Factorial: %d", factorial);

        close(fd2[0]);

        wait(NULL);
    }
    return 0;
}