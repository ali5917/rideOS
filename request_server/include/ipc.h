#ifndef IPC_H
#define IPC_H

#include "../../shared/include/ipc_shared.h"

int initRequestPipe(void);
int writePipeRequest(const PipeRequest *req);
void pipeCleanup(void);

int initSharedMemory(SharedState **state);
int initShmLock(void);
int readSharedState(SharedState *out);
void shmCleanup(void);
void shmLockCleanup(void);

#endif