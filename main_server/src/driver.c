#include "../include/driver.h"

#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

extern Driver driverPool[MAX_DRIVERS];
extern int numDrivers;
extern pthread_mutex_t driverMutex;
extern volatile sig_atomic_t systemRunning;

int driverCanHandle(DriverCategory category, RequestType type) {
	if (type == NORMAL) return (category == DRIVER_STANDARD);
	if (type == VIP) return (category == DRIVER_PLUS);
	if (type == EMERGENCY) return (category == DRIVER_STANDARD || category == DRIVER_PLUS);
	return 0;
}

int driverFindAvailable(RequestType type) {
	int match = -1;

	if (type == EMERGENCY) {
		for (int i = 0; i < numDrivers; i++) {
			if (driverPool[i].status == DRIVER_ONLINE && driverPool[i].category == DRIVER_PLUS) {
				return i;
			}
		}
		for (int i = 0; i < numDrivers; i++) {
			if (driverPool[i].status == DRIVER_ONLINE && driverPool[i].category == DRIVER_STANDARD) {
				return i;
			}
		}
		return -1;
	}

	if (type == VIP) {
		for (int i = 0; i < numDrivers; i++) {
			if (driverPool[i].status == DRIVER_ONLINE && driverPool[i].category == DRIVER_PLUS) {
				return i;
			}
		}
		return -1;
	}

	for (int i = 0; i < numDrivers; i++) {
		if (driverPool[i].status == DRIVER_ONLINE && driverPool[i].category == DRIVER_STANDARD) {
			return i;
		}
	}
	return match;
}

void* driverLifecycleThread(void *arg) {
	(void)arg;

	srand((unsigned int)time(NULL));
	while (systemRunning) {
		sleep((rand() % 10) + 5);

		pthread_mutex_lock(&driverMutex);
		int i = rand() % numDrivers;
		if (driverPool[i].status == DRIVER_ONLINE) {
			driverPool[i].status = DRIVER_OFFLINE;
			driverPool[i].lastStatusChange = time(NULL);
		} else if (driverPool[i].status == DRIVER_OFFLINE) {
			driverPool[i].status = DRIVER_ONLINE;
			driverPool[i].lastStatusChange = time(NULL);
		}
		pthread_mutex_unlock(&driverMutex);
	}

	return NULL;
}
