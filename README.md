# RideOS: An Autonomous Ride Sharing Dispatch System

RideOS is a concurrent system designed to simulate the complexities of a real-time ride-sharing dispatch environment. The project demonstrates advanced Operating Systems concepts, including multithreading, inter-process communication (IPC), and synchronization primitives. It utilizes a dual-process architecture to decouple backend dispatch logic from the frontend graphical user interface.

## Group Members

| Name | ID |
| :--- | :--- |
| Ali Kashif | 24K-0802 |
| Ismail Silat | 24K-0546 |
| Hammad Abdul Rahim | 24K-0581 |

## Core Technical Features

- **Multithreaded Dispatch Engine**: Implements a producer-consumer model using POSIX threads (pthreads) to manage incoming ride requests and driver assignments concurrently.
- **Priority-Based Scheduling & Aging**: Features a robust scheduling algorithm that prioritizes Emergency and VIP requests. It incorporates an aging mechanism to prevent process starvation for standard requests.
- **Advanced Synchronization**: Ensures data integrity across threads using Mutexes for mutual exclusion and Condition Variables for thread signaling and coordination.
- **Inter-Process Communication (IPC)**:
    - **Named Pipes (FIFOs)**: Used for reliable message passing from the Request Server to the Main Server.
    - **POSIX Shared Memory & Semaphores**: Facilitates high-speed state synchronization between processes, protected by semaphores to prevent race conditions during concurrent access.
- **Real-Time Resource Monitoring**: A Raylib-based dashboard that visualizes driver states (Idle, Busy, Offline) and pending request queues.

## System Architecture

The simulation is partitioned into two distinct processes:

1.  **Main Server (Backend)**: Responsible for the core simulation logic. It manages the global driver pool, maintains the request queue, and executes the dispatcher thread which performs the matching logic.
2.  **Request Server (Frontend)**: Handles user interaction and request generation. It hosts an automated request generator thread and provides a GUI for manual request injection and system configuration.

## Visual Interface

### 1. Intro Screen
The landing interface featuring the deterministic dispatch wordmark and entry button.
![Intro Screen](screenshots/intro.png)

### 2. Configuration & Drivers' Fleet Selection
Users select the initial drivers' fleet size to initialize the global driver pool.
![Configuration Screen](screenshots/selectDrivers.png)

### 3. Real-Time Dashboard
The primary monitoring interface displaying driver status cards, pending queues, and live activity logs.
![Dashboard](screenshots/dashboard.png)

### 4. Performance Metrics
A summarized report generated upon simulation termination, displaying system efficiency and wait time analytics.
![Metrics Screen](screenshots/metrics.png)

## Build and Execution

### Requirements
- GCC (C11 Standard)
- Raylib Development Libraries
- POSIX Thread Support (lpthread)

### Compilation
Use the provided Makefile to compile both modules:
```bash
make
```

### Execution Sequence
To establish the IPC channels correctly, the Main Server must be initialized first:

1. **Initialize Main Server**:
   ```bash
   ./main_server_bin
   ```
2. **Launch GUI Dashboard**:
   ```bash
   ./request_server_bin
   ```

## User Interface Controls

| Key | Function |
| :--- | :--- |
| **R** | Submit a manual request into the IPC pipe |
| **TAB** | Cycle through request priority levels (Normal, VIP, Emergency) |
| **E** | Trigger simulation termination and generate metrics report |
| **ESC** | Graceful shutdown of the process |

## Simulation Metrics
Upon termination, the system generates a performance report based on the following telemetry:
- **Wait Time Analysis**: Calculation of average response time per priority tier.
- **Throughput & Efficiency**: Tracking of completed rides versus cancelled/timed-out requests.
- **Resource Utilization**: Percentage-based tracking of driver fleet activity.

---
