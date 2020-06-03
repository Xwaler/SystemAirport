#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <pthread.h>

#include "main.h"
#include "tower.h"
#include "plane.h"

const plane_type planeType[] = {
        {"737-800", false},
        {"737-700", false},
        {"767-800", true},
        {"A320",    false},
        {"A321",    false},
        {"A380",    true},
};
const char *states[] = {"desembarquement",
                        "au hangar",
                        "embarquement",
                        "en roulement",
                        "\033[0;33mdecole  (attente)\033[0;37m",
                        "\033[0;31mdecole (priorite)\033[0;37m",
                        "decole",
                        "en vol",
                        "\033[0;31men vol (priorite)\033[0;37m",
                        "\033[0;33men vol  (attente)\033[0;37m",
                        "atterit",
};
const char *alerts[] = {
        "\033[0;31mcarburant\033[0;37m",
        "\033[0;31mtechnique\033[0;37m",
        "\033[0;31mcrash\033[0;37m",
};
const char *sizes[] = {
        "petit",
        "large",
};
const int numberPlaneTypes = sizeof(planeType) / sizeof(plane_type);
const int sizeRequest = sizeof(planeRequest) - sizeof(long);
const int sizeResponse = sizeof(planeResponse);

int newDestination(const unsigned int origine) {
    int destination = rand() % (NUMBER_AIRPORT - 1);
    if (destination >= origine) {
        ++destination;
    }
    return destination;
}

void initPlane(plane_struct *info) {
    int i = rand() % numberPlaneTypes;
    float vector[2];

    info->model = planeType[i].model;
    info->large = planeType[i].large;
    info->origin = rand() % NUMBER_AIRPORT;
    info->destination = newDestination(info->origin);
    info->timeLanding = 0;
    info->timeTakeoff = 0;
    info->redirection = NOT_REDIRECTED;
    info->position = airports[info->origin].position;
    getVector(vector, &(airports[info->origin].position), &(airports[info->destination].position));
    info->totalDistance = distance(vector);
    info->runwayNumber = NO_RUNWAY;
    info->state = HANGAR;
    info->alert = NONE;
    info->lateTakeoff = false;
    info->lateLanding = false;
    info->landed = false;
    info->tookoff = false;
    info->fuel = 100.f;
}

void checkIfLate(plane_struct *info) {
    time_t now;
    time(&now);
    if (!info->tookoff && !info->lateTakeoff && now > info->timeTakeoff) {
        info->lateTakeoff = true;
        if (info->state == WAITING_TAKEOFF) info->state = PRIORITY_TAKEOFF;
    } else if (!info->lateLanding && info->actual != info->destination && now > info->timeLanding) {
        info->lateLanding = true;
        if (info->state == FLYING || info->state == WAITING_LANDING) info->state = PRIORITY_LANDING;
    }
}

void decrementFuel(plane_struct *info) {
    info->fuel -= info->state == LANDING ? FUEL_CONSUMPTION_RATE / 2 : FUEL_CONSUMPTION_RATE;
    if (info->alert == NONE && info->fuel <= CRITICAL_FUEL_LIMIT) {
        info->alert = CRITICAL_FUEL;
        info->state = PRIORITY_LANDING;
    } else if (info->fuel <= 0) {
        info->fuel = 0;
        info->alert = CRASHED;
    }
    checkIfLate(info);
}

bool move(plane_struct *info) {
    float vector[2], d;

    unsigned int destination = info->redirection <= NOT_REDIRECTED ? info->destination : info->redirection;

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
        info->progress = (info->totalDistance - d) / info->totalDistance * 100;
        return true;
    }
}

void sendRequestInfo(const unsigned int id) {
    int res;
    planeRequest request = {(long) id};
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

void asyncSleep(const unsigned int nsec, plane_struct *info) {
    unsigned int n = nsec * 1000 / UPDATE_EVERY;
    for (int i = 0; i < n; ++i) {
        respondInfoRequest(info);
        usleep(UPDATE_EVERY * 1000);
    }
}

void landOrTakeoff(const unsigned int nsec, plane_struct *info) {
    unsigned int i = 0, n = nsec * 1000 / UPDATE_EVERY;
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
                info->state = PRIORITY_LANDING;
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
                info->totalDistance = distanceMin;
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
        time_t now;
        info.id = *((int *) arg);

        initPlane(&info);
        while (info.alert != CRASHED) {
            info.actual = info.origin;

            if (info.redirection <= NOT_REDIRECTED) {
                info.timeTakeoff =
                        time(NULL) + EMBARKMENT_DURATION + ROLLING_DURATION + TAKEOFF_DURATION + ACCEPTABLE_LATENCY;
                info.timeLanding =
                        info.timeTakeoff + ((info.totalDistance / SPEED) * (UPDATE_EVERY / 1000.)) + LANDING_DURATION +
                        ACCEPTABLE_LATENCY;
                info.lateTakeoff = false;
                info.lateLanding = false;
                info.tookoff = false;
                info.landed = false;
                info.hasBeenRedirected = false;

                info.state = EMBARKMENT;
                asyncSleep(EMBARKMENT_DURATION, &info);
            } else {
                info.redirection = NOT_REDIRECTED;
                info.hasBeenRedirected = true;
            }

            info.state = ROLLING;
            asyncSleep(ROLLING_DURATION, &info);

            requestTakeoff(&info);

            info.state = TAKEOFF;
            landOrTakeoff(TAKEOFF_DURATION, &info);

            freeRunway(&info);

            info.state = FLYING;
            info.tookoff = true;
            fly(&info);

            if (info.alert != CRASHED) {
                info.actual = info.redirection <= NOT_REDIRECTED ? info.destination : info.redirection;
                requestLanding(&info);

                info.state = LANDING;
                landOrTakeoff(LANDING_DURATION, &info);

                if (info.actual == info.destination) info.landed = true;
                freeRunway(&info);

                if (info.alert != CRASHED) {
                    info.state = ROLLING;
                    asyncSleep(ROLLING_DURATION, &info);
                    info.progress = 0;

                    if (info.redirection <= NOT_REDIRECTED) {
                        info.state = DISBARKEMENT;
                        asyncSleep(DISBARKMENT_DURATION, &info);
                    }

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
                    info.totalDistance = distance(vector);
                }
            }
        }

        while (true) {
            respondInfoRequest(&info);
            usleep(UPDATE_EVERY * 1000);
        }

    pthread_cleanup_pop(1);
}
