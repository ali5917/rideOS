#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include "../include/request.h"
#include "../include/queue.h"
#include "../include/driver.h"
#include "../include/ipc.h"

// Global references defined in main.c
extern PriorityQueue requestQueue;
extern pthread_mutex_t driverMutex;

void* requestThread(void* arg) {
    RideRequest* req = (RideRequest*)arg;
    
    req->requestTime = time(NULL);          // assign the current time
    req->status = REQUEST_WAITING;
    
    // insert into the priority queue and signal dispatcher
    insertRequest(&requestQueue, req);
    
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
            }
            break;
        }
    }
    pthread_mutex_unlock(&req->waitMutex);

    // If assigned, the thread exits normally. 
    // If cancelled, it might need to notify metrics (handled by friend's logger/metrics).
    pthread_exit(0);
}

void* rideThread(void* arg) {
    RideRequest* req = (RideRequest*)arg;

    // Simulate the ride
    printf("RIDE --- Started Request #%d | Driver #%d | Duration: %ds\n", 
           req->id, req->assignedDriverId, req->rideDuration);
    
    sleep(req->rideDuration);

    // Release the driver (use assignedDriverId as pool index)
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

    // TODO: Call friend's update_metrics() and log_event() here
    // TODO: Trigger Shared Memory update via IPC

    pthread_exit(0);
}