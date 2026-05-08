#ifndef LOGGER_H
#define LOGGER_H

#include <pthread.h>
#include <stdio.h>

typedef struct {
	FILE *file;
	pthread_mutex_t lock;
} Logger;

int loggerInit(const char *path);
void loggerClose(void);
void loggerLogEvent(const char *event, int requestId, const char *details);

#endif
