#ifndef IPC_H
#define IPC_H

#include "../../shared/include/ipc_shared.h"
#include "request.h"

int initRequestPipe(void);
int initSharedMemory(SharedState **state);
int initShmLock(void);
int readPipeRequest(RideRequest *out, int timeoutSeconds);
void writeSharedState(const SharedState *frame);
void pipeCleanup(void);
void shmCleanup(void);
void shmLockCleanup(void);

#endif