// Create a program in C that sets up a shared memory segment.
// The child process should write a message ("Hello from child") into the shared memory.
// The parent process should read the message from the shared memory and display it.

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
    const int size = 4096;
    
    int fd = shm_open("/OS", O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }
        
    if (ftruncate(fd, size) == -1) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }

    char *ptr = (char *) mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
            perror("mmap");
            exit(EXIT_FAILURE);
    }
    
    pid_t pid = fork();
    if (pid == 0) {
        // child process
        char *msg = "Hello from Child!\n";
        sprintf(ptr, "%s", msg);

        munmap(ptr, size);
        close(fd);

    } else {
        // parent process
        wait(NULL);
        printf("Read message: %s", ptr);

        munmap(ptr, size);
        close(fd);
        
        shm_unlink("/OS");
    }
}