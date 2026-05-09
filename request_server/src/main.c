#include "../include/gui.h"
#include "../include/ipc.h"
#include "../include/generator.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("REQUEST SERVER --- Starting Dashboard Process...\n");

    // 1. Connect to IPC resources created by the Main Server.
    //    Make sure the Main Server is already running before launching this.
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

    // 2. Enter the GUI.
    //    The GUI state machine handles:
    //      - Intro screen
    //      - Config / driver-count selection screen
    //      - Starting the request generator thread (after config is chosen)
    //      - Live dashboard
    //      - Post-simulation metrics screen
    //    runGui() blocks until the window is closed.
    runGui();

    // 3. Cleanup (generator is already stopped inside runGui before it returns)
    printf("REQUEST SERVER --- Shutting down...\n");
    pipeCleanup();
    shmCleanup();
    shmLockCleanup();

    return 0;
}