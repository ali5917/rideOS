#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>

#include "../include/dispatch.h"
#include "../include/driver.h"
#include "../include/queue.h"
#include "../include/request.h"
#include "../include/ipc.h"
#include "../include/logger.h"
#include "../include/metrics.h"
#include "../include/state.h"
#include "../include/config.h"

// Define global variables
PriorityQueue requestQueue;
Driver driverPool[MAX_DRIVERS];
int numDrivers = 10;
pthread_mutex_t driverMutex;

Metrics metrics;

SharedState *sharedState = NULL;

volatile sig_atomic_t systemRunning = 1;

// signal handler
void handleShutdown(int sig) {
    const char *msg = "\nMAIN --- Signal received, shutting down...\n";
    write(STDOUT_FILENO, msg, strlen(msg)); 
    systemRunning = 0;
}

int main(int argc, char *argv[]) {
    // setting up signal handler for Ctrl + C 
    struct sigaction sa = {0};
    sa.sa_handler = handleShutdown;
    sigaction(SIGINT, &sa, NULL);

    printf("MAIN --- Starting Main Server\n");

    // initialize core data structures
    initializeRequestQueue(&requestQueue);
    pthread_mutex_init(&driverMutex, NULL);
    metricsInit(&metrics);
    configInit(&config);
    configLoadFile(&config, "config.txt");
    configApplyArgs(&config, argc, argv);

    if (config.numDrivers > MAX_DRIVERS) {
        config.numDrivers = MAX_DRIVERS;
    }
    numDrivers = config.numDrivers;

    if (loggerInit("rideos.log") != 0) {
        fprintf(stderr, "MAIN --- Failed to initialize logger.\n");
    }
    
    // initialize drivers
    for (int i = 0; i < numDrivers; i++) {
        driverPool[i].ID = i + 1;
        driverPool[i].status = DRIVER_ONLINE;
        
        // assign categories (PLUS is a smaller subset)
        if (i % 3 == 0) driverPool[i].category = DRIVER_PLUS;
        else driverPool[i].category = DRIVER_STANDARD;
        
        driverPool[i].currentRequestID = -1;
        driverPool[i].ridesCompleted = 0;
        driverPool[i].lastStatusChange = time(NULL);
    }

    // initialize IPC
    if (initRequestPipe() == -1) {
        fprintf(stderr, "MAIN --- Failed to initialize request pipe.\n");
        return EXIT_FAILURE;
    }
    if (initSharedMemory(&sharedState) == -1) {
        fprintf(stderr, "MAIN --- Failed to initialize shared memory.\n");
        return EXIT_FAILURE;
    }
    if (initShmLock() == -1) {
        fprintf(stderr, "MAIN --- Failed to initialize shared memory lock.\n");
        return EXIT_FAILURE;
    }

    // start the dispatcher thread
    pthread_t dispatcherTid;
    if (pthread_create(&dispatcherTid, NULL, dispatcherThread, NULL) != 0) {
        perror("MAIN --- Failed to create dispatcher thread");
        return EXIT_FAILURE;
    }

    printf("MAIN --- Dispatcher started. Waiting for requests...\n");

    // main loop - read from pipe and spawn request threads
    while (systemRunning) {
        PipeRequest msg = {0};
        // readPipeRequest returns 0 on success, 1 on "no data"
        int res = readPipeRequest(&msg);

        if (res == 0) {
            if (msg.msgType == PIPE_MSG_CONFIG) {
                int desired = msg.configDrivers;
                if (desired > MAX_DRIVERS) desired = MAX_DRIVERS;
                if (desired < 1) desired = 1;

                pthread_mutex_lock(&driverMutex);
                config.numDrivers = desired;
                numDrivers = desired;
                for (int i = 0; i < numDrivers; i++) {
                    driverPool[i].ID = i + 1;
                    driverPool[i].status = DRIVER_ONLINE;
                    driverPool[i].category = (i % 3 == 0) ? DRIVER_PLUS : DRIVER_STANDARD;
                    driverPool[i].currentRequestID = -1;
                    driverPool[i].ridesCompleted = 0;
                    driverPool[i].lastStatusChange = time(NULL);
                }
                pthread_mutex_unlock(&driverMutex);

                printf("MAIN --- Updated driver count: %d\n", numDrivers);
                updateSharedState();
                usleep(100000);
                continue;
            }

            RideRequest *newReq = malloc(sizeof(RideRequest));
            if (!newReq) {
                perror("MAIN --- Failed to allocate memory for request");
                continue;
            }

            pthread_mutex_init(&newReq->waitMutex, NULL);
            pthread_cond_init(&newReq->assignedCond, NULL);

            memset(newReq, 0, sizeof(RideRequest));
            newReq->id = msg.id;
            newReq->type = msg.type;
            newReq->originalType = msg.type;
            newReq->status = REQUEST_WAITING;
            newReq->assignedDriverId = -1;
            newReq->fare = (float)msg.baseFare;
            newReq->rideDuration = msg.rideDuration;
            newReq->requestTime = msg.requestTime;
            newReq->timeoutSeconds = configGetTimeout(&config, msg.type);
            newReq->deferredCount = 0;

            printf("MAIN --- Received Ride Request #%d.\n", newReq->id);

            pthread_t reqTid;
            if (pthread_create(&reqTid, NULL, requestThread, (void*)newReq) != 0) {
                perror("MAIN --- Failed to spawn request thread");
                destroyRequest(newReq);
            } else {
                pthread_detach(reqTid);
            }
        } else if (res == 1) {
            usleep(100000);
        } else {
            if (systemRunning) {
                fprintf(stderr, "MAIN --- Pipe error, retrying.\n");
                sleep(1);
            }
        }
    }

    updateSharedState();

    // cleanup
    printf("MAIN --- Waiting for threads to finish...\n");
    wakeAllRequests(&requestQueue);
    pthread_join(dispatcherTid, NULL);
    
    destroyRequestQueue(&requestQueue);
    pthread_mutex_destroy(&driverMutex);

    metricsReport(&metrics, numDrivers);
    loggerClose();
    
    pipeCleanup();
    shmCleanup();
    shmLockCleanup();

    printf("MAIN --- Shutdown complete.\n");
    return EXIT_SUCCESS;
}