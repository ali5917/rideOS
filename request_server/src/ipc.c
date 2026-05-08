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

int ipc_init_request_pipe(void) {
	pipe_fd = open(REQUEST_PIPE, O_WRONLY);
	if (pipe_fd == -1) {
		perror("ipc_init_request_pipe: open write");
		return -1;
	}
	return 0;
}

int ipc_write_pipe_request(const PipeRequest *req) {
	if (pipe_fd == -1 || req == NULL) {
		return -1;
	}

	ssize_t n = write(pipe_fd, req, sizeof(PipeRequest));
	if (n != sizeof(PipeRequest)) {
		perror("ipc_write_pipe_request: write");
		return -1;
	}
	return 0;
}

void ipc_pipe_cleanup(void) {
	if (pipe_fd != -1) {
		close(pipe_fd);
		pipe_fd = -1;
	}
}

int ipc_init_shared_memory(SharedState **state) {
	int shm_fd = -1;
	int retries = 10;
	for (int attempt = 1; attempt <= retries; attempt++) {
		shm_fd = shm_open(SHM_NAME, O_RDONLY, 0666);
		if (shm_fd != -1) {
			break;
		}
		fprintf(stderr, "ipc_init_shared_memory: waiting for main_server... (%d/%d)\n", attempt, retries);
		sleep(1);
	}
	if (shm_fd == -1) {
		fprintf(stderr, "ipc_init_shared_memory: main_server never came up\n");
		return -1;
	}

	void *ptr = mmap(NULL, sizeof(SharedState), PROT_READ, MAP_SHARED, shm_fd, 0);
	close(shm_fd);
	if (ptr == MAP_FAILED) {
		perror("ipc_init_shared_memory: mmap");
		return -1;
	}

	shm_ptr = (SharedState *)ptr;
	if (state != NULL) {
		*state = shm_ptr;
	}
	return 0;
}

int ipc_init_shm_lock(void) {
	int retries = 10;
	for (int attempt = 1; attempt <= retries; attempt++) {
		shm_sem = sem_open(SEM_SHM_LOCK, 0);
		if (shm_sem != SEM_FAILED) {
			return 0;
		}
		fprintf(stderr, "ipc_init_shm_lock: waiting for main_server... (%d/%d)\n", attempt, retries);
		sleep(1);
	}
	fprintf(stderr, "ipc_init_shm_lock: main_server never came up\n");
	return -1;
}

int ipc_read_shared_state(SharedState *out) {
	if (shm_ptr == NULL || shm_sem == NULL || out == NULL) {
		return -1;
	}

	sem_wait(shm_sem);
	memcpy(out, shm_ptr, sizeof(SharedState));
	sem_post(shm_sem);
	return 0;
}

void ipc_shm_cleanup(void) {
	if (shm_ptr != NULL) {
		munmap(shm_ptr, sizeof(SharedState));
		shm_ptr = NULL;
	}
}

void ipc_shm_lock_cleanup(void) {
	if (shm_sem != NULL) {
		sem_close(shm_sem);
		shm_sem = NULL;
	}
}
