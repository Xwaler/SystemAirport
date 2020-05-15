#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <pthread.h>

#include "main.h"
#include "tower.h"
#include "plane.h"

const plane_type planeType[] = {
        {164, "Boeing 737-800", 0},
        {149, "Boeing 737-700", 0},
        {375, "Boeing 767-800", 1},
        {186, "Airbus A320   ", 0},
        {220, "Airbus A321   ", 1},
};
const char *states[] = {"au hangar       ",
                        "en roulement    ",
                        "décoloage       ",
                        "atterissage     ",
                        "en vol          ",
                        "en vol (attente)",
                        "en vol (priorit)",
};
const char *conditions[] = {
        "normale",
        "carburant critique",
        "problème technique",
};
const char *sizes[] = {
        "petit(e)",
        "large",
};
const int numberPlaneTypes = sizeof(planeType) / sizeof(plane_type);
const int sizeRequest = sizeof(planeRequest) - sizeof(long);
const int sizeResponse = sizeof(planeResponse);

int newDestination(int depart) {
    int destination;
    do {
        destination = rand() % NUMBERAIRPORT;
    } while (destination == depart);
    return destination;
}

void initPlane(plane_struct *info) {
    int i = rand() % numberPlaneTypes;
    info->seats = planeType[i].seats;
    info->model = planeType[i].model;
    info->depart = rand() % NUMBERAIRPORT;
    info->destination = newDestination(info->depart);
    info->runwayNumber = -1;
    info->large = planeType[i].large;
    info->condition = 0;
}

void decrementFuel(plane_struct *info) {
    --(info->fuel);
}

void sendRequestInfo(plane_struct *info) {
    planeRequest request = {info->id, getpid(), 0};
    msgsnd(msgid, &request, sizeRequest, 0);
}

plane_struct getRequestResponse(plane_struct *info) {
    planeResponse response;
    msgrcv(msgid, &response, sizeResponse, getpid(), 0);
    return response.planeInfo;
}

void respondInfoRequest(plane_struct *info) {
    planeRequest request;

    if (msgrcv(msgid, &request, sizeRequest, info->id, IPC_NOWAIT) != -1) {
        planeResponse reponse = {request.pidSender, *info};
        msgsnd(msgid, &reponse, sizeResponse, 0);
    }
}

void asyncSleep(int nsec, plane_struct *info) {
    int n = nsec * 1000 / RESPONDEVERY;
    for (int i = 0; i < n; ++i) {
        usleep(RESPONDEVERY * 1000);

        if (info->state >= 4) {
            decrementFuel(info);

            if (info->condition == 0) {
                if (info->fuel <= CRITICALFUEL) {
                    info->condition = 1;
                    info->state = 6;
                } else if ((rand() % 1000) == 0) {
                    info->condition = 2;
                    info->state = 6;
                }
            }
        }
        respondInfoRequest(info);
    }
}

void *plane(void *arg) {
    pthread_cleanup_push(cleanupHandler, NULL);

    int id = *((int *) arg);
    plane_struct info;
    info.id = id;
    initPlane(&info);

    sleep(1);
    while (1) {
        info.actual = info.depart;
        info.state = 0;
        info.fuel = 100;
        info.condition = 0;
        info.passengers = (int) (.7 * info.seats) + rand() % ((int) (.3 * info.seats) + 1);

        requestTakeoff(&info);
        info.state = 1;
        asyncSleep(1, &info);

        info.state = 2;
        asyncSleep(1, &info);
        freeRunway(&info);

        info.state = 4;
        asyncSleep((rand() % 6) + 5, &info);

        requestLanding(&info);
        info.state = 3;
        asyncSleep(2, &info);

        info.actual = info.destination;
        info.state = 1;
        freeRunway(&info);
        asyncSleep(1, &info);

        info.depart = info.destination;
        info.destination = newDestination(info.depart);
    }

    pthread_cleanup_pop(1);
}
