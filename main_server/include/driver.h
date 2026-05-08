#ifndef DRIVER_H
#define DRIVER_H

#include <time.h>
#include <stdint.h>

typedef enum {
    DRIVER_OFFLINE,
    DRIVER_ONLINE,
    DRIVER_BUSY,
} DriverStatus;

typedef enum {
    DRIVER_STANDARD,        // NORMAL requests only
    DRIVER_PLUS,            // NORMAL and VIP
    DRIVER_ELITE            // all three, preferred for EMERGENCY
} DriverCategory;

typedef struct Driver {
    int ID;
    DriverCategory category;
    DriverStatus status;
    int currentRequestID;
    time_t lastStatusChange;
    time_t lastAssignedTime;
} Driver;

extern pthread_mutex_t driverMutex;

#endif
#endif
