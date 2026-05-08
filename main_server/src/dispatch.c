#include "../include/dispatch.h"
#include "../include/driver.h"
#include "../include/queue.h"
#include "../include/request.h"
#include "../include/ipc.h"
#include "../include/logger.h"

// TODO: Implement dispatcher_thread function.
// TODO: Logic: pop request -> find FREE driver (prefer VIP for EMERGENCY/VIP) -> assign -> signal request_thread -> spawn ride_thread.
// TODO: Implement starvation prevention: call age_waiting_requests() before sleeping/retrying.
// TODO: Write updated SharedState to shared memory after every state change.
