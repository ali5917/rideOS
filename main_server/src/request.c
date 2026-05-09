#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include "../include/request.h"
#include "../include/queue.h"
#include "../include/driver.h"
#include "../include/ipc.h"
#include "../include/logger.h"
#include "../include/metrics.h"
#include "../include/state.h"

// Global references defined in main.c
extern PriorityQueue requestQueue;
extern pthread_mutex_t driverMutex;
extern Metrics metrics;

void* requestThread(void* arg) {
    RideRequest* req = (RideRequest*)arg;
    
    req->requestTime = time(NULL);          // assign the current time
    req->status = REQUEST_WAITING;

    metricsRecordCreated(&metrics, req);
    loggerLogEvent("REQUEST_CREATED", req->id, NULL);
    
    // insert into the priority queue and signal dispatcher
    insertRequest(&requestQueue, req);
    updateSharedState();
    
    // calculate timeout deadline
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += req->timeoutSeconds;

    pthread_mutex_lock(&req->waitMutex);
    while (req->status == REQUEST_WAITING) {
        int rc = pthread_cond_timedwait(&req->assignedCond, &req->waitMutex, &ts);
        
        if (rc == ETIMEDOUT) {
            if (req->status == REQUEST_WAITING) {
                req->status = REQUEST_CANCELLED;
                printf("TIMEOUT --- Request #%d timed out after %ds and cancelled itself.\n", 
                       req->id, req->timeoutSeconds);
                  metricsRecordCancelled(&metrics, req);
                  loggerLogEvent("REQUEST_CANCELLED", req->id, "timeout");
                updateSharedState();
            }
            break;
        }
    }
    pthread_mutex_unlock(&req->waitMutex);
    pthread_exit(0);
}

void* rideThread(void* arg) {
    RideRequest* req = (RideRequest*)arg;

    // simulate the ride
    printf("RIDE --- Started Request #%d | Driver #%d | Duration: %ds\n", 
           req->id, req->assignedDriverId, req->rideDuration);
    
    sleep(req->rideDuration);

    // release the driver
    pthread_mutex_lock(&driverMutex);
    for (int i = 0; i < numDrivers; i++) {
        if (driverPool[i].ID == req->assignedDriverId) {
            driverPool[i].status = DRIVER_ONLINE;
            driverPool[i].ridesCompleted++;
            driverPool[i].currentRequestID = -1;
            break;
        }
    }
    pthread_mutex_unlock(&driverMutex);

    req->status = REQUEST_COMPLETED;
    printf("COMPLETED --- Request #%d finished. Driver #%d is now ONLINE.\n", 
           req->id, req->assignedDriverId);

        metricsRecordCompleted(&metrics, req);
        loggerLogEvent("REQUEST_COMPLETED", req->id, NULL);
        updateSharedState();

    destroyRequest(req);

    pthread_exit(0);
}

void destroyRequest(RideRequest* req) {
    if (req == NULL) {
        return;
    }
    pthread_mutex_destroy(&req->waitMutex);
    pthread_cond_destroy(&req->assignedCond);
    free(req);
}