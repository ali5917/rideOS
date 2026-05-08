#include <stdio.h>
#include <stdlib.h>
#include "../include/queue.h"

// Priority: EMERGENCY (3) > VIP (2) > NORMAL (1)
int getPriority(RequestType type) {
    switch (type) {
        case EMERGENCY: 
            return 3; 
        case VIP:       
            return 2;
        case NORMAL:    
            return 1; 
        default:        
            return 0;
    }
}

// to maintain heap property after insertion
static void heapifyUp(PriorityQueue* pq, int index) {
    while (index > 0) {
        int parent = (index - 1) / 2;
        if (getPriority(pq->heap[index]->type) > getPriority(pq->heap[parent]->type)) {
            RideRequest* temp = pq->heap[index];
            pq->heap[index] = pq->heap[parent];
            pq->heap[parent] = temp;
            index = parent;
        } else {
            break;
        }
    }
}

// to maintain heap property after extraction
static void heapifyDown(PriorityQueue* pq, int index) {
    while (1) {
        int left = 2 * index + 1;
        int right = 2 * index + 2;
        int smallest = index;

        if (left < pq->size && getPriority(pq->heap[left]->type) > getPriority(pq->heap[smallest]->type))
            smallest = left;
        if (right < pq->size && getPriority(pq->heap[right]->type) > getPriority(pq->heap[smallest]->type))
            smallest = right;

        if (smallest != index) {
            RideRequest* temp = pq->heap[index];
            pq->heap[index] = pq->heap[smallest];
            pq->heap[smallest] = temp;
            index = smallest;
        } else {
            break;
        }
    }
}

void initializeRequestQueue(PriorityQueue* pq) {
    pq->size = 0;
    pthread_mutex_init(&pq->lock, NULL);
    pthread_cond_init(&pq->notEmpty, NULL);
}

void destroyRequestQueue(PriorityQueue* pq) {
    pthread_mutex_destroy(&pq->lock);
    pthread_cond_destroy(&pq->notEmpty);
}

void insertRequest(PriorityQueue* pq, RideRequest* req) {
    pthread_mutex_lock(&pq->lock);
    
    if (pq->size >= MAX_QUEUE_SIZE) {
        fprintf(stderr, "Queue Overflow!\n");
        pthread_mutex_unlock(&pq->lock);
        return;
    }

    pq->heap[pq->size] = req;
    heapifyUp(pq, pq->size);
    pq->size++;

    pthread_cond_signal(&pq->notEmpty);
    pthread_mutex_unlock(&pq->lock);
}

RideRequest* getRequest(PriorityQueue* pq) {
    pthread_mutex_lock(&pq->lock);

    while (pq->size == 0) {
        pthread_cond_wait(&pq->notEmpty, &pq->lock);
    }

    RideRequest* top = pq->heap[0];
    pq->heap[0] = pq->heap[pq->size - 1];
    pq->size--;

    if (pq->size > 0) {
        heapifyDown(pq, 0);
    }

    pthread_mutex_unlock(&pq->lock);
    return top;
}

void updateDeferCount(PriorityQueue* pq) {
    pthread_mutex_lock(&pq->lock);
    
    int changed = 0;
    for (int i = 0; i < pq->size; i++) {
        RideRequest* req = pq->heap[i];
        req->deferredCount++;

        if (req->deferredCount > MAX_DEFER_LIMIT) {
            if (req->type == NORMAL) {
                req->type = VIP;
                printf("AGING --- Request #%d promoted to VIP\n", req->id);
            } else if (req->type == VIP) {
                req->type = EMERGENCY;
                printf("AGING --- Request #%d promoted to EMERGENCY\n", req->id);
            }
            req->deferredCount = 0;
            changed = 1;
        }
    }

    // rebuild the entire heap if priority changed
    if (changed) {
        for (int i = (pq->size / 2) - 1; i >= 0; i--) {
            heapifyDown(pq, i);
        }
    }

    pthread_mutex_unlock(&pq->lock);
}