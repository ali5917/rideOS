#ifndef IPC_SHARED_H
#define IPC_SHARED_H

#include <time.h>

#include "ipc_types.h"

#define MAX_PENDING_VIEW 50
#define MAX_DRIVERS 128

// for req sever to main server
typedef struct {
    int id;
    RequestType type;
    int rideDuration;
    time_t requestTime;
    int baseFare;
} PipeRequest;

typedef struct {
    int id;
    RequestType type;
    RequestType originalType;
    RequestStatus status;
    int assignedDriverId;
    int waitTicks;
    time_t requestTime;
    time_t assignedTime;
    time_t completedTime;
    int timeoutSeconds;
    int fare;
} RequestView;

typedef struct {
    int id;
    DriverCategory category;
    DriverStatus status;
    int currentRequestId;
    int ridesCompleted;
} DriverView;

typedef struct {
    int numDrivers;
    int durationSec; // simulation period
    int surgeThreshold; // threshold for fare surge
    int timeoutNormal; // time until starvation
    int timeoutVip; // time until starvation
    int timeoutEmergency; // time until starvation
    int agingNormalToVip; // normal to vip threshold
    int agingVipToEmergency; // vip to emergency threshold
} ConfigSnapshot;

// from main server to request server, for interface
typedef struct {
    int shutdownFlag; // is shutting down

    int tick; // to avoid rerender

    ConfigSnapshot config;

    DriverView drivers[MAX_DRIVERS];
    int numDrivers;

    RequestView pendingRequests[MAX_PENDING_VIEW];
    int pendingCount;
    int pendingNormal;
    int pendingVip;
    int pendingEmergency;

    int activeRides;

    MetricsSnapshot metrics;
} SharedState;

#define REQUEST_PIPE "/tmp/rideos_pipe"
#define SHM_NAME "/rideos_shm"
#define SEM_SHM_LOCK "/rideos_sem"

#endif
