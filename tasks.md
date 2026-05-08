# Task List: RideOS Implementation

## Phase 1: Shared IPC Definitions (Foundation)
1.  Create `main_server/include/ipc.h`: Define `PipeRequest` and `SharedState` structures.
2.  Create `main_server/include/config.h`: Define all constants (timeouts, max drivers, thresholds).

## Phase 2: Main Server Data Structures
3.  Implement `driver.h` & `request.h`: Define core structs for drivers and ride requests.
4.  Implement `queue.h` & `queue.c`: Build the thread-safe Priority Queue with Aging support.

## Phase 3: Main Server Core Logic
5.  Implement `logger.c` & `metrics.c`: Thread-safe logging and statistics collection.
6.  Implement `request.c`: `request_thread` (producer logic) and `ride_thread` (simulation).
7.  Implement `dispatch.c`: `dispatcher_thread` (the matching brain) and Shared Memory updates.

## Phase 4: Main Server Entry & IPC
8.  Implement `ipc.c` (Main Server): Named pipe reading and Shared Memory writing.
9.  Implement `main.c` (Main Server): Orchestration and cleanup logic.

## Phase 5: Request Server
10. Implement `ipc.c` (Request Server): Pipe writing and Shared Memory reading.
11. Implement `gui.c`: Raylib dashboard visualization.
12. Implement `main.c` (Request Server): GUI loop and interaction.

## Phase 6: Final Features & Testing
13. Driver Lifecycle: Randomly toggle driver availability.
14. Stress Testing: Verify synchronization under high load.
