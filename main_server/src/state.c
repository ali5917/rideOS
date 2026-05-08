#include <string.h>
#include <pthread.h>
#include <signal.h>

#include "../include/state.h"
#include "../include/driver.h"
#include "../include/ipc.h"
#include "../include/metrics.h"
#include "../include/queue.h"
#include "../include/request.h"
#include "../include/config.h"

extern PriorityQueue requestQueue;
extern Driver driverPool[MAX_DRIVERS];
extern int numDrivers;
extern pthread_mutex_t driverMutex;
extern Metrics metrics;
extern SharedState *sharedState;
extern volatile sig_atomic_t systemRunning;
extern Config config;

void updateSharedState(void) {
    if (sharedState == NULL) {
        return;
    }

    SharedState frame;
    memset(&frame, 0, sizeof(frame));

    frame.shutdownFlag = systemRunning ? 0 : 1;
    frame.config.numDrivers = numDrivers;
    frame.config.durationSec = config.durationSec;
    frame.config.surgeThreshold = config.surgeThreshold;
    frame.config.timeoutNormal = config.timeoutNormal;
    frame.config.timeoutVip = config.timeoutVip;
    frame.config.timeoutEmergency = config.timeoutEmergency;
    frame.config.agingNormalToVip = config.agingNormalToVip;
    frame.config.agingVipToEmergency = config.agingVipToEmergency;

    pthread_mutex_lock(&driverMutex);
    frame.numDrivers = numDrivers;
    for (int i = 0; i < numDrivers && i < MAX_DRIVERS; i++) {
        frame.drivers[i].id = driverPool[i].ID;
        frame.drivers[i].category = driverPool[i].category;
        frame.drivers[i].status = driverPool[i].status;
        frame.drivers[i].currentRequestId = driverPool[i].currentRequestID;
        frame.drivers[i].ridesCompleted = driverPool[i].ridesCompleted;
        if (driverPool[i].status == DRIVER_BUSY) {
            frame.activeRides += 1;
        }
    }
    pthread_mutex_unlock(&driverMutex);

    pthread_mutex_lock(&requestQueue.lock);
    int viewCount = (requestQueue.size < MAX_PENDING_VIEW) ? requestQueue.size : MAX_PENDING_VIEW;
    frame.pendingCount = viewCount;
    for (int i = 0; i < viewCount; i++) {
        RideRequest *req = requestQueue.heap[i];
        if (req == NULL) {
            continue;
        }
        pthread_mutex_lock(&req->waitMutex);
        frame.pendingRequests[i].id = req->id;
        frame.pendingRequests[i].type = req->type;
        frame.pendingRequests[i].originalType = req->originalType;
        frame.pendingRequests[i].status = req->status;
        frame.pendingRequests[i].assignedDriverId = req->assignedDriverId;
        frame.pendingRequests[i].waitTicks = req->deferredCount;
        frame.pendingRequests[i].requestTime = req->requestTime;
        frame.pendingRequests[i].timeoutSeconds = req->timeoutSeconds;
        frame.pendingRequests[i].fare = (int)req->fare;
        pthread_mutex_unlock(&req->waitMutex);

        if (req->type == NORMAL) {
            frame.pendingNormal += 1;
        } else if (req->type == VIP) {
            frame.pendingVip += 1;
        } else if (req->type == EMERGENCY) {
            frame.pendingEmergency += 1;
        }
    }
    pthread_mutex_unlock(&requestQueue.lock);

    int surgeActive = (config.surgeThreshold > 0 && frame.pendingCount > config.surgeThreshold) ? 1 : 0;
    metricsSnapshot(&metrics, &frame.metrics, numDrivers, surgeActive, config.surgeMultiplier);

    writeSharedState(&frame);
}