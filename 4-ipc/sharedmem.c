// Producer

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

int main() {
    const int size = 4096;
    const char *name = "/OS";
    int fd;
    char *ptr;

    // 1. Create shared memory object
    fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    // 2. Set size
    if (ftruncate(fd, size) == -1) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }

    // 3. Map memory
    ptr = (char *)mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    // 4. Write data
    sprintf(ptr, "Hello World!");
    printf("Producer wrote: %s\n", ptr);

    // 5. Cleanup (no unlink here so consumer can read)
    munmap(ptr, size);
    close(fd);

    return 0;
}

// Consumer

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>


int main() {
    const int size = 4096;
    const char *name = "/OS";
    int fd;
    char *ptr;

    // 1. Open existing shared memory
    fd = shm_open(name, O_RDONLY, 0666);
    if (fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    // 2. Map memory
    ptr = (char *)mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    // 3. Read data
    printf("Consumer read: %s\n", ptr);

    // 4. Cleanup
    munmap(ptr, size);
    close(fd);

    // Remove shared memory after use
    shm_unlink(name);

    return 0;
}