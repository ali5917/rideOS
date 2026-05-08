INTRODUCTION
This project aims to simulate a ride-sharing backend system where multiple
customer ride requests and drivers operate concurrently. Each ride request is
modeled as a separate thread (producer), while drivers are treated as shared
resources. The system assigns available drivers to ride requests using synchronization
mechanisms such as mutexes and semaphores to prevent race conditions and
ensure safe concurrent access to shared data structures.
This project is highly relevant to Operating Systems concepts because it
demonstrates thread management, synchronization, resource allocation, deadlock
prevention, and concurrency control while ensuring fairness and performance.
Real-world ride-hailing platforms such as Uber and Careem use similar backend
logic to handle thousands of concurrent ride requests safely and efficiently.

OBJECTIVES
Main Objective:
To design and implement a concurrent ride-sharing dispatch system using threads,
mutexes, and semaphores in C, ensuring safe and deadlock-free driver allocation.
Specific Objectives:
1. To simulate multiple ride request threads operating concurrently.
2. To implement a shared driver pool protected by mutexes.
3. To ensure deadlock-free and race-condition-free dispatch logic.
4. To measure and analyze system performance under concurrent load.
5. To visualize driver-request assignments through a real-time interface.

OPERATING SYSTEM CONCEPTS COVERED
Concept How it will be implemented
Concurrency Multiple ride request threads using

pthread_create()

Thread Management Creation, execution, and termination of request

threads

Synchronization pthread_mutex_t to protect driver pool
Semaphores sem_t to limit dispatch operations
Resource Allocation Drivers treated as limited shared resources
Deadlock Prevention Ordered locking and non-blocking resource

acquisition

Race Condition
Handling

Critical section protection for matching logic

Performance
Monitoring

Measurement of wait time, throughput, and
driver utilization

METHODOLOGY

Programming Language & Tools
Language: C
Compiler: gcc
Development Environment: Linux/Ubuntu
Libraries:
● pthread.h (threading)
● semaphore.h (semaphores)
● GUI library (Raylib)
Version Control: GitHub

Core Implementation Approach
Ride Request Threads
Each incoming ride request will be created as a separate thread using
pthread_create(). The thread will attempt to acquire a driver from the shared pool.
Shared Driver Pool
Drivers will be stored in a shared array or list. Access to this pool will be protected
using a mutex to prevent concurrent modification.
Mutex-Protected Matching
Before assigning a driver, the request thread will lock the mutex, search for an
available driver, mark it as busy, and release the mutex.
Deadlock-Free Dispatch Logic
Semaphores will limit the number of concurrent dispatch operations. Lock
acquisition order will be strictly defined to avoid circular wait conditions.
Additional Features Planned
S.No Feature Brief Description
1 Ride Cancellation Requests can cancel before driver assignment
2 Driver Priority Levels VIP drivers assigned first
3 Surge Pricing
Simulation

Increased fare during high demand

4 Activity Logging Log file recording assignments and events
5 Performance
Metrics

Measure average waiting time & completed rides

SYSTEM ARCHITECTURE/DESIGN
The system consists of the following modules:
1. Ride Request Generator (creates request threads)
2. Driver Pool Manager (maintains shared driver list)
3. Dispatch Controller (handles matching logic)
4. Synchronization Layer (mutex + semaphore)

5. Visualization Module (GUI interface)
Data Flow:
Ride Request Thread → Lock Mutex → Check Driver Pool → Assign Driver → Update
Stats → Unlock Mutex → Display Update
The semaphore ensures that dispatch operations do not exceed system capacity,
while the mutex ensures exclusive access to the shared driver pool.

DATA STRUCTURES USED
Data Structure Purpose Synchronization Mechanism
Array/List Store driver information Mutex
Queue Store pending ride requests Mutex
Structure Driver & request metadata Mutex
Semaphore Counter Limit dispatch operations Semaphore

INTERFACE PLAN
Type: GUI
The interface will display:
● Active drivers (Available/Busy status)
● Incoming ride requests
● Real-time driver-request matching
● Completed rides counter
● Waiting time statistics
● Activity log panel
Key Features:
1. Color-coded driver status (Green = Free, Red = Busy)
2. Real-time updates of assignments
3. Performance statistics dashboard

TESTING PLAN
Test Case Input Expected Output OS Concept Tested
1 Multiple simultaneous

requests

Safe driver allocation Concurrency

2 Limited drivers Requests wait properly Semaphore
3 High load No race conditions Mutex
4 Cancellation during wait Safe removal of request Synchronization

PROJECT PLAN (1 DAY, 2 STUDENTS)
Scope (Day-1 Feasible)
1. Console-based simulation (no GUI unless time permits)
2. pthreads + mutex + semaphore core synchronization
3. Driver pool + request threads + stats
4. Logging to terminal (optional log file)

Environment Target
● Primary development on macOS
● Final testing on Ubuntu VM
● Use gcc/clang with pthread support

Work Breakdown
Student A (Core Concurrency)
1. Define driver and request structures
2. Implement shared driver pool with mutex
3. Request thread logic (allocate driver, simulate ride, release driver)

Student B (Control + Metrics)
1. Implement semaphore-based dispatch limiting
2. Collect statistics (avg wait time, completed rides, utilization)
3. Add logging + test cases

Timeline (Suggested)
Hour 1: Finalize constants, data structures, and thread workflow
Hours 2–3: Implement driver pool + request threads + mutex locking
Hours 4–5: Add semaphores + stats + clean shutdown
Hours 6–7: Run tests, fix race conditions, verify on Ubuntu
Hours 8–9: Prepare report summary + demo output screenshots

Deliverables
1. main.c with working dispatcher simulation
2. Console demo output showing safe assignment under load
3. Short report section: methodology, OS concepts, testing, results

Risk Control (If time is short)
1. Skip GUI and advanced features
2. Keep only essential stats
3. Hardcode a small set of test scenarios