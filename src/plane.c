#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <pthread.h>

#include "main.h"
#include "tower.h"
#include "plane.h"

const plane_type planeType[] = {
        {164, "737-800", false},
        {149, "737-700", false},
        {375, "767-800", true},
        {186, "A320", false},
        {220, "A321", false},
        {868, "A380", true},
};
const char *states[] = {"au hangar",
                        "embarquement",
                        "en roulement",
                        "decole  (attente)",
                        "decole",
                        "en vol",
                        "en vol (priorite)",
                        "atterit (attente)",
                        "atterit",
                        "libere piste",
};
const char *alerts[] = {
        "carburant",
        "technique",
        "crash",
};
const char *sizes[] = {
        "petit",
        "large",
};
const int numberPlaneTypes = sizeof(planeType) / sizeof(plane_type);
const int sizeRequest = sizeof(planeRequest) - sizeof(long);
const int sizeResponse = sizeof(planeResponse);

int newDestination(const int origine) {
    int destination = rand() % (NUMBER_AIRPORT - 1);
    if (destination >= origine) {
        ++destination;
    } return destination;
}

void initPlane(plane_struct *info) {
    int i = rand() % numberPlaneTypes;
    float vector[2];

    info->model = planeType[i].model;
    info->large = planeType[i].large;
    info->origin = rand() % NUMBER_AIRPORT;
    info->destination = newDestination(info->origin);
    info->redirection = NOT_REDIRECTED;
    info->position = airports[info->origin].position;
    getVector(vector, &(airports[info->origin].position), &(airports[info->destination].position));
    info->total_distance = distance(vector);
    info->runwayNumber = NO_RUNWAY;
    info->alert = NONE;
    info->fuel = 100.f;
}

void decrementFuel(plane_struct *info) {
    info->fuel -= info->state == LANDING ? FUEL_CONSUMPTION_RATE / 2: FUEL_CONSUMPTION_RATE;
    if (info->alert == NONE && info->fuel <= CRITICAL_FUEL_LIMIT) {
        info->alert = CRITICAL_FUEL;
        info->state = PRIORITY_IN_FLIGHT;
    } else if (info->fuel <= 0) {
        info->fuel = 0;
        info->alert = CRASHED;
    }
}

bool move(plane_struct *info) {
    float vector[2], d;

    int destination = info->redirection <= NOT_REDIRECTED ? info->destination : info->redirection;

    getVector(vector, &(info->position), &(airports[destination].position));
    d = distance(vector);

    if (d <= SPEED) {
        info->position = airports[destination].position;
        info->progress = 100.f;
        return false;
    } else {
        float normalizedVector[2];
        normalize(normalizedVector, vector, d);
        info->position.latitude += normalizedVector[0];
        info->position.longitude += normalizedVector[1];
        info->progress = (info->total_distance - d) / info->total_distance * 100;
        return true;
    }
}

void sendRequestInfo(const int id) {
    int res;
    planeRequest request = {id};
    do { res = msgsnd(msgid, &request, sizeRequest, 0); } while (res == -1 && errno == EINTR);
}

plane_struct getRequestResponse() {
    int res;
    planeResponse response;
    do { res = msgrcv(msgid, &response, sizeResponse, 1, 0); } while (res == -1 && errno == EINTR);
    return response.planeInfo;
}

void respondInfoRequest(const plane_struct *info) {
    planeRequest request;

    if (msgrcv(msgid, &request, sizeRequest, info->id, IPC_NOWAIT) != -1) {
        planeResponse reponse = {1, *info};
        msgsnd(msgid, &reponse, sizeResponse, 0);
    }
}

void asyncSleep(const int nsec, plane_struct *info) {
    int n = nsec * 1000 / UPDATE_EVERY;
    for (int i = 0; i < n; ++i) {
        respondInfoRequest(info);
        usleep(UPDATE_EVERY * 1000);
    }
}

void landOrTakeoff(int nsec, plane_struct *info) {
    int i = 0, n = nsec * 1000 / UPDATE_EVERY;
    while (i < n && info->alert != CRASHED) {
        decrementFuel(info);
        respondInfoRequest(info);
        usleep(UPDATE_EVERY * 1000);
        ++i;
    }
}

int fly(plane_struct *info) {
    while (info->alert != CRASHED && move(info)) {
        decrementFuel(info);

        if (info->alert == NONE) {
            if (!(rand() % TECHNICAL_PROBLEM_VALUE)) {
                info->alert = TECHNICAL_PROBLEM;
                info->state = PRIORITY_IN_FLIGHT;
            }
        }
        if (info->alert != NONE && info->redirection == NOT_REDIRECTED) {
            int closestAirport;
            float distanceMin = 99999, d;
            float vector[2];

            for (int i = 0; i < NUMBER_AIRPORT; ++i) {
                if (i != info->origin) {
                    getVector(vector, &(info->position), &(airports[i].position));
                    d = distance(vector);
                    if (d < distanceMin) {
                        distanceMin = d;
                        closestAirport = i;
                    }
                }
            }

            if (closestAirport != info->destination) {
                info->redirection = closestAirport;
                info->total_distance = distanceMin;
                info->progress = 0;
            } else {
                info->redirection = MUST_NOT_REDIRECT;
            }
        }

        respondInfoRequest(info);
        usleep(UPDATE_EVERY * 1000);
    }
}

void *plane(void *arg) {
    pthread_cleanup_push(cleanupHandler, NULL);

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTSTP);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    float vector[2];
    plane_struct info;
    info.id = *((int *) arg);
    initPlane(&info);

    while (info.alert != CRASHED) {
        info.actual = info.origin;

        if (info.redirection <= NOT_REDIRECTED) {
            info.state = EMBARKMENT;
            asyncSleep(EMBARKMENT_DURATION, &info);
        } else {
            info.redirection = NOT_REDIRECTED;
        }

        info.state = ROLLING;
        asyncSleep(ROLLING_DURATION, &info);

        info.state = WAITING_TAKEOFF;
        requestTakeoff(&info);

        info.state = TAKEOFF;
        landOrTakeoff(TAKEOFF_DURATION, &info);

        freeRunway(&info);

        info.state = FLYING;
        fly(&info);

        if (info.alert != CRASHED) {
            info.actual = info.redirection <= NOT_REDIRECTED ? info.destination : info.redirection;
            requestLanding(&info);

            info.state = LANDING;
            landOrTakeoff(LANDING_DURATION, &info);

            if (info.alert != CRASHED) {
                info.state = FREEING_RUNWAY;
                freeRunway(&info);

                info.state = ROLLING;
                asyncSleep(ROLLING_DURATION, &info);
                info.progress = 0;

                info.state = HANGAR;
                asyncSleep(REFUEL_DURATION, &info);

                info.alert = NONE;
                info.fuel = 100.f;
                switch (info.redirection) {
                    case MUST_NOT_REDIRECT:
                        info.redirection = NOT_REDIRECTED;
                    case NOT_REDIRECTED:
                        info.origin = info.destination;
                        info.destination = newDestination(info.origin);
                        break;
                    default:
                        info.origin = info.redirection;
                }
                getVector(vector, &(airports[info.origin].position), &(airports[info.destination].position));
                info.total_distance = distance(vector);
            }
        }
    }

    while (true) {
        respondInfoRequest(&info);
        usleep(UPDATE_EVERY * 1000);
    }

    pthread_cleanup_pop(1);
}
