#ifndef LO41_PLANE_H
#define LO41_PLANE_H

#include "main.h"

#define CRITICAL_FUEL_LIMIT 25
#define RESPOND_EVERY 250
#define SPEED 15

#define HANGAR 0
#define ROLLING 1
#define TAKEOFF 2
#define LANDING 3
#define FLYING 4
#define WAITING_IN_FLIGHT 5
#define PRIORITY_IN_FLIGHT 6

#define NOT_REDIRECTED -1
#define MUST_NOT_REDIRECT -2

#define NORMAL 0
#define CRITICAL_FUEL 1
#define TECHNICAL_PROBLEM 2

#include <stdbool.h>

extern const char *sizes[];
extern const char *states[];
extern const char *conditions[];
extern const int sizeRequest;
extern const int sizeResponse;

typedef struct {
    int seats;
    char *model;
    bool large;
} plane_type;

typedef struct {
    int id;
    char *model;
    int fuel;
    int start;
    int destination;
    int actual;
    int redirection;
    float latitude;
    float longitude;
    float total_distance;
    int progress;
    int runwayNumber;
    int state;
    int condition;
    bool large;
} plane_struct;

typedef struct {
    long type;
} planeRequest;

typedef struct {
    long type;
    plane_struct planeInfo;
} planeResponse;

int newDestination(int depart);

void initPlane(plane_struct *info);

void decrementFuel(plane_struct *info);

bool move(plane_struct *info);

void sendRequestInfo(int id);

plane_struct getRequestResponse();

void respondInfoRequest(const plane_struct *info);

void asyncSleep(int nsec, plane_struct *info);

int fly(plane_struct *info);

void *plane(void *arg);

#endif //LO41_PLANE_H
