#include "../include/ipc.h"
#include "../include/request.h"
#include "../include/config.h"

#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

// mkfifo pipes bring requests from request_server to main_server
// shm sends stats, & state from main_server to request server

// unitialized statics
static int pipe_fd = -1;
static int pipe_dummy_fd = -1;
static SharedState *shm_ptr = NULL;
static sem_t *shm_sem = NULL;

int initRequestPipe(void) {
	// REQUEST_PIPE = tmp/rideos_pipe
	if (mkfifo(REQUEST_PIPE, 0666) == -1 && errno != EEXIST) {
		perror("initRequestPipe: mkfifo");
		return -1;
	}

	pipe_fd = open(REQUEST_PIPE, O_RDONLY | O_NONBLOCK);
	if (pipe_fd == -1) {
		perror("initRequestPipe: open read");
		return -1;
	}

	pipe_dummy_fd = open(REQUEST_PIPE, O_WRONLY | O_NONBLOCK); // just to avoid the wait in main server until request server is active
	if (pipe_dummy_fd == -1) {
		perror("initRequestPipe: open dummy write");
	}
	return pipe_fd;
}

int initSharedMemory(SharedState **state) {
	int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
	if (shm_fd == -1) {
		perror("initSharedMemory: shm_open");
		return -1;
	}

	if (ftruncate(shm_fd, sizeof(SharedState)) == -1) {
		perror("initSharedMemory: ftruncate");
		close(shm_fd);
		return -1;
	}

	void *ptr = mmap(NULL, sizeof(SharedState), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	close(shm_fd);
	if (ptr == MAP_FAILED) {
		perror("initSharedMemory: mmap");
		return -1;
	}

	shm_ptr = (SharedState *)ptr;
	memset(shm_ptr, 0, sizeof(SharedState));
	*state = shm_ptr;
	return 0;
}

int initShmLock(void) {
	// named semaphore
	shm_sem = sem_open(SEM_SHM_LOCK, O_CREAT, 0666, 1);
	if (shm_sem == SEM_FAILED) {
		perror("initShmLock: sem_open");
		shm_sem = NULL;
		return -1;
	}
	return 0;
}

int readPipeRequest(RideRequest *out) {
	if (pipe_fd == -1 || out == NULL) {
		return -1;
	}

	PipeRequest pr;
	ssize_t n = read(pipe_fd, &pr, sizeof(PipeRequest));
	if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
		return 1;
	}
	if (n != sizeof(PipeRequest)) {
		perror("readPipeRequest: read");
		return -1;
	}

	memset(out, 0, sizeof(RideRequest));
	out->id = pr.id;
	out->type = pr.type;
	out->originalType = pr.type;
	out->status = REQUEST_WAITING;
	out->assignedDriverId = -1;
	out->fare = (float)pr.baseFare;
	out->rideDuration = pr.rideDuration;
	out->requestTime = pr.requestTime;
	out->timeoutSeconds = configGetTimeout(&config, pr.type);
	out->deferredCount = 0;

	return 0;
}

void writeSharedState(const SharedState *frame) {
	if (shm_ptr == NULL || shm_sem == NULL || frame == NULL) {
		return;
	}

	sem_wait(shm_sem);
	int nextTick = shm_ptr->tick + 1;
	memcpy(shm_ptr, frame, sizeof(SharedState));
	shm_ptr->tick = nextTick;
	sem_post(shm_sem);
}

void pipeCleanup(void) {
	if (pipe_fd != -1) {
		close(pipe_fd);
		pipe_fd = -1;
	}
	if (pipe_dummy_fd != -1) {
		close(pipe_dummy_fd);
		pipe_dummy_fd = -1;
	}
	unlink(REQUEST_PIPE);
}

void shmCleanup(void) {
	if (shm_ptr != NULL) {
		munmap(shm_ptr, sizeof(SharedState));
		shm_ptr = NULL;
	}
	shm_unlink(SHM_NAME);
}

void shmLockCleanup(void) {
	if (shm_sem != NULL) {
		sem_close(shm_sem);
		shm_sem = NULL;
	}
	sem_unlink(SEM_SHM_LOCK);
}
