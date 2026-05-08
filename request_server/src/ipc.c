#include "../include/ipc.h"

#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static int pipe_fd = -1;
static SharedState *shm_ptr = NULL;
static sem_t *shm_sem = NULL;

int initRequestPipe(void) {
	pipe_fd = open(REQUEST_PIPE, O_WRONLY);
	if (pipe_fd == -1) {
		perror("initRequestPipe: open write");
		return -1;
	}
	return 0;
}

int writePipeRequest(const PipeRequest *req) {
	if (pipe_fd == -1 || req == NULL) {
		return -1;
	}

	ssize_t n = write(pipe_fd, req, sizeof(PipeRequest));
	if (n != sizeof(PipeRequest)) {
		perror("writePipeRequest: write");
		return -1;
	}
	return 0;
}

void pipeCleanup(void) {
	if (pipe_fd != -1) {
		close(pipe_fd);
		pipe_fd = -1;
	}
}

int initSharedMemory(SharedState **state) {
	int shm_fd = -1;
	int retries = 10;
	for (int attempt = 1; attempt <= retries; attempt++) {
		shm_fd = shm_open(SHM_NAME, O_RDONLY, 0666);
		if (shm_fd != -1) {
			break;
		}
		fprintf(stderr, "initSharedMemory: waiting for main_server... (%d/%d)\n", attempt, retries);
		sleep(1);
	}
	if (shm_fd == -1) {
		fprintf(stderr, "initSharedMemory: main_server never came up\n");
		return -1;
	}

	void *ptr = mmap(NULL, sizeof(SharedState), PROT_READ, MAP_SHARED, shm_fd, 0);
	close(shm_fd);
	if (ptr == MAP_FAILED) {
		perror("initSharedMemory: mmap");
		return -1;
	}

	shm_ptr = (SharedState *)ptr;
	if (state != NULL) {
		*state = shm_ptr;
	}
	return 0;
}

int initShmLock(void) {
	int retries = 10;
	for (int attempt = 1; attempt <= retries; attempt++) {
		shm_sem = sem_open(SEM_SHM_LOCK, 0);
		if (shm_sem != SEM_FAILED) {
			return 0;
		}
		fprintf(stderr, "initShmLock: waiting for main_server... (%d/%d)\n", attempt, retries);
		sleep(1);
	}
	fprintf(stderr, "initShmLock: main_server never came up\n");
	return -1;
}

int readSharedState(SharedState *out) {
	if (shm_ptr == NULL || shm_sem == NULL || out == NULL) {
		return -1;
	}

	sem_wait(shm_sem);
	memcpy(out, shm_ptr, sizeof(SharedState));
	sem_post(shm_sem);
	return 0;
}

void shmCleanup(void) {
	if (shm_ptr != NULL) {
		munmap(shm_ptr, sizeof(SharedState));
		shm_ptr = NULL;
	}
}

void shmLockCleanup(void) {
	if (shm_sem != NULL) {
		sem_close(shm_sem);
		shm_sem = NULL;
	}
}
