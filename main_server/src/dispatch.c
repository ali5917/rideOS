#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "../include/dispatch.h"
#include "../include/driver.h"
#include "../include/queue.h"
#include "../include/request.h"
#include "../include/ipc.h"

// External globals from main.c
extern PriorityQueue requestQueue;
extern Driver driverPool[MAX_DRIVERS];
extern int numDrivers;
extern pthread_mutex_t driverMutex;
extern int systemRunning;

// check if a driver category can handle a request type
static int canHandle(DriverCategory category, RequestType type) {
    if (type == NORMAL) return 1; 
    if (type == VIP && (category == DRIVER_PLUS || category == DRIVER_ELITE)) return 1;
    if (type == EMERGENCY && category == DRIVER_ELITE) return 1;
    return 0;
}

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
        RideRequest* req = getRequest(&requestQueue);

        // skip cancelled request
        if (req->status == REQUEST_CANCELLED) {
            printf("DISPATCHER --- Skipping cancelled Request #%d\n", req->id);
            // Memory cleanup for req should be handled carefully (e.g., in main)
            continue;
        }

        // find a suitable driver
        int foundDriverIndex = -1;
        pthread_mutex_lock(&driverMutex);
        
        for (int i = 0; i < numDrivers; i++) {
            if (driverPool[i].status == DRIVER_ONLINE && canHandle(driverPool[i].category, req->type)) {
                foundDriverIndex = i;
                break;
            }
        }

        if (foundDriverIndex != -1) {
            // assign the driver
            Driver* driver = &driverPool[foundDriverIndex];
            driver->status = DRIVER_BUSY;
            driver->currentRequestID = req->id;
            driver->lastAssignedTime = time(NULL);

            pthread_mutex_unlock(&driverMutex);

            // update request status
            pthread_mutex_lock(&req->waitMutex);
            req->status = REQUEST_ASSIGNED;
            req->assignedDriverId = driver->ID;
            pthread_cond_signal(&req->assignedCond);
            pthread_mutex_unlock(&req->waitMutex);

            printf("DISPATCHER --- Assigned Request #%d to Driver #%d (%s)\n", req->id, driver->ID, getRequestTypeString(req->type));

            // spawn the ride thread to start the ride
            pthread_t rideTid;
            if (pthread_create(&rideTid, NULL, rideThread, (void*)req) != 0) {
                perror("Failed to start the ride");
            }
            pthread_detach(rideTid);
            
            // TODO: Update Shared Memory State for the GUI
        } else {
            // re-insert and update the queue's deferCount
            pthread_mutex_unlock(&driverMutex);
            insertRequest(&requestQueue, req);
            updateDeferCount(&requestQueue);
            
            usleep(500000); 
        }
    }

    printf("DISPATCHER --- Thread exiting.\n");
    pthread_exit(0);
}