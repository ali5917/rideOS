#include "../include/gui.h"
#include "../include/ipc.h"
#include "../include/generator.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

// TODO: Main loop: Render UI, handle user input.
// TODO: Close Raylib window.

int main() {
    printf("REQUEST SERVER --- Starting...\n");

    // initialize IPC (pipe for writing, shm for reading)
    // assumes that the main server is running already to create the SHM
    if (initSharedMemory(NULL) != 0) {
        fprintf(stderr, "Could not connect to Shared Memory. Is Main Server running?\n");
        return 1;
    }
    if (initShmLock() != 0) {
        fprintf(stderr, "Could not connect to Shm Lock.\n");
        return 1;
    }
    if (initRequestPipe() != 0) {
        fprintf(stderr, "Could not open Request Pipe.\n");
        return 1;
    }

    // start the automatic request generator thread
    pthread_t generatorTid;
    if (pthread_create(&generatorTid, NULL, generatorLoop, NULL) != 0) {
        perror("Failed to start generator thread");
        return 1;
    }
    pthread_detach(generatorTid);

    // cleanup 
    printf("REQUEST SERVER --- Shutting down...\n");
    pipeCleanup();
    shmCleanup();
    shmLockCleanup();

    return 0;
}