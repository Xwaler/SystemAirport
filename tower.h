#ifndef LO41_TOWER_H
#define LO41_TOWER_H

#include "plane.h"

void initRunways();

void deleteRunways();

void cleanupHandler(void *);

void requestLanding(plane_struct *info);

void freeRunway(plane_struct *info);

void requestTakeoff(plane_struct *info);

void tower(const int *);

#endif //LO41_TOWER_H
