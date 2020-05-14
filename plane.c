#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <pthread.h>

#include "main.h"
#include "tower.h"

const plane_type planeType[] = {
        {164,  "Boeing 737-800", 0},
        {149,  "Boeing 737-700", 0},
        {375, "Boeing 767-800", 1},
        {186, "A320", 0},
        {220, "A321", 1},
};
const char *sizes[5] = {"petit(e)", "gros(se)"};
const int numberPlaneTypes = sizeof(planeType) / sizeof(plane_type);
const int sizeRequest = sizeof(planeRequest) - sizeof(long);
const int sizeResponse = sizeof(planeResponse);
const int checkRequestEveryms = 100;

void initPlane(plane_struct *info) {
    int i = rand() % numberPlaneTypes;
    info->seats = planeType[i].seats;
    info->model = planeType[i].model;
    info->fuel = 100;
    info->runwayNumber = -1;
    info->large = planeType[i].large;
}

void getInfoPlane(plane_struct *planeInfo) {
    planeRequest request = {planeInfo->id, getpid(), 0};
    planeResponse response;

    msgsnd(msgid, &request, sizeRequest, 0);
    msgrcv(msgid, &response, sizeResponse, getpid(), 0);
    *planeInfo = response.planeInfo;
}

void respondInfoRequest(plane_struct *info) {
    planeRequest request;

    if (msgrcv(msgid, &request, sizeRequest, info->id, IPC_NOWAIT) != -1) {
        planeResponse reponse = {request.pidSender, *info};
        msgsnd(msgid, &reponse, sizeResponse, 0);
    }
}

void asyncSleep(int nsec, plane_struct *info) {
    int n = nsec * 1000 / checkRequestEveryms;
    for (int i = 0; i < n; ++i) {
        usleep(checkRequestEveryms * 1000);
        respondInfoRequest(info);
    }
}

void *plane(void *arg) {
    pthread_cleanup_push(cleanupHandler, NULL);

    int id = *((int *) arg);
    srand((getpid() + (id * id)) * id);

    plane_struct info;
    info.id = id;
    initPlane(&info);

    sleep(1);
    while (1) {
        requestTakeoff(&info);
        asyncSleep(1, &info);

        freeRunway(&info);
        asyncSleep(2, &info);

        requestLanding(&info);
        asyncSleep(1, &info);

        freeRunway(&info);
        asyncSleep(2, &info);
        info.fuel = 100;

        asyncSleep(5, &info);
    }

    pthread_cleanup_pop(1);
}
