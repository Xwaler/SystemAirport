#ifndef LO41_TOWER_H
#define LO41_TOWER_H

#include "plane.h"

#define NUMBER_AIRPORT 30

#define NO_RUNWAY -1
#define SMALL_RUNWAY 0
#define LARGE_RUNWAY 1

#define LATITUDE 0
#define LONGITUDE 1

extern const char *airportNames[];
extern const int airportPostitions[][2];
extern pthread_mutex_t mutex[];

void initRunways();

void deleteRunways();

void cleanupHandler(void *);

void vectorPlane(float *, float, float , const int *);

void vectorAirport(float *, const int *, const int *);

float distance(float []);

void normalize(float [], const float [], float);

void requestLanding(plane_struct *info);

void freeRunway(plane_struct *info);

void requestTakeoff(plane_struct *info);

#endif //LO41_TOWER_H