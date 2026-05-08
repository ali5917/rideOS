#ifndef IPC_TYPES_H
#define IPC_TYPES_H

#define MAX_DRIVERS 128

typedef enum {
    NORMAL,
    VIP,
    EMERGENCY
} RequestType;

typedef enum {
    REQUEST_WAITING,
    REQUEST_ASSIGNED,
    REQUEST_COMPLETED,
    REQUEST_CANCELLED
} RequestStatus;

typedef enum {
    DRIVER_OFFLINE,
    DRIVER_ONLINE,
    DRIVER_BUSY
} DriverStatus;

typedef enum {
    DRIVER_STANDARD,
    DRIVER_PLUS,
    DRIVER_ELITE
} DriverCategory;

#endif
