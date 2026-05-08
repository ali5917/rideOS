#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include "request.h"

#define MAX_QUEUE_SIZE 100
#define MAX_DEFER_LIMIT 10

typedef struct {
    RideRequest* heap[MAX_QUEUE_SIZE];
    int size;
    pthread_mutex_t lock;
    pthread_cond_t notEmpty;
} PriorityQueue;

void initializeRequestQueue(PriorityQueue* pq);
void destroyRequestQueue(PriorityQueue* pq);
void insertRequest(PriorityQueue* q, RideRequest* req);
RideRequest* getRequest(PriorityQueue* q);
void updateDeferCount(PriorityQueue* q);

#endif
