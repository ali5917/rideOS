#ifndef METRICS_H
#define METRICS_H

#include <pthread.h>
#include <time.h>

#include "../../shared/include/metrics.h"
#include "request.h"

typedef struct {
	int totalCreated;
	int totalCompleted;
	int totalCancelled;

	double totalWaitNormal;
	double totalWaitVip;
	double totalWaitEmergency;

	int countNormal;
	int countVip;
	int countEmergency;

	double totalBusySeconds;
	time_t startTime;

	pthread_mutex_t lock;
} Metrics;

void metricsInit(Metrics *m);
void metricsRecordCreated(Metrics *m, const RideRequest *req);
void metricsRecordCompleted(Metrics *m, const RideRequest *req);
void metricsRecordCancelled(Metrics *m, const RideRequest *req);
void metricsSnapshot(const Metrics *m, MetricsSnapshot *out, int numDrivers, int surgeActive, float surgeMultiplier);
void metricsReport(const Metrics *m, int numDrivers);

#endif
