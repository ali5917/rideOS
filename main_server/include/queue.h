#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include "request.h"

#define MAX_QUEUE_SIZE 100

typedef struct {
    RideRequest* heap[MAX_QUEUE_SIZE];
    int size;
    pthread_mutex_t lock;
    pthread_cond_t notEmpty;
} PriorityQueue;

void initializeRequestQueue(PriorityQueue* q);

void insertRequest(PriorityQueue* q, RideRequest* req);

RideRequest* getRequest(PriorityQueue* q);

void updateDeferCount(PriorityQueue* q);

#endif
