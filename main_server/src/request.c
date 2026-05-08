#include "../include/request.h"
#include "../include/queue.h"
#include "../include/logger.h"
#include "../include/ipc.h"

// TODO: Implement request_thread function.
// TODO: Logic: insert into PriorityQueue -> signal dispatcher -> pthread_cond_timedwait (per-category timeout).
// TODO: Handle Happy path (ASSIGNED) vs Timeout path (CANCELLED).

// TODO: Implement ride_thread function.
// TODO: Logic: apply surge pricing if queue size > threshold -> sleep (duration) -> mark driver FREE -> signal dispatcher -> update metrics.
