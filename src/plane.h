#ifndef LO41_PLANE_H
#define LO41_PLANE_H

#include "main.h"

#define CRITICAL_FUEL_LIMIT 30
#define UPDATE_EVERY 200
#define FUEL_CONSUMPTION_RATE .40f
#define SPEED 8
#define TECHNICAL_PROBLEM_VALUE 1500

#define REFUEL_DURATION 2
#define EMBARKMENT_DURATION 2
#define TAKEOFF_DURATION 2
#define ROLLING_DURATION 1
#define LANDING_DURATION 1
#define DISBARKMENT_DURATION 2

#define ACCEPTABLE_LATENCY 4

#define DISBARKEMENT 0
#define HANGAR 1
#define EMBARKMENT 2
#define ROLLING 3
#define WAITING_TAKEOFF 4
#define PRIORITY_TAKEOFF 5
#define TAKEOFF 6
#define FLYING 7
#define PRIORITY_IN_FLIGHT 8
#define WAITING_LANDING 9
#define LANDING 10

#define MUST_NOT_REDIRECT -2
#define NOT_REDIRECTED -1

#define NONE -1
#define CRITICAL_FUEL 0
#define TECHNICAL_PROBLEM 1
#define CRASHED 2

#include <stdbool.h>

extern const char *sizes[];
extern const char *states[];
extern const char *alerts[];
extern const int sizeRequest;
extern const int sizeResponse;

typedef struct {
    float latitude;
    float longitude;
} position;

typedef struct {
    int seats;
    char *model;
    bool large;
} plane_type;

typedef struct {
    int id;
    char *model;
    float fuel;
    int origin;
    time_t timeTakeoff;
    int destination;
    time_t timeLanding;
    int actual;
    int redirection;
    bool hasBeenRedirected;
    position position;
    float totalDistance;
    float progress;
    int runwayNumber;
    int state;
    int alert;
    bool lateTakeoff;
    bool lateLanding;
    bool large;
} plane_struct;

typedef struct {
    long type;
} planeRequest;

typedef struct {
    long type;
    plane_struct planeInfo;
} planeResponse;

int newDestination(int origine);

void initPlane(plane_struct *info);

void checkIfLate(plane_struct *info);

void decrementFuel(plane_struct *info);

bool move(plane_struct *info);

void sendRequestInfo(int id);

plane_struct getRequestResponse();

void respondInfoRequest(const plane_struct *info);

void asyncSleep(int nsec, plane_struct *info);

void landOrTakeoff(int nsec, plane_struct *info);

int fly(plane_struct *info);

void *plane(void *arg);

#endif //LO41_PLANE_H
