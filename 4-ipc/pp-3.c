// An echo server echoes back whatever it receives from a client through shared memory
// after reversing each character in the message. 
// if a client sends the server the string “operating system”, the server will print “metsys gnitarepo”.

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
    const char *name = "/OS";
    const int size = 4096;

    int fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open");
    }

    if (ftruncate(fd, size) == -1) {
        perror("ftruncate");    
    }

    char* ptr = (char*) mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
    }

    // client
    char *sentMsg = "operating system";
    sprintf(ptr, "%s", sentMsg);

    // server
    char recivedMsg[BUFSIZ];
    strcpy(recivedMsg, ptr);
    
    int length = strlen(recivedMsg);
    char newString[BUFSIZ];
    int j = 0;
    for (int i = length - 1; i >= 0; i--) {
        newString[j++] = recivedMsg[i];
    }
    newString[j] = '\0';
    printf("%s", newString);

    munmap(ptr, size);
    close(fd);
    shm_unlink(name);
}