#ifndef LO41_TOWER_H
#define LO41_TOWER_H

#include "plane.h"

#define NUMBER_AIRPORT 30

#define NO_RUNWAY -1
#define SMALL_RUNWAY 0
#define LARGE_RUNWAY 1

#define LATITUDE 0
#define LONGITUDE 1

typedef struct {
    char* name;
    position position;
} airport;

extern const airport airports[];
extern pthread_mutex_t mutex[];

void initRunways();

void deleteRunways();

void cleanupHandler(void *arg);

void incrementTime(struct timespec *ts);

void getVector(float vector[], const position *depart, const position *dest);

float distance(float vector[]);

void normalize(float normalizedVector[], const float vector[], float d);

void requestLanding(plane_struct *info);

void freeRunway(plane_struct *info);

void requestTakeoff(plane_struct *info);

#endif //LO41_TOWER_H
