#include "../include/gui.h"
#include "../include/ipc.h"
#include "../include/generator.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("REQUEST SERVER --- Starting Dashboard Process...\n");

    // 1. Initialize IPC (Assumes Main Server is already up)
    if (initSharedMemory(NULL) != 0) {
        fprintf(stderr, "Could not connect to Shared Memory. Is the Main Server running?\n");
        return 1;
    }
    if (initShmLock() != 0) {
        fprintf(stderr, "Could not connect to Shared Memory Lock.\n");
        return 1;
    }
    if (initRequestPipe() != 0) {
        fprintf(stderr, "Could not open Request Pipe.\n");
        return 1;
    }

    // 2. Start Request Generator in a background thread
    pthread_t generatorTid;
    if (pthread_create(&generatorTid, NULL, generatorLoop, NULL) != 0) {
        perror("Failed to launch generator thread");
        return 1;
    }

    // 3. Enter GUI Main Loop (Blocks until window closed)
    runGui();

    // 4. Cleanup on exit
    printf("REQUEST SERVER --- Shutting down...\n");
    generatorStop();
    pthread_join(generatorTid, NULL);
    pipeCleanup();
    shmCleanup();
    shmLockCleanup();

    return 0;
}