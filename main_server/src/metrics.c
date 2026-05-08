#include "../include/metrics.h"

// TODO: Implement metrics accumulation (total requests, completed, cancelled).
// TODO: Implement avg wait time calculation bucketed by original_type.
// TODO: Implement metrics_report function for human-readable summary.
#include <stdio.h>
#include <string.h>

static double getWaitSeconds(const RideRequest *req) {
	if (req == NULL || req->requestTime == 0) {
		return 0.0;
	}
	return difftime(time(NULL), req->requestTime);
}

static void addWaitBucket(Metrics *m, RequestType type, double waitSeconds) {
	switch (type) {
		case NORMAL:
			m->totalWaitNormal += waitSeconds;
			m->countNormal += 1;
			break;
		case VIP:
			m->totalWaitVip += waitSeconds;
			m->countVip += 1;
			break;
		case EMERGENCY:
			m->totalWaitEmergency += waitSeconds;
			m->countEmergency += 1;
			break;
		default:
			break;
	}
}

void metricsInit(Metrics *m) {
	if (m == NULL) {
		return;
	}
	memset(m, 0, sizeof(*m));
	pthread_mutex_init(&m->lock, NULL);
	m->startTime = time(NULL);
}

void metricsRecordCreated(Metrics *m, const RideRequest *req) {
	if (m == NULL || req == NULL) {
		return;
	}
	pthread_mutex_lock(&m->lock);
	m->totalCreated += 1;
	pthread_mutex_unlock(&m->lock);
}

void metricsRecordCompleted(Metrics *m, const RideRequest *req) {
	if (m == NULL || req == NULL) {
		return;
	}
	pthread_mutex_lock(&m->lock);
	m->totalCompleted += 1;
	addWaitBucket(m, req->originalType, getWaitSeconds(req));
	if (req->rideDuration > 0) {
		m->totalBusySeconds += (double)req->rideDuration;
	}
	pthread_mutex_unlock(&m->lock);
}

void metricsRecordCancelled(Metrics *m, const RideRequest *req) {
	if (m == NULL || req == NULL) {
		return;
	}
	pthread_mutex_lock(&m->lock);
	m->totalCancelled += 1;
	addWaitBucket(m, req->originalType, getWaitSeconds(req));
	pthread_mutex_unlock(&m->lock);
}

void metricsSnapshot(const Metrics *m, MetricsSnapshot *out, int numDrivers, int surgeActive, float surgeMultiplier) {
	if (m == NULL || out == NULL) {
		return;
	}

	pthread_mutex_lock((pthread_mutex_t *)&m->lock);
	memset(out, 0, sizeof(*out));
	out->totalCreated = m->totalCreated;
	out->totalCompleted = m->totalCompleted;
	out->totalCancelled = m->totalCancelled;
	out->cancellationRate = (m->totalCreated > 0) ?
		(float)m->totalCancelled / (float)m->totalCreated : 0.0f;

	out->avgWaitNormal = (m->countNormal > 0) ?
		(float)(m->totalWaitNormal / (double)m->countNormal) : 0.0f;
	out->avgWaitVip = (m->countVip > 0) ?
		(float)(m->totalWaitVip / (double)m->countVip) : 0.0f;
	out->avgWaitEmergency = (m->countEmergency > 0) ?
		(float)(m->totalWaitEmergency / (double)m->countEmergency) : 0.0f;

	double elapsed = difftime(time(NULL), m->startTime);
	if (elapsed > 0.0 && numDrivers > 0) {
		out->driverUtilization = (float)(m->totalBusySeconds / (elapsed * (double)numDrivers));
		if (out->driverUtilization < 0.0f) {
			out->driverUtilization = 0.0f;
		} else if (out->driverUtilization > 1.0f) {
			out->driverUtilization = 1.0f;
		}
	}

	out->surgeActive = surgeActive;
	out->surgeMultiplier = surgeMultiplier;
	pthread_mutex_unlock((pthread_mutex_t *)&m->lock);
}

void metricsReport(const Metrics *m, int numDrivers) {
	if (m == NULL) {
		return;
	}

	MetricsSnapshot snap;
	metricsSnapshot(m, &snap, numDrivers, 0, 1.0f);

	printf("\n=== Metrics Report ===\n");
	printf("Total created:   %d\n", snap.totalCreated);
	printf("Completed:       %d\n", snap.totalCompleted);
	printf("Cancelled:       %d\n", snap.totalCancelled);
	printf("Cancel rate:     %.2f%%\n", snap.cancellationRate * 100.0f);
	printf("Avg wait NORMAL: %.2fs\n", snap.avgWaitNormal);
	printf("Avg wait VIP:    %.2fs\n", snap.avgWaitVip);
	printf("Avg wait EMERG:  %.2fs\n", snap.avgWaitEmergency);
	printf("Utilization:     %.2f%%\n", snap.driverUtilization * 100.0f);
}
