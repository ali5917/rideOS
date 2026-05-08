#ifndef DRIVER_H
#define DRIVER_H

#include <pthread.h>
#include <time.h>
#include <stdint.h>

#include "../../shared/include/ipc_types.h"

typedef struct Driver {
    int ID;
    DriverCategory category;
    DriverStatus status;
    int currentRequestID;
    int ridesCompleted;
    time_t lastStatusChange;
    time_t lastAssignedTime;
} Driver;


extern Driver driverPool[MAX_DRIVERS];
extern int numDrivers;
extern pthread_mutex_t driverMutex;

#endif
