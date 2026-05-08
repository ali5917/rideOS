# Implementation Plan (Replanned)

This plan is based on the detailed design provided, with clear phases, deliverables, and a build order that fits a two-student workflow.

---

## Phase 0 — Setup (30 min)

**Project layout**
```
project/
├── main.c
├── driver.h / driver.c
├── request.h / request.c  
├── queue.h / queue.c
├── dispatch.h / dispatch.c
├── logger.h / logger.c
├── metrics.h / metrics.c
└── gui.h / gui.c
```

**Makefile (initial)**
```makefile
CC = gcc
FLAGS = -pthread -lraylib -lm
all: gcc $(FLAGS) *.c -o rideshare
```

**Deliverable**
- Clean build on macOS and Ubuntu VM

---

## Phase 1 — Core Data Structures (2–3 hrs)

**driver.h**
```c
typedef enum { FREE, BUSY, OFFLINE } DriverStatus;

typedef struct {
    int id;
    DriverStatus status;
    int is_vip;
    float rating;
    int rides_completed;
} Driver;

#define MAX_DRIVERS 10
Driver driver_pool[MAX_DRIVERS];
pthread_mutex_t driver_mutex;
```

**request.h**
```c
typedef enum { EMERGENCY = 0, VIP = 1, NORMAL = 2 } RequestType;
typedef enum { WAITING, ASSIGNED, COMPLETED, CANCELLED } RequestStatus;

typedef struct {
    int id;
    RequestType type;
    RequestStatus status;
    int assigned_driver_id;
    time_t request_time;
    int timeout_seconds;
    float fare;
    int ride_duration;
} RideRequest;
```

**queue.h**
```c
// Priority queue — lower enum value = higher priority
typedef struct QNode {
    RideRequest *request;
    struct QNode *next;
} QNode;

typedef struct {
    QNode *head;
    int size;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
} PriorityQueue;

void queue_insert(PriorityQueue *q, RideRequest *r);
RideRequest* queue_pop(PriorityQueue *q);
```

**Deliverable**
- Headers compile and data structures are defined

---

## Phase 2 — Logger (30 min)

```c
// logger.c
FILE *log_file;
pthread_mutex_t log_mutex;

void logger_init() {
    log_file = fopen("rideshare.log", "w");
}

void log_event(const char *event, RideRequest *r) {
    pthread_mutex_lock(&log_mutex);
    time_t now = time(NULL);
    fprintf(log_file, "[%s] %s | Request #%d | Type: %s | Driver: %d\n",
        ctime(&now), event, r->id, type_to_str(r->type), r->assigned_driver_id);
    fflush(log_file);
    pthread_mutex_unlock(&log_mutex);
}
```

**Events to log**
- REQUEST_CREATED
- REQUEST_ASSIGNED
- REQUEST_COMPLETED
- REQUEST_CANCELLED
- DRIVER_OFFLINE / DRIVER_ONLINE

**Deliverable**
- Logs written to rideshare.log with timestamps

---

## Phase 3 — Request Thread Logic (2–3 hrs)

```c
void *request_thread(void *arg) {
    RideRequest *req = (RideRequest *)arg;
    req->status = WAITING;
    log_event("REQUEST_CREATED", req);

    queue_insert(&pending_queue, req);
    pthread_cond_signal(&pending_queue.not_empty);

    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += req->timeout_seconds;

    pthread_mutex_lock(&req->wait_mutex);
    while (req->status == WAITING) {
        int result = pthread_cond_timedwait(&req->assigned_cond,
                                             &req->wait_mutex,
                                             &deadline);
        if (result == ETIMEDOUT) {
            req->status = CANCELLED;
            log_event("REQUEST_CANCELLED", req);
            queue_remove(&pending_queue, req);
            pthread_mutex_unlock(&req->wait_mutex);
            return NULL;
        }
    }
    pthread_mutex_unlock(&req->wait_mutex);
    return NULL;
}
```

**Deliverable**
- Request thread enqueues + times out cleanly

---

## Phase 4 — Dispatcher Logic (2–3 hrs)

```c
void *dispatcher_thread(void *arg) {
    while (system_running) {
        pthread_mutex_lock(&pending_queue.lock);
        while (pending_queue.size == 0) {
            pthread_cond_wait(&pending_queue.not_empty, &pending_queue.lock);
        }

        RideRequest *req = queue_pop(&pending_queue);
        pthread_mutex_unlock(&pending_queue.lock);

        pthread_mutex_lock(&driver_mutex);
        Driver *driver = find_free_driver(req->type);

        if (driver == NULL) {
            queue_insert(&pending_queue, req);
            pthread_mutex_unlock(&driver_mutex);
            usleep(500000);
            continue;
        }

        driver->status = BUSY;
        req->assigned_driver_id = driver->id;
        req->status = ASSIGNED;
        pthread_mutex_unlock(&driver_mutex);

        pthread_mutex_lock(&req->wait_mutex);
        pthread_cond_signal(&req->assigned_cond);
        pthread_mutex_unlock(&req->wait_mutex);

        log_event("REQUEST_ASSIGNED", req);

        pthread_t ride_tid;
        pthread_create(&ride_tid, NULL, ride_thread, (void*)req);
        pthread_detach(ride_tid);
    }
    return NULL;
}
```

**find_free_driver logic**
```c
Driver* find_free_driver(RequestType type) {
    // EMERGENCY/VIP → prefer VIP drivers first
    // NORMAL → any free driver
    // Return NULL if none available
}
```

**Deliverable**
- Dispatcher assigns drivers without race conditions

---

## Phase 5 — Ride Execution Thread (1 hr)

```c
void *ride_thread(void *arg) {
    RideRequest *req = (RideRequest *)arg;

    if (pending_queue.size > SURGE_THRESHOLD) {
        req->fare *= 1.5;
    }

    sleep(req->ride_duration);

    pthread_mutex_lock(&driver_mutex);
    driver_pool[req->assigned_driver_id].status = FREE;
    driver_pool[req->assigned_driver_id].rides_completed++;
    pthread_mutex_unlock(&driver_mutex);

    req->status = COMPLETED;
    log_event("REQUEST_COMPLETED", req);
    update_metrics(req);

    pthread_cond_signal(&pending_queue.not_empty);
    return NULL;
}
```

**Deliverable**
- Driver release and completion metrics work

---

## Phase 6 — Metrics (1 hr)

```c
typedef struct {
    int total_requests;
    int completed;
    int cancelled;
    double avg_wait_time;
    double driver_utilization;
    int emergency_completed;
    int vip_completed;
    int normal_completed;
} Metrics;

void print_metrics();
void export_metrics_to_file();
```

**Deliverable**
- Final stats printed and/or exported

---

## Phase 7 — Dynamic Drivers (1 hr)

```c
void *driver_lifecycle_thread(void *arg) {
    while (system_running) {
        sleep(rand() % 10 + 5);

        pthread_mutex_lock(&driver_mutex);
        int i = rand() % MAX_DRIVERS;

        if (driver_pool[i].status == FREE) {
            driver_pool[i].status = OFFLINE;
            log_event("DRIVER_OFFLINE", ...);
        } else if (driver_pool[i].status == OFFLINE) {
            driver_pool[i].status = FREE;
            log_event("DRIVER_ONLINE", ...);
        }
        pthread_mutex_unlock(&driver_mutex);
    }
}
```

**Deliverable**
- Drivers can go offline/online without breaking dispatch

---

## Phase 8 — GUI with Raylib (3–4 hrs)

**Rule:** GUI reads only; it never writes to shared state.

```c
typedef struct {
    Driver drivers_snapshot[MAX_DRIVERS];
    RideRequest recent_requests[50];
    Metrics current_metrics;
    char log_lines[20][256];
    pthread_mutex_t snapshot_mutex;
} GUIState;
```

**Layout**
```
┌─────────────────────────────────────────────────┐
│           RIDE SHARING DISPATCH SYSTEM           │
├──────────────┬──────────────┬────────────────────┤
│   DRIVERS    │  PENDING     │   METRICS          │
│              │  REQUESTS    │                    │
│ [1] 🟢 FREE  │ 🔴 EMG #23  │ Total:  45         │
│ [2] 🔴 BUSY  │ 🟡 VIP #24  │ Done:   38         │
│ [3] ⚫ OFF   │ ⚪ NRM #25  │ Cancel: 4          │
│              │              │ Avg Wait: 12s      │
│              │              │ Surge: ACTIVE 🔥   │
├──────────────┴──────────────┴────────────────────┤
│  ACTIVITY LOG                                    │
│  [12:03:01] Request #23 ASSIGNED to Driver 1     │
│  [12:03:05] Request #20 COMPLETED                │
│  [12:03:09] Request #22 CANCELLED (timeout)      │
└─────────────────────────────────────────────────┘
```

**Deliverable**
- GUI renders live state snapshots

---

## Thread Summary

| Thread | Count | Role |
|---|---|---|
| request_thread | 1 per request | Producer — enters queue, waits, times out |
| dispatcher_thread | 1 | Matches requests to drivers |
| ride_thread | 1 per active ride | Simulates ride, releases driver |
| driver_lifecycle_thread | 1 | Toggles drivers online/offline |
| gui_thread (main) | 1 | Raylib render loop |

---

## Build Order

**Day 1**
- Phase 0 — File structure
- Phase 1 — Structs & data structures
- Phase 2 — Logger
- Phase 3 — Request threads (test in terminal)
- Phase 4 — Dispatcher (test in terminal)
- Phase 5 — Ride execution

**Day 2**
- Phase 6 — Metrics
- Phase 7 — Dynamic drivers
- Phase 8 — GUI
- Testing & debugging

**Rule:** Terminal output must be stable before GUI.
