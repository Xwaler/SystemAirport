#ifndef LO41_TOWER_H
#define LO41_TOWER_H

#include "plane.h"

#define NUMBER_AIRPORT 30
#define NUMBER_SOLICITATION_TYPES 6

#define PRIORITIZED_LARGE_PLANE_LANDING 0
#define PRIORITIZED_SMALL_PLANE_LANDING 1
#define LARGE_PLANE_LANDING 2
#define SMALL_PLANE_LANDING 3
#define LARGE_PLANE_TAKEOFF 4
#define SMALL_PLANE_TAKEOFF 5

#define NO_RUNWAY -1
#define SMALL_RUNWAY 0
#define LARGE_RUNWAY 1

#define LATITUDE 0
#define LONGITUDE 1

typedef struct {
    char* name;
    position position;
} airport;

extern const airport airports[NUMBER_AIRPORT];
extern int numberPlanesWaiting[NUMBER_AIRPORT][NUMBER_SOLICITATION_TYPES];
extern pthread_mutex_t mutex[NUMBER_AIRPORT];

void initRunways();

void deleteRunways();

void cleanupHandler(void *arg);

void incrementTime(struct timespec *ts);

void getVector(float vector[], const position *origine, const position *dest);

float distance(float vector[]);

void normalize(float normalizedVector[], const float vector[], float d);

void requestLanding(plane_struct *info);

void freeRunway(plane_struct *info);

void requestTakeoff(plane_struct *info);

#endif //LO41_TOWER_H
