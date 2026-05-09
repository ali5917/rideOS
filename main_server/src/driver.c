#include "../include/driver.h"

#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

extern Driver driverPool[MAX_DRIVERS];
extern int numDrivers;
extern pthread_mutex_t driverMutex;
extern volatile sig_atomic_t systemRunning;

int driverCanHandle(DriverCategory category, RequestType type) {
    // everyone handles EMERGENCY
    if (type == EMERGENCY) return 1;
    // VIP requires PLUS
    if (type == VIP) return (category == DRIVER_PLUS);
    // NORMAL can be handled by anyone
    if (type == NORMAL) return (category == DRIVER_STANDARD);
    return 0;
}

int driverFindAvailable(RequestType type) {
    // for EMERGENCY, use any driver (prefer PLUS over STANDARD)
    if (type == EMERGENCY) {
        for (int i = 0; i < numDrivers; i++) {
            if (driverPool[i].status == DRIVER_ONLINE && driverPool[i].category == DRIVER_PLUS) {
                return i;
            }
        }
        for (int i = 0; i < numDrivers; i++) {
            if (driverPool[i].status == DRIVER_ONLINE && driverPool[i].category == DRIVER_STANDARD) {
                return i;
            }
        }
        return -1;
    }

    // for VIP, use PLUS drivers
    if (type == VIP) {
        for (int i = 0; i < numDrivers; i++) {
            if (driverPool[i].status == DRIVER_ONLINE && driverPool[i].category == DRIVER_PLUS) {
                return i;
            }
        }
        return -1;
    }

    // for NORMAL, use STANDARD drivers
    for (int i = 0; i < numDrivers; i++) {
        if (driverPool[i].status == DRIVER_ONLINE && driverPool[i].category == DRIVER_STANDARD) {
            return i;
        }
    }

    return -1;
}

void* driverLifecycleThread(void *arg) {
    (void)arg;

    srand((unsigned int)time(NULL));
    while (systemRunning) {
        sleep((rand() % 10) + 5);

        pthread_mutex_lock(&driverMutex);
        if (numDrivers > 0) {
            int i = rand() % numDrivers;
            if (driverPool[i].status == DRIVER_ONLINE) {
                driverPool[i].status = DRIVER_OFFLINE;
                driverPool[i].lastStatusChange = time(NULL);
            } else if (driverPool[i].status == DRIVER_OFFLINE) {
                driverPool[i].status = DRIVER_ONLINE;
                driverPool[i].lastStatusChange = time(NULL);
            }
        }
        pthread_mutex_unlock(&driverMutex);
    }

    return NULL;
}
