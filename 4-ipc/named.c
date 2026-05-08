// Producer 

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#define FIFO_FILE "/tmp/myfifo" 

int main() {
    int fd;
    char buffer[BUFSIZ];
    ssize_t num_bytes;

    mkfifo(FIFO_FILE, 0666); // Create the named pipe 
    fd = open(FIFO_FILE, O_WRONLY); // Open for writing
    
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    while (1) { 
        printf("Producer: Enter a message (or 'exit' to quit): ");
        fgets(buffer, BUFSIZ, stdin); 
        num_bytes = write(fd, buffer, strlen(buffer));
        if (num_bytes == -1) {
               perror("write");
               exit(EXIT_FAILURE);
        }

        if (strncmp(buffer, "exit", 4) == 0) { // Check for exit 
            break;
        }
    }

    close(fd); 
    unlink(FIFO_FILE); // Remove pipe from filesystem
    return 0;
}

// Consumer 

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#define FIFO_FILE "/tmp/myfifo"

int main() {
    char buffer[BUFSIZ];
    ssize_t num_bytes;

    // Open the named pipe for reading
    int fd = open(FIFO_FILE, O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    printf("Consumer: Waiting for messages...\n");

    while (1) {
        // Read from the pipe
        num_bytes = read(fd, buffer, BUFSIZ);
        if (num_bytes == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        buffer[num_bytes] = '\0'; // Null-terminate the string

        printf("Consumer received: %s\n", buffer);

        // Exit condition
        if (strncmp(buffer, "exit", 4) == 0) {
            break;
        }
    }

    close(fd);
    return 0;
}