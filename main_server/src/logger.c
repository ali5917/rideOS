#include "../include/logger.h"

#include <string.h>
#include <time.h>

static Logger logger;

int loggerInit(const char *path) {
	if (path == NULL) {
		return -1;
	}

	logger.file = fopen(path, "a");
	if (logger.file == NULL) {
		return -1;
	}

	pthread_mutex_init(&logger.lock, NULL);
	return 0;
}

void loggerClose(void) {
	if (logger.file == NULL) {
		return;
	}

	pthread_mutex_lock(&logger.lock);
	fclose(logger.file);
	logger.file = NULL;
	pthread_mutex_unlock(&logger.lock);
	pthread_mutex_destroy(&logger.lock);
}

void loggerLogEvent(const char *event, int requestId, const char *details) {
	if (logger.file == NULL || event == NULL) {
		return;
	}

	time_t now = time(NULL);
	struct tm timeNow;
	localtime_r(&now, &timeNow);

	char timeBuf[32];
	strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &timeNow);

	char line[256];
	if (details != NULL && details[0] != '\0') {
		snprintf(line, sizeof(line), "%s | %s | request_id=%d | %s", timeBuf, event, requestId, details);
	} else {
		snprintf(line, sizeof(line), "%s | %s | request_id=%d", timeBuf, event, requestId);
	}

	pthread_mutex_lock(&logger.lock);
	fprintf(logger.file, "%s\n", line);
	fflush(logger.file);
	pthread_mutex_unlock(&logger.lock);
}
