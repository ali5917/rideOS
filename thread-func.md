Detailed Threads Functionalities

request_thread
Think of this as a customer calling for a ride. Each customer is their own thread.
Here is exactly what happens in order:

1. It spawns. A new thread is created representing one ride request — say, Request #47, type NORMAL.
2. It inserts into the PriorityQueue. Request #47 gets placed into the shared queue. The queue is ordered by priority, so EMERGENCY requests sit at the top, then VIP, then NORMAL at the bottom.
3. It signals the dispatcher. After inserting, it calls pthread_cond_signal on the not_empty condition. This is basically a tap on the dispatcher's shoulder saying "hey, something is in the queue, wake up."
4. It waits. Now the customer thread just sits and waits. It is not busy-looping — it is sleeping via pthread_cond_timedwait. This call sleeps the thread BUT with a deadline. For a NORMAL request the deadline might be 10 seconds (from config). For EMERGENCY it might be 3 seconds.
   5a. Happy path — dispatcher wakes it. If a driver is found in time, the dispatcher signals this thread back. The thread wakes up, sees its status is ASSIGNED, and exits cleanly.
   5b. Timeout path — nobody came. If the deadline expires before the dispatcher signals it, pthread_cond_timedwait returns a timeout code. The thread checks this, sets its own status to CANCELLED, writes a log line like [CANCELLED] Request #47, and exits. Nobody has to kill it — it cancels itself.

dispatcher_thread
This is the single brain of the whole system. There is exactly one dispatcher thread, always running.

1. It sleeps until there is work. It is blocked on pthread_cond_wait for the not_empty signal. When a request_thread inserts something and signals, the dispatcher wakes up.
2. It pops the highest priority request. It takes whatever is at the top of the heap — say there are 5 requests, it takes the EMERGENCY one. The others stay in the queue.
3. It looks for a free driver. It scans the driver list for anyone with state FREE. If the request is EMERGENCY or VIP, it specifically looks for a VIP-tier driver first before settling for a normal driver.
   4a. Driver found. Mark that driver BUSY, signal the request_thread (the customer) that it has been assigned, and spawn a ride_thread to handle the actual ride. Dispatcher is now free to loop back and handle the next request.
   4b. No driver found — this is where aging happens. The dispatcher has nobody to give the request to. It re-inserts the request back into the queue. Now, before it sleeps 500ms to wait for a driver to free up, it calls age_waiting_requests(). This function walks every request currently sitting in the queue and increments their wait_ticks by 1 — meaning "you were skipped one more time." Then it checks: if a NORMAL request's wait_ticks has hit the threshold (say 5), it gets promoted to VIP. Its priority field is updated, a PROMOTION log line is written, and wait_ticks resets to zero. Same logic for VIP reaching its threshold — it becomes EMERGENCY. After all promotions are done, the heap is rebuilt because priorities changed mid-queue. Then the dispatcher sleeps 500ms and retries.
   The key insight here is that a NORMAL request that keeps getting skipped because EMERGENCY and VIP requests keep arriving will eventually become VIP itself, and then EMERGENCY, guaranteeing it eventually gets served. Without this, it could wait forever.

ride_thread
This is the actual trip happening. It is spawned by the dispatcher once a driver is assigned.

1. It checks the queue depth. Before doing anything else it looks at how many requests are currently pending in the queue. If that number is above surge_threshold (from config, say 10), it applies surge pricing — multiplies the fare by some factor. This simulates real surge pricing: high demand = higher price.
2. It sleeps for ride_duration. This simulates the trip happening. The thread just sleeps — say 8 seconds. Nothing else needs to happen during this time.
3. Driver is marked FREE. When it wakes up, the ride is over. It sets the driver's state back to FREE. If the driver's state was GOING_OFFLINE (the driver asked to go offline mid-ride), it now sets them to OFFLINE instead of FREE.
4. It signals the dispatcher. Calls pthread_cond_signal to wake the dispatcher again, because a new free driver is now available and there may be pending requests waiting.
5. It updates metrics and logs. Increments the completed ride counter, records the wait time for this request under its original_type bucket, and writes a [COMPLETED] Request #47 log line. Then the thread exits.
