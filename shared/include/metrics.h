#ifndef METRICS_SNAPSHOT_H
#define METRICS_SNAPSHOT_H
 
typedef struct {
    int totalCreated;
    int totalCompleted;
    int totalCancelled;
    float cancellationRate;

    int numRequestsNormal;
    int numRequestsVip;
    int numRequestsEmergency;
 
    float avgWaitNormal;
    float avgWaitVip;
    float avgWaitEmergency;
 
    float driverUtilization;
    int surgeActive;
    float surgeMultiplier;
} MetricsSnapshot;
 
#endif
 