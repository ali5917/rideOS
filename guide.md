# RideOS Viva Comprehensive Study Guide

This guide covers the technical architecture, synchronization primitives, and common viva questions for the **RideOS** project.

---

## 1. Project Architecture Overview

**RideOS** is a multi-threaded, multi-process ride-sharing dispatch simulation.

- **Two Core Processes**:
  1. **Main Server**: The backend engine. Handles request queuing, driver dispatching, ride simulation, and metrics.
  2. **Request Server**: The frontend GUI (built with Raylib). Generates ride requests and displays the system state in real-time.

- **Inter-Process Communication (IPC)**:
  - **Named Pipe (FIFO)**: Sends `PipeRequest` objects from the Request Server to the Main Server.
  - **Shared Memory (POSIX SHM)**: The Main Server writes a `SharedState` frame (containing current queue and driver info) which the Request Server reads for visualization.
  - **Named Semaphore**: Synchronizes access to the Shared Memory between the two processes.

---

## 2. Synchronization Primitives (The "Meat" of OS)

### A. Mutexes (`pthread_mutex_t`)
Mutexes ensure **mutual exclusion**, meaning only one thread can execute a critical section of code at a time. 

#### 1. `requestQueue.lock`
*   **Location**: Initialized in `main_server/src/queue.c`.
*   **Purpose**: Protects the Priority Queue's internal heap (`pq->heap`) and `size` variable from race conditions.
*   **Blocked By**: 
    *   `insertRequest()`: Blocks at the start to safely insert a request into the heap.
    *   `getRequest()`: Blocks at the start to safely remove the highest-priority request. (Note: It temporarily releases the lock while blocked on the `notEmpty` condition variable).
    *   `updateDeferCount()`: Blocks at the start to iterate through all waiting requests, age them (e.g., NORMAL to VIP), and rebuild the heap if priorities changed.
    *   `wakeAllRequests()`: Blocks before broadcasting to ensure no race conditions during shutdown.
*   **Unblocked By**: The same function that blocked it unblocks it immediately after finishing the heap manipulation.

#### 2. `driverMutex`
*   **Location**: Externally declared in `main_server/src/driver.c`.
*   **Purpose**: Protects the global `driverPool` array so that multiple threads don't assign the same driver to different requests simultaneously.
*   **Blocked By**:
    *   `dispatcherThread()` (in `dispatch.c`): Blocks right before searching for an available driver using `driverFindAvailable()`. If it finds one, it marks the driver as `DRIVER_BUSY`.
    *   `rideThread()` (in `request.c`): Blocks at the end of the simulated ride to mark the driver back to `DRIVER_ONLINE` and increment their completed rides.
    *   `driverLifecycleThread()` (in `driver.c`): Blocks periodically to randomly toggle a driver's status between ONLINE and OFFLINE to simulate real-world driver behavior.
*   **Unblocked By**: The thread that locked it unblocks it right after the driver's state has been updated.

#### 3. `req->waitMutex`
*   **Location**: Initialized inside each specific `RideRequest` struct.
*   **Purpose**: Protects the state/status of an individual request, ensuring the dispatcher and the request's own thread don't conflict (e.g., dispatcher assigns a driver at the exact same millisecond the request decides to timeout and cancel itself).
*   **Blocked By**:
    *   `requestThread()` (in `request.c`): Blocks before checking if the status is `REQUEST_WAITING`. It releases the lock while waiting on `assignedCond`, re-acquires it upon waking, and then updates the status to `REQUEST_CANCELLED` if a timeout occurred.
    *   `dispatcherThread()` (in `dispatch.c`): Blocks right after pulling the request from the queue to ensure it hasn't already timed out. It blocks *again* right before assigning a driver to ensure a timeout hasn't occurred while it was searching for a driver.
*   **Unblocked By**: The respective thread after updating the request's status (e.g., changing to `REQUEST_ASSIGNED` or `REQUEST_CANCELLED`).

#### 4. `metrics.lock` & `logger.lock`
*   **Purpose**: Protects the global `metrics` structure and the `Logger` file descriptor, respectively.
*   **Blocked By**: Any thread calling `metricsRecordCreated`, `metricsRecordCompleted`, `metricsRecordCancelled`, `metricsSnapshot`, or `loggerLogEvent`. 
*   **Unblocked By**: The same thread immediately after the counter is incremented or the log string is written (`fprintf` + `fflush`) to the file.

### B. Condition Variables (`pthread_cond_t`)
Condition variables allow threads to sleep (block) until a specific condition becomes true, avoiding CPU-intensive busy-waiting (like an infinite `while` loop).

#### 1. `requestQueue.notEmpty`
*   **Location**: Initialized in `main_server/src/queue.c`.
*   **When/Who gets Blocked**: The `dispatcherThread` (by calling `getRequest()`) calls `pthread_cond_wait()`. It gets put to sleep if it checks the priority queue and finds that its `size == 0`.
*   **When/Who Unblocks it**: 
    *   **Signaled by**: `insertRequest()` calls `pthread_cond_signal()` every time a new request is successfully added to the queue. This wakes up the sleeping dispatcher so it can assign a driver.
    *   **Broadcast by**: `wakeAllRequests()` calls `pthread_cond_broadcast()` during server shutdown to wake up any sleeping dispatchers so the program can exit cleanly.

#### 2. `req->assignedCond`
*   **Location**: Initialized inside each specific `RideRequest` struct.
*   **When/Who gets Blocked**: The `requestThread()` handling that specific user request calls `pthread_cond_timedwait()`. It gets put to sleep, waiting for the dispatcher to find a driver. It will block until either a driver is assigned OR its `timeoutSeconds` limit expires.
*   **When/Who Unblocks it**: The `dispatcherThread` calls `pthread_cond_signal()` immediately after it successfully assigns a driver ID to the request and changes its status to `REQUEST_ASSIGNED`.

### C. Semaphores (`sem_t`)
Semaphores are used for synchronization. In this project, a named semaphore acts as a mutex *across different processes*.

#### 1. `SEM_SHM_LOCK` (Named Binary Semaphore)
*   **Location**: Used in both `main_server/src/ipc.c` and `request_server/src/ipc.c`. It's initialized to `1` (meaning it's available).
*   **When/Who gets Blocked**: 
    *   The **Main Server** calls `sem_wait()` before calling `memcpy` to write the latest `SharedState` to POSIX Shared Memory. It blocks if the Request Server is currently reading the memory.
    *   The **Request Server** calls `sem_wait()` before reading the `SharedState`. It blocks if the Main Server is currently updating it.
*   **When/Who Unblocks it**: Both processes call `sem_post()` immediately after their read/write operations finish, incrementing the semaphore back to 1 and allowing the other process to access the shared memory.

---

## 3. System Workflows & Key Concepts

To fully understand the system, we must trace how data flows between processes and how threads orchestrate the lifecycle of a ride request.

### A. The System Startup & IPC Workflow
1. **Main Server Initialization**: The `main()` function starts. It initializes the Priority Queue, config settings, metrics, and logger. It pre-allocates the `driverPool` (assigning 1/3 as PLUS drivers and 2/3 as STANDARD).
2. **IPC Setup**: `main()` creates the **Named Pipe (FIFO)** (`tmp/rideos_pipe`), allocates the **POSIX Shared Memory** (`SHM_NAME`), and creates the **Named Semaphore** (`SEM_SHM_LOCK`).
3. **Thread Spawning**: `main()` launches the `dispatcherThread` and enters an infinite `while(systemRunning)` loop, constantly reading from the Named Pipe.
4. **Request Server (GUI)**: The separate GUI process launches, opens the write end of the Named Pipe, and maps the Shared Memory to read the system state.

### B. The Request Lifecycle Workflow (Start to Finish)
1. **Generation (Request Server)**: The user clicks the UI or the random generator creates a request. A `PipeRequest` struct is written to the Named Pipe.
2. **Reception (Main Server)**: The Main Server's `while` loop reads the `PipeRequest` from the pipe. It dynamically allocates a `RideRequest`, initializes its specific Mutex/Cond var, and spawns a **detached** `requestThread` for it.
3. **Queuing (`requestThread`)**: The newly spawned `requestThread` logs its creation, calls `insertRequest()` to lock the queue, inserts itself into the Min-Heap, and **signals** `requestQueue.notEmpty`. It then calls `pthread_cond_timedwait` on its own `assignedCond` and goes to sleep.
4. **Dispatching (`dispatcherThread`)**: 
   - Wakes up because `notEmpty` was signaled.
   - Extracts the highest priority request from the heap.
   - Locks `driverMutex` and searches for an available driver matching the request type.
   - **If a driver is found**: It marks the driver as `DRIVER_BUSY`, sets the request status to `REQUEST_ASSIGNED`, and **signals** the sleeping `requestThread`. It then spawns a detached `rideThread`.
   - **If NO driver is found**: It re-inserts the request into the queue and calls `updateDeferCount()` to age the waiting requests.
5. **Request Thread Exit**: Woken by the dispatcher's signal, the `requestThread` wakes up, sees its status is assigned, and safely calls `pthread_exit(0)`.
6. **Ride Execution (`rideThread`)**: Simulates the physical trip by calling `sleep(duration)`. Upon waking, it locks `driverMutex`, sets the driver back to `DRIVER_ONLINE`, updates the global metrics, and calls `destroyRequest()` to free memory.

### C. Starvation and The "Aging" Algorithm
*   **The Problem**: Because this uses a Priority Queue (EMERGENCY=3, VIP=2, NORMAL=1), a constant influx of VIP requests could mean a NORMAL request sits at the bottom of the heap forever. This is known as **Starvation**.
*   **The Solution**: Inside `updateDeferCount()` (called by the dispatcher when no drivers are available), we increment a `deferredCount` for every request in the queue. 
*   **Promotion**: If a request's `deferredCount` exceeds a config threshold (`agingNormalToVip`), a NORMAL request is forcefully promoted to a VIP request. The entire heap is then rebuilt (`heapifyDown`) to reflect the new priorities.

### D. The GUI Update Workflow
1. Every time a major event occurs (a request is queued, a driver is assigned, config is updated), the Main Server calls `updateSharedState()`.
2. This function locks the `SEM_SHM_LOCK` semaphore, `memcpy`s the current `PriorityQueue` size and `driverPool` statuses into the POSIX Shared Memory block, increments a "tick" counter, and unlocks the semaphore.
3. 60 times a second, the Request Server's Raylib GUI locks the semaphore, reads the memory block, and redraws the screen based on the updated data.

### E. Graceful Shutdown Workflow
1. When the user presses `Ctrl+C`, the OS sends `SIGINT`.
2. The `handleShutdown` signal handler catches it and sets the global `volatile sig_atomic_t systemRunning = 0;`.
3. The `main` loop stops reading the pipe. It calls `wakeAllRequests()` which **broadcasts** to all sleeping dispatchers so they can exit.
4. `pthread_join` waits for the dispatcher to finish. The IPC resources (pipes, shared memory, named semaphores) are unlinked and destroyed to prevent kernel memory leaks.

## 4. Potential Viva Questions & Answers

### Q1: What is the difference between a Mutex and a Semaphore?
- **Answer**: A **Mutex** is a locking mechanism used to synchronize access to a resource. Only the thread that locked it can unlock it (ownership). A **Semaphore** is a signaling mechanism (a counter). In this project, we use a **Named Semaphore** as a binary semaphore (0 or 1) to synchronize two different processes, whereas Mutexes are used within a single process for threads.

### Q2: Why did you use `pthread_cond_timedwait` instead of `pthread_cond_wait`?
- **Answer**: In `requestThread`, we use `timedwait` to implement the **Timeout** feature. If a request isn't assigned a driver within $X$ seconds, it wakes up automatically, cancels itself, and cleans up.

### Q3: How do you handle "Starvation"?
- **Answer**: We use an **Aging Algorithm**. Every time the dispatcher fails to find a driver for a request, we increment a `deferredCount`. Once it hits a threshold, we promote the request to a higher priority.

### Q4: Why use Shared Memory for state but Pipes for requests?
- **Answer**: **Pipes (FIFO)** are great for discrete messages (events) like "create a request." **Shared Memory** is better for "frequent state updates" because it allows the GUI to read the entire system state (all drivers/queue) very quickly without the overhead of copying data through the kernel multiple times.

### Q5: What is a "Race Condition" and where could it happen here?
- **Answer**: A race condition occurs when two threads access shared data simultaneously and at least one is writing. Example: Two `rideThreads` finishing at the same time and trying to update `driverPool[i].status`. We prevent this using `driverMutex`.

### Q6: What does `pthread_detach` do?
- **Answer**: It tells the system that we don't need to "join" the thread later to get its return value. The system automatically reclaims the thread's resources when it finishes. We use it for `rideThread` because they finish independently.

### Q7: What is the purpose of `sig_atomic_t systemRunning`?
- **Answer**: It is a type that is guaranteed to be accessed atomically. We use it for the global shutdown flag so that when `SIGINT` (Ctrl+C) is caught, all loops (`while(systemRunning)`) can exit safely and perform cleanup.

---

## 5. File Cheat Sheet
- `main_server/src/main.c`: Initialization and signal handling.
- `main_server/src/queue.c`: Heap logic, Mutex/Cond Var for queue.
- `main_server/src/dispatch.c`: Dispatcher thread logic (The "Brain").
- `main_server/src/request.c`: Request/Ride thread logic.
- `main_server/src/driver.c`: Driver status and search logic.
- `main_server/src/ipc.c`: Pipe and Shared Memory setup.
- `shared/include/ipc_types.h`: Common structures used by both processes.
