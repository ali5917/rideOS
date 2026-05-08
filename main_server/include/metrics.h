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

void metrics_init(Metrics *m);
void metrics_record_created(Metrics *m, const RideRequest *req);
void metrics_record_completed(Metrics *m, const RideRequest *req);
void metrics_record_cancelled(Metrics *m, const RideRequest *req);
void metrics_snapshot(const Metrics *m, MetricsSnapshot *out, int numDrivers, int surgeActive, float surgeMultiplier);
void metrics_report(const Metrics *m, int numDrivers);

#endif
