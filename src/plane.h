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

extern const char *sizes[];
extern const char *states[];
extern const char *conditions[];
extern const int sizeRequest;
extern const int sizeResponse;

typedef struct {
    int seats;
    char *model;
    unsigned int large;
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
    unsigned int large;
} plane_struct;

typedef struct {
    long type;
    int pidSender;
    int kill;
} planeRequest;

typedef struct {
    long type;
    plane_struct planeInfo;
} planeResponse;

void initPlane(plane_struct *);

void decrementFuel(plane_struct *);

void sendRequestInfo(const plane_struct *);

plane_struct getRequestResponse();

void respondInfoRequest(const plane_struct *);

void asyncSleep(int, plane_struct *);

void *plane(void *);

#endif //LO41_PLANE_H
