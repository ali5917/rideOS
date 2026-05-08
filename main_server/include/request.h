#ifndef REQUEST_H
#define REQUEST_H

#include <pthread.h>
#include <time.h>

#include "../../shared/include/ipc_types.h"

typedef struct {
    int id;
    RequestType type;           // current type 
    RequestType originalType;   // assigned type at the time of creation
    RequestStatus status;
    int assignedDriverId;
    float fare;
    int rideDuration;
    time_t requestTime;         // time at request creation
    int timeoutSeconds;         // patience time of request (before it cancels itself)            
    int deferredCount;          // to prevent starvation

    pthread_mutex_t waitMutex;    // mutex for the request thread to sleep on
    pthread_cond_t assignedCond;  // signaled by dispatcher when a driver is assigned
} RideRequest;

void* requestThread(void* arg);
void* rideThread(void* arg);
void destroyRequest(RideRequest* req);

#endif