#ifndef LO41_PLANE_H
#define LO41_PLANE_H

#define PLANENUMBER 35
#define BUFFER 200
#define CRITICALFUEL 30
#define RESPONDEVERY 250

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
    int seats;
    int passengers;
    char *model;
    int fuel;
    int depart;
    int destination;
    int actual;
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

void sendRequestInfo(plane_struct *);

plane_struct getRequestResponse();

void respondInfoRequest(plane_struct *);

void asyncSleep(int, plane_struct *);

void *plane(void *);

#endif //LO41_PLANE_H
