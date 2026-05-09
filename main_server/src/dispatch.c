#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "../include/dispatch.h"
#include "../include/driver.h"
#include "../include/queue.h"
#include "../include/request.h"
#include "../include/ipc.h"
#include "../include/logger.h"
#include "../include/state.h"

// External globals from main.c
extern PriorityQueue requestQueue;
extern Driver driverPool[MAX_DRIVERS];
extern int numDrivers;
extern pthread_mutex_t driverMutex;
extern volatile sig_atomic_t systemRunning;

static const char* getRequestTypeString(RequestType type) {
    switch (type) {
        case EMERGENCY: return "EMERGENCY";
        case VIP:       return "VIP";
        case NORMAL:    return "NORMAL";
        default:        return "UNKNOWN";
    }
}

void* dispatcherThread(void* arg) {
    printf("DISPATCHER --- Thread started.\n");

    while (systemRunning) {
        // get the request 
        RideRequest* req = getRequest(&requestQueue, &systemRunning);
        if (req == NULL) {
            break;
        }

        pthread_mutex_lock(&req->waitMutex);
        if (req->status == REQUEST_CANCELLED) {
            pthread_mutex_unlock(&req->waitMutex);
            printf("DISPATCHER --- Skipping cancelled Request #%d\n", req->id);
            loggerLogEvent("REQUEST_CANCELLED", req->id, "dispatcher_skip");
            destroyRequest(req);
            continue;
        }
        pthread_mutex_unlock(&req->waitMutex);

        // find a suitable driver
        int foundDriverIndex = -1;
        pthread_mutex_lock(&driverMutex);

        foundDriverIndex = driverFindAvailable(req->type);

        if (foundDriverIndex != -1) {
            // assign the driver
            Driver* driver = &driverPool[foundDriverIndex];
            driver->status = DRIVER_BUSY;
            driver->currentRequestID = req->id;
            driver->lastAssignedTime = time(NULL);
            req->assignedTime = time(NULL);

            // update request status
            pthread_mutex_lock(&req->waitMutex);
            if (req->status == REQUEST_CANCELLED) {
                pthread_mutex_unlock(&req->waitMutex);
                driver->status = DRIVER_ONLINE;
                driver->currentRequestID = -1;
                pthread_mutex_unlock(&driverMutex);
                loggerLogEvent("REQUEST_CANCELLED", req->id, "late_cancel");
                destroyRequest(req);
                continue;
            }
            req->status = REQUEST_ASSIGNED;
            req->assignedDriverId = driver->ID;
            pthread_cond_signal(&req->assignedCond);
            pthread_mutex_unlock(&req->waitMutex);

            pthread_mutex_unlock(&driverMutex);

            printf("DISPATCHER --- Assigned Request #%d to Driver #%d (%s)\n", 
                req->id, driver->ID, getRequestTypeString(req->type));
            {
                char details[64];
                snprintf(details, sizeof(details), "driver_id=%d", driver->ID);
                loggerLogEvent("REQUEST_ASSIGNED", req->id, details);
            }
            updateSharedState();

            // spawn the ride thread to start the ride
            pthread_t rideTid;
            if (pthread_create(&rideTid, NULL, rideThread, (void*)req) != 0) {
                perror("Failed to start the ride");
            }
            pthread_detach(rideTid);
        } else {
            // re-insert and update the queue's deferCount
            pthread_mutex_unlock(&driverMutex);
            insertRequest(&requestQueue, req);
            updateDeferCount(&requestQueue);
            updateSharedState();
            
            usleep(500000); 
        }
    }

    printf("DISPATCHER --- Thread exiting.\n");
    pthread_exit(0);
}