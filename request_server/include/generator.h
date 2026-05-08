#ifndef GENERATOR_H
#define GENERATOR_H

#include <pthread.h>

void* generatorLoop(void* arg);
void generatorStop(void);

#endif
