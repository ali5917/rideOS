#ifndef DRIVER_H
#define DRIVER_H

#include <pthread.h>
#include <time.h>
#include <stdint.h>

#include "../../shared/include/ipc_types.h"
#include "request.h"

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

int driverCanHandle(DriverCategory category, RequestType type);
int driverFindAvailable(RequestType type);
void* driverLifecycleThread(void *arg);

#endif
