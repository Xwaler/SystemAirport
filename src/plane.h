#ifndef LO41_PLANE_H
#define LO41_PLANE_H

#include "main.h"

#define CRITICAL_FUEL_LIMIT 25
#define UPDATE_EVERY 100
#define FUEL_CONSUMPTION_RATE .33f
#define SPEED 5
#define TECHNICAL_PROBLEM_VALUE 3000

#define REFUEL_DURATION 2
#define EMBARKMENT_DURATION 2
#define TAKEOFF_DURATION 2
#define ROLLING_DURATION 1
#define LANDING_DURATION 1

#define HANGAR 0
#define EMBARKMENT 1
#define ROLLING 2
#define WAITING_TAKEOFF 3
#define TAKEOFF 4
#define FLYING 5
#define PRIORITY_IN_FLIGHT 6
#define WAITING_IN_FLIGHT 7
#define LANDING 8
#define FREEING_RUNWAY 9

#define NOT_REDIRECTED -1
#define MUST_NOT_REDIRECT -2

#define NONE -1
#define CRITICAL_FUEL 0
#define TECHNICAL_PROBLEM 1

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
    int start;
    int destination;
    int actual;
    int redirection;
    position position;
    float total_distance;
    float progress;
    int runwayNumber;
    int state;
    int alert;
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

void land(plane_struct *info);

int fly(plane_struct *info);

void *plane(void *arg);

#endif //LO41_PLANE_H
