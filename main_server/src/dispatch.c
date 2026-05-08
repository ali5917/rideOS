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

// Helper: Check if a driver category can handle a request type
static int canHandle(DriverCategory category, RequestType type) {
    if (type == NORMAL) return 1; // Everyone can take normal
    if (type == VIP && (category == DRIVER_PLUS || category == DRIVER_ELITE)) return 1;
    if (type == EMERGENCY && category == DRIVER_ELITE) return 1;
    return 0;
}

void* dispatcherThread(void* arg) {
    printf("[DISPATCHER] Thread started.\n");

    while (systemRunning) {
        // 1. Get the highest priority request (Blocks if empty)
        RideRequest* req = getRequest(&requestQueue);

        // 2. Skip if already cancelled
        if (req->status == REQUEST_CANCELLED) {
            printf("[DISPATCHER] Skipping cancelled Request #%d\n", req->id);
            // Memory cleanup for req should be handled carefully (e.g., in main)
            continue;
        }

        // 3. Try to find a matching driver
        int foundDriverIndex = -1;
        pthread_mutex_lock(&driverMutex);
        
        for (int i = 0; i < numDrivers; i++) {
            if (driverPool[i].status == DRIVER_ONLINE && canHandle(driverPool[i].category, req->type)) {
                foundDriverIndex = i;
                break;
            }
        }

        if (foundDriverIndex != -1) {
            // 4. Assign the driver
            Driver* driver = &driverPool[foundDriverIndex];
            driver->status = DRIVER_BUSY;
            driver->currentRequestID = req->id;
            driver->lastAssignedTime = time(NULL);

            pthread_mutex_unlock(&driverMutex);

            // 5. Update request status
            pthread_mutex_lock(&req->waitMutex);
            req->status = REQUEST_ASSIGNED;
            req->assignedDriverId = driver->ID;
            pthread_cond_signal(&req->assignedCond);
            pthread_mutex_unlock(&req->waitMutex);

            printf("[DISPATCHER] Assigned Request #%d to Driver #%d (%s)\n", 
                   req->id, driver->ID, (req->type == EMERGENCY ? "EMERGENCY" : (req->type == VIP ? "VIP" : "NORMAL")));

            // 6. Spawn the Ride Thread to handle the actual journey
            pthread_t rideTid;
            if (pthread_create(&rideTid, NULL, rideThread, (void*)req) != 0) {
                perror("Failed to create ride thread");
            }
            pthread_detach(rideTid); // We don't need to join it
            
            // TODO: Update Shared Memory State for the GUI
        } else {
            // 7. No driver available: Re-insert and Age the queue
            pthread_mutex_unlock(&driverMutex);
            
            insertRequest(&requestQueue, req);
            updateDeferCount(&requestQueue);
            
            // Wait a bit before trying again to prevent CPU hammering
            usleep(500000); 
        }
    }

    printf("[DISPATCHER] Thread exiting.\n");
    return NULL;
}
