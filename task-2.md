# Implementation Plan (Aligned to Two-Process Architecture)

This plan mirrors the current design: Main Server owns all dispatch logic, Request Server owns GUI and request generation. Each phase includes responsibilities so two students can work in parallel without conflicts.

---

## Phase 0 — Setup & Build (45 min)

**Goals**
- Confirm two-process layout and Makefile targets for `main_server` and `request_server`.
- Verify shared header contract for IPC structs.

**Responsibilities**
- **Student A (Main Server):**
    - Set up [main_server/include](main_server/include) and [main_server/src](main_server/src) build target.
    - Stub `main_server/src/main.c` with clean startup/shutdown.
- **Student B (Request Server):**
    - Set up [request_server/include](request_server/include) and [request_server/src](request_server/src) build target.
    - Stub `request_server/src/main.c` with GUI loop skeleton.

**Deliverable**
- Two executables build cleanly (no GUI logic in Main Server).

---

## Phase 1 — Core Types & Config (2–3 hrs)

**Goals**
- Define shared request/driver/state structures and config parsing.
- Establish configuration precedence: CLI overrides config.txt.

**Responsibilities**
- **Student A (Main Server):**
    - Implement config parsing in [main_server/src/config.c](main_server/src/config.c) + [main_server/include/config.h](main_server/include/config.h).
    - Define `RideRequest`, `Driver`, `SharedState` in [main_server/include/request.h](main_server/include/request.h) and [main_server/include/driver.h](main_server/include/driver.h) (and mirrored types for Request Server if required).
- **Student B (Request Server):**
    - Mirror shared structs in [request_server/include](request_server/include) and validate sizes/fields match the Main Server definitions.
    - Implement request generator config in [request_server/src/generator.c](request_server/src/generator.c).

**Deliverable**
- Structs compile; config is loaded by both processes.

---

## Phase 2 — IPC: Named Pipe + Shared Memory (2–3 hrs)

**Goals**
- Pipe for requests (Request Server → Main Server).
- Shared memory for state broadcast (Main Server → Request Server) protected by named semaphore `shm_lock`.

**Responsibilities**
- **Student A (Main Server):**
    - Implement pipe read loop in [main_server/src/ipc.c](main_server/src/ipc.c).
    - Initialize shared memory and write `SharedState` snapshots after every state change.
- **Student B (Request Server):**
    - Implement pipe write function in [request_server/src/ipc.c](request_server/src/ipc.c).
    - Implement shared memory read with semaphore protection for GUI render.

**Deliverable**
- Request Server can send a `PipeRequest` and Main Server receives it.
- Request Server can read `SharedState` snapshots without crashing.

---

## Phase 3 — Priority Queue + Aging (2–3 hrs)

**Goals**
- Priority queue with EMERGENCY > VIP > NORMAL.
- Starvation prevention using wait ticks and promotions.

**Responsibilities**
- **Student A (Main Server):**
    - Implement queue in [main_server/src/queue.c](main_server/src/queue.c) + [main_server/include/queue.h](main_server/include/queue.h).
    - Implement `age_waiting_requests()` and promotion logging.
- **Student B (Request Server):**
    - Provide test harness in Request Server (optional CLI or temporary) to flood requests for aging validation.

**Deliverable**
- Queue supports insert/pop/remove/rebuild; promotions logged.

---

## Phase 4 — Request Threads + Timeouts (2–3 hrs)

**Goals**
- Each request thread waits with `pthread_cond_timedwait` and cancels on timeout.
- Logs `REQUEST_CREATED`, `REQUEST_CANCELLED`.

**Responsibilities**
- **Student A (Main Server):**
    - Implement request thread in [main_server/src/request.c](main_server/src/request.c).
    - Ensure removal from queue on timeout.
- **Student B (Request Server):**
    - Implement request generation flow to pipe using GUI and/or random generator.

**Deliverable**
- Requests time out correctly by category.

---

## Phase 5 — Dispatcher + Driver Pool (3–4 hrs)

**Goals**
- Dedicated dispatcher thread selects top priority request and assigns drivers.
- VIP drivers preferred for EMERGENCY/VIP.
- Driver lifecycle state: FREE → BUSY → FREE and OFFLINE with GOING_OFFLINE handling.

**Responsibilities**
- **Student A (Main Server):**
    - Implement dispatcher in [main_server/src/dispatch.c](main_server/src/dispatch.c).
    - Implement driver pool and lifecycle in [main_server/src/driver.c](main_server/src/driver.c).
- **Student B (Request Server):**
    - Provide GUI visualization for driver states (including GOING_OFFLINE).

**Deliverable**
- Dispatcher assigns drivers without race conditions; drivers transition correctly.

---

## Phase 6 — Ride Thread + Surge Pricing (2–3 hrs)

**Goals**
- Ride thread simulates ride duration and releases driver.
- Surge pricing activates when queue size exceeds threshold.

**Responsibilities**
- **Student A (Main Server):**
    - Implement `ride_thread` in [main_server/src/dispatch.c](main_server/src/dispatch.c) or [main_server/src/request.c](main_server/src/request.c).
    - Update shared state after start/end of ride.
- **Student B (Request Server):**
    - Display surge indicator in GUI based on shared state.

**Deliverable**
- Ride completion updates metrics and logs.

---

## Phase 7 — Logger + Metrics (2–3 hrs)

**Goals**
- Central logging for all lifecycle events with request IDs.
- Final metrics report at shutdown.

**Responsibilities**
- **Student A (Main Server):**
    - Implement logger in [main_server/src/logger.c](main_server/src/logger.c).
    - Implement metrics in [main_server/src/metrics.c](main_server/src/metrics.c).
- **Student B (Request Server):**
    - Expose recent log lines in GUI (from shared state snapshot only).

**Deliverable**
- Logs and metrics verified in terminal output.

---

## Phase 8 — GUI + Request Generator (3–4 hrs)

**Goals**
- GUI reads shared state only; no writes.
- Manual request submission + random generation toggle.

**Responsibilities**
- **Student A (Main Server):**
    - Ensure shared memory snapshot has everything GUI needs: drivers, pending requests, metrics, surge, log lines.
- **Student B (Request Server):**
    - Implement GUI in [request_server/src/gui.c](request_server/src/gui.c).
    - Implement generator in [request_server/src/generator.c](request_server/src/generator.c).

**Deliverable**
- GUI renders live state and request submission works.

---

## Phase 9 — Shutdown & Clean Exit (1–2 hrs)

**Goals**
- Wait for in-flight rides using `active_rides` counter.
- Cleanly close pipe, shared memory, and semaphores.

**Responsibilities**
- **Student A (Main Server):**
    - Implement graceful shutdown in [main_server/src/main.c](main_server/src/main.c).
- **Student B (Request Server):**
    - Ensure GUI exits cleanly and stops generator threads.

**Deliverable**
- Metrics report printed after all rides drain.

---

## Integration Checklist

- Named pipe: Request Server → Main Server only.
- Shared memory: Main Server writes, Request Server reads.
- All shared memory access guarded by `shm_lock`.
- No UNIX signals; timeouts via `pthread_cond_timedwait`.
- Dispatcher is single-threaded.
- GUI never writes shared memory.

---

## Build Order (Two-Day)

**Day 1**
- Phase 0 → Phase 3
- Validate pipe + shared memory

**Day 2**
- Phase 4 → Phase 9
- Stabilize terminal output before final GUI polish
    // NORMAL → any free driver
