#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "../include/generator.h"
#include "../include/ipc.h"

static volatile int generatorRunning = 1;

void* generatorLoop(void* arg) {
    int nextId = 1001; 
    srand(time(NULL));

    printf("GENERATOR --- Automatic request generation started.\n");

    while (generatorRunning) {
        // sleep for a random interval (2-6 seconds)
        int interval = (rand() % 5) + 2;
        sleep(interval);

        // create a new random request
        PipeRequest req;
        req.id = nextId++;
        
        // 60% normal, 30% vip, 10% emergency
        int r = rand() % 100;
        if (r < 60) req.type = NORMAL;
        else if (r < 90) req.type = VIP;
        else req.type = EMERGENCY;

        req.rideDuration = (rand() % 10) + 5; // 5-15 seconds
        req.requestTime = time(NULL);
        
        // base fare based on type
        if (req.type == EMERGENCY) req.baseFare = 50;
        else if (req.type == VIP) req.baseFare = 30;
        else req.baseFare = 15;

        // send via pipe
        if (writePipeRequest(&req) == 0) {
            printf("GENERATOR --- Sent Request #%d (Type: %s, Duration: %ds)\n", 
                   req.id, 
                   (req.type == EMERGENCY ? "EMERGENCY" : (req.type == VIP ? "VIP" : "NORMAL")),
                   req.rideDuration);
        } else {
            fprintf(stderr, "GENERATOR --- Main Server pipe not ready. Retrying...\n");
        }
    }

    return NULL;
}

void generatorStop(void) {
    generatorRunning = 0;
}
