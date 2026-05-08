#ifndef DRIVER_H
#define DRIVER_H

#include <time.h>
#include <stdint.h>

typedef enum {
    DRIVER_OFFLINE,
    DRIVER_ONLINE,
    DRIVER_BUSY,
    DRIVER_GOING_OFFLINE
} DriverStatus;

typedef enum {
    DRIVER_STANDARD, // std cant take vip/emergency ride requests
    DRIVER_PLUS, // can take vip
    DRIVER_EMERGENCY // preferred for emergency, can take all
} DriverCategory;

typedef struct Driver {
    int ID;
    DriverCategory category;
    DriverStatus status;
    int currentRequestID;
    time_t lastStatusChange;
    time_t lastAssignedTime;
} Driver;

#endif
