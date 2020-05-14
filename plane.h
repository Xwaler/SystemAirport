#ifndef LO41_PLANE_H
#define LO41_PLANE_H

#define PLANENUMBER 30
#define BUFFER 100
#define CRITICALFUEL 30
#define RESPONDEVERY 100

extern const char *sizes[];
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
    char *model;
    int fuel;
    int runwayNumber;
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

void getInfoPlane(plane_struct *);

void respondInfoRequest(plane_struct *);

void asyncSleep(int, plane_struct *);

void *plane(void *);

#endif //LO41_PLANE_H
