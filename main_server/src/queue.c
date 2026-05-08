#include "../include/queue.h"

// Priority: EMERGENCY > VIP > NORMAL
int getPriority(RequestType type) {
    switch (type) {
        case EMERGENCY: 
            return 0; 
        case VIP:       
            return 1;
        case NORMAL:    
            return 2; 
        default:        
            return 3;
    }
}

void initializeRequestQueue(PriorityQueue* q) {
    q->size = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->notEmpty, NULL);
}

static void swap(RideRequest** a, RideRequest** b) {
    RideRequest* temp = *a;
    *a = *b;
    *b = temp;
}

void insertRequest(PriorityQueue* q, RideRequest* req) {
    pthread_mutex_lock(&q->lock);
    
    if (q->size >= MAX_QUEUE_SIZE) {
        fprintf(stderr, "Queue Overflow!\n");
        pthread_mutex_unlock(&q->lock);
        return;
    }

    // Insert at the end and bubble up
    int i = q->size++;
    q->heap[i] = req;

    while (i > 0) {
        int parent = (i - 1) / 2;
        if (getPriority(q->heap[i]->type) < getPriority(q->heap[parent]->type)) {
            swap(&q->heap[i], &q->heap[parent]);
            i = parent;
        } else {
            break;
        }
    }

    pthread_cond_signal(&q->notEmpty);
    pthread_mutex_unlock(&q->lock);
}

RideRequest* getRequest(PriorityQueue* q) {
    pthread_mutex_lock(&q->lock);

    while (q->size == 0) {
        pthread_cond_wait(&q->notEmpty, &q->lock);
    }

    RideRequest* top = q->heap[0];
    q->heap[0] = q->heap[--q->size];

    // Bubble down
    int i = 0;
    while (1) {
        int left = 2 * i + 1;
        int right = 2 * i + 2;
        int smallest = i;

        if (left < q->size && getPriority(q->heap[left]->type) < getPriority(q->heap[smallest]->type)) {
            smallest = left;
        }
        if (right < q->size && getPriority(q->heap[right]->type) < getPriority(q->heap[smallest]->type)) {
            smallest = right;
        }

        if (smallest != i) {
            swap(&q->heap[i], &q->heap[smallest]);
            i = smallest;
        } else {
            break;
        }
    }

    pthread_mutex_unlock(&q->lock);
    return top;
}

void updateDeferCount(PriorityQueue* q) {
    pthread_mutex_lock(&q->lock);
    
    int changed = 0;
    for (int i = 0; i < q->size; i++) {
        RideRequest* req = q->heap[i];
        req->deferredCount++;

        // Promotion Logic (Example: every 5 skips)
        if (req->deferredCount >= 5) {
            if (req->type == NORMAL) {
                req->type = VIP;
                req->deferredCount = 0;
                changed = 1;
                printf("[AGING] Request #%d promoted to VIP\n", req->id);
            } else if (req->type == VIP) {
                req->type = EMERGENCY;
                req->deferredCount = 0;
                changed = 1;
                printf("[AGING] Request #%d promoted to EMERGENCY\n", req->id);
            }
        }
    }

    // If types changed, the heap property might be violated. Rebuild it.
    if (changed) {
        // Simple Floyd's build-heap (O(n))
        for (int i = (q->size / 2) - 1; i >= 0; i--) {
            int j = i;
            while (1) {
                int left = 2 * j + 1;
                int right = 2 * j + 2;
                int smallest = j;

                if (left < q->size && getPriority(q->heap[left]->type) < getPriority(q->heap[smallest]->type))
                    smallest = left;
                if (right < q->size && getPriority(q->heap[right]->type) < getPriority(q->heap[smallest]->type))
                    smallest = right;

                if (smallest != j) {
                    swap(&q->heap[j], &q->heap[smallest]);
                    j = smallest;
                } else {
                    break;
                }
            }
        }
    }

    pthread_mutex_unlock(&q->lock);
}
