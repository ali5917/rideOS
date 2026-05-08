#include "../include/dispatch.h"
#include "../include/ipc.h"
#include "../include/logger.h"
#include "../include/config.h"

int main(int argc, char *argv[]) {
    // TODO: Parse config and CLI args.
    // TODO: Initialize IPC (Named Pipe, Shared Memory, Semaphores).
    // TODO: Initialize Driver Pool, Priority Queue, and Logger.
    // TODO: Start dispatcher_thread.
    // TODO: Main loop: read from named pipe, spawn request_threads.
    // TODO: Clean shutdown: wait for active rides, report metrics, destroy IPC.
    return 0;
}
