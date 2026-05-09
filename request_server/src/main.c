#include "../include/gui.h"
#include "../include/ipc.h"
#include "../include/generator.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

static void sigintHandler(int sig) {
    (void)sig;
    g_sigintReceived = 1;
}

int main(void) {
    struct sigaction sa = {0};
    sa.sa_handler = sigintHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESETHAND;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    printf("REQUEST SERVER --- Starting Dashboard Process...\n");

    // connect to IPC resources created by the main server.
    // ensure the main server is already running.
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
    runGui();

    // cleanup 
    printf("REQUEST SERVER --- Shutting down...\n");
    pipeCleanup();
    shmCleanup();
    shmLockCleanup();

    return 0;
}