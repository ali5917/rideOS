#ifndef IPC_H
#define IPC_H

#include "../../shared/include/ipc_shared.h"

int ipc_init_request_pipe(void);
int ipc_write_pipe_request(const PipeRequest *req);
void ipc_pipe_cleanup(void);

int ipc_init_shared_memory(SharedState **state);
int ipc_init_shm_lock(void);
int ipc_read_shared_state(SharedState *out);
void ipc_shm_cleanup(void);
void ipc_shm_lock_cleanup(void);

#endif
