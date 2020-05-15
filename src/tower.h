#ifndef LO41_TOWER_H
#define LO41_TOWER_H

#include "plane.h"

#define NUMBERAIRPORT 20

extern const char *airportNames[];
extern pthread_mutex_t mutex[];

void initRunways();

void deleteRunways();

void cleanupHandler(void *);

void requestLanding(plane_struct *info);

void freeRunway(plane_struct *info);

void requestTakeoff(plane_struct *info);

#endif //LO41_TOWER_H
