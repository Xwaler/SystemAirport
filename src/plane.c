#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <pthread.h>

#include "main.h"
#include "tower.h"
#include "plane.h"

const plane_type planeType[] = {
        {164, "Boeing 737-800", false},
        {149, "Boeing 737-700", false},
        {375, "Boeing 767-800", true},
        {186, "Airbus A320", false},
        {220, "Airbus A321", true},
};
const char *states[] = {"au hangar",
                        "embarquement",
                        "en roulement",
                        "decole (attente)",
                        "decole",
                        "atterit",
                        "en vol",
                        "atterit (attente)",
                        "en vol (priorite)",
                        "libere piste",
};
const char *alerts[] = {
        "carburant",
        "technique",
};
const char *sizes[] = {
        "petit(e)",
        "large",
};
const int numberPlaneTypes = sizeof(planeType) / sizeof(plane_type);
const int sizeRequest = sizeof(planeRequest) - sizeof(long);
const int sizeResponse = sizeof(planeResponse);

int newDestination(const int depart) {
    int destination = rand() % (NUMBER_AIRPORT - 1);
    if (destination >= depart) {
        ++destination;
    } return destination;
}

void initPlane(plane_struct *info) {
    int i = rand() % numberPlaneTypes;
    float vector[2];

    info->model = planeType[i].model;
    info->large = planeType[i].large;
    info->start = rand() % NUMBER_AIRPORT;
    info->destination = newDestination(info->start);
    info->redirection = NOT_REDIRECTED;
    info->latitude = (float) airportPostitions[info->start][LATITUDE];
    info->longitude = (float) airportPostitions[info->start][LONGITUDE];
    vectorAirport(vector, airportPostitions[info->start], airportPostitions[info->destination]);
    info->total_distance = distance(vector);
    info->runwayNumber = NO_RUNWAY;
    info->alert = NONE;
    info->fuel = 100;
}

void decrementFuel(plane_struct *info) {
    info->fuel -= FUEL_CONSUMPTION_RATE;
    if (info->fuel <= CRITICAL_FUEL_LIMIT && info->alert == NONE) {
        info->alert = CRITICAL_FUEL;
        info->state = LANDING;
    }
}

bool move(plane_struct *info) {
    float vector[2], d;

    int destination = info->redirection <= NOT_REDIRECTED ? info->destination : info->redirection;

    vectorPlane(vector, info->latitude, info->longitude, airportPostitions[destination]);
    d = distance(vector);

    if (d <= SPEED) {
        info->latitude = (float) airportPostitions[destination][LATITUDE];
        info->longitude = (float) airportPostitions[destination][LONGITUDE];
        info->progress = 100.f;
        return true;
    } else {
        float normalizedVector[2];
        normalize(normalizedVector, vector, d);
        info->latitude += normalizedVector[0];
        info->longitude += normalizedVector[1];
        info->progress = (info->total_distance - d) / info->total_distance * 100;
        return false;
    }
}

void sendRequestInfo(const int id) {
    planeRequest request = {id};
    msgsnd(msgid, &request, sizeRequest, 0);
}

plane_struct getRequestResponse() {
    planeResponse response;
    msgrcv(msgid, &response, sizeResponse, 1, 0);
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

void land(plane_struct *info) {
    int n = LANDING_DURATION * 1000 / UPDATE_EVERY;
    for (int i = 0; i < n; ++i) {
        decrementFuel(info);
        respondInfoRequest(info);
        usleep(UPDATE_EVERY * 1000);
    }
}

int fly(plane_struct *info) {
    while (move(info) == 0) {
        decrementFuel(info);

        if (info->alert == NONE) {
            if (!(rand() % TECHNICAL_PROBLEM_VALUE)) {
                info->alert = TECHNICAL_PROBLEM;
                info->state = LANDING;
            }
        }
        if (info->alert != NONE && info->redirection == NOT_REDIRECTED) {
            int closestAirport;
            float distanceMin = 99999, d;
            float vector[2];

            for (int i = 0; i < NUMBER_AIRPORT; ++i) {
                if (i != info->start) {
                    vectorPlane(vector, info->latitude, info->longitude, airportPostitions[i]);
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

    while (true) {
        info.actual = info.start;

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
        asyncSleep(TAKEOFF_DURATION, &info);

        freeRunway(&info);

        info.state = PRIORITY_IN_FLIGHT;
        fly(&info);

        info.actual = info.redirection <= NOT_REDIRECTED ? info.destination : info.redirection;
        requestLanding(&info);

        info.state = FLYING;
        land(&info);

        info.state = FREEING_RUNWAY;
        freeRunway(&info);

        info.progress = 0;
        info.state = ROLLING;
        asyncSleep(ROLLING_DURATION, &info);

        info.state = HANGAR;
        asyncSleep(REFUEL_DURATION, &info);

        info.alert = NONE;
        info.fuel = 100.f;
        switch (info.redirection) {
            case MUST_NOT_REDIRECT:
                info.redirection = NOT_REDIRECTED;
            case NOT_REDIRECTED:
                info.start = info.destination;
                info.destination = newDestination(info.start);
                break;
            default:
                info.start = info.redirection;
        }
        vectorAirport(vector, airportPostitions[info.start], airportPostitions[info.destination]);
        info.total_distance = distance(vector);
    }

    pthread_cleanup_pop(1);
}
