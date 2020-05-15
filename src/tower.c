#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <pthread.h>

#include "main.h"
#include "tower.h"

#define PRIORITIZED_LARGE_PLANE_LANDING 0
#define PRIORITIZED_SMALL_PLANE_LANDING 1
#define LARGE_PLANE_LANDING 2
#define SMALL_PLANE_LANDING 3
#define LARGE_PLANE_TAKEOFF 4
#define SMALL_PLANE_TAKEOFF 5

#define NUMBER_SOLICITATION_TYPES 6

const char *airportNames[] = {
        "Paris",
        "Marseille",
        "Lyon",
        "Toulouse",
        "Nice",
        "Nantes",
        "Montpellier",
        "Strasbourg",
        "Bordeaux",
        "Lille",
        "Rennes",
        "Reims",
        "Saint-Etienne",
        "Toulon",
        "Le Havre",
        "Grenoble",
        "Dijon",
        "Angers",
        "Nimes",
        "Saint-Denis"
};

int numberPlanesWaiting[NUMBERAIRPORT][NUMBER_SOLICITATION_TYPES] = {[0 ... (NUMBERAIRPORT - 1)] = {}};
pthread_cond_t solicitations[NUMBERAIRPORT][NUMBER_SOLICITATION_TYPES];

int largeRunwayFree[NUMBERAIRPORT] = {[0 ... (NUMBERAIRPORT - 1)] = 1};
int smallRunwayFree[NUMBERAIRPORT] = {[0 ... (NUMBERAIRPORT - 1)] = 1};

pthread_mutex_t mutex[NUMBERAIRPORT];

void initRunways() {
    for (int i = 0; i < NUMBERAIRPORT; ++i) {
        pthread_mutex_init(&(mutex[i]), 0);

        for (int j = 0; j < NUMBER_SOLICITATION_TYPES; ++j) {
            pthread_cond_init(&(solicitations[i][j]), 0);
        }
    }
}

void deleteRunways() {
    for (int i = 0; i < NUMBERAIRPORT; ++i) {
        printf("Destruction aéroport %i / %i\r", i + 1, NUMBERAIRPORT);
        fflush(stdout);
        for (int j = 0; j < NUMBER_SOLICITATION_TYPES; ++j) {
            if (pthread_cond_destroy(&(solicitations[i][j])) != 0) {
                perror("Problème destruction cond");
            }
        }
        pthread_mutex_destroy(&(mutex[i]));
    }
    printf("\n");
}

void cleanupHandler(void *arg) {
    for (int i = 0; i < NUMBERAIRPORT; ++i) {
        pthread_mutex_unlock(&(mutex[i]));
    }
}

void incrementTime(struct timespec *ts) {
    ts->tv_nsec += (RESPONDEVERY + (rand() % 50)) * 1000 * 1000;
    if (ts->tv_nsec >= 1000 * 1000 * 1000) {
        ts->tv_nsec %= 1000 * 1000 * 1000;
        ts->tv_sec += 1;
    }
}

void requestLanding(plane_struct *info){
    pthread_mutex_lock(&(mutex[info->destination]));

    int res;
    const char* size = sizes[info->large];
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    int solicitation = info->large ? LARGE_PLANE_LANDING : SMALL_PLANE_LANDING;
    int* preferedRunwayFree = info->large ? &(largeRunwayFree[info->destination]) : &(smallRunwayFree[info->destination]);

    if (logging) {
        printf("Demande d'atterrissage %s avion %03i à %s\n",
                size, info->id, airportNames[info->destination]);
        fflush(stdout);
    }

    ++(numberPlanesWaiting[info->destination][solicitation]);
    while (!(*preferedRunwayFree) && !largeRunwayFree[info->destination] && info->condition == 0) {
        if (logging) {
            printf("Avion %03i attend %s piste de %s pour atterrir\n",
                    info->id, info->large ? "large" : "une", airportNames[info->destination]);
            fflush(stdout);
        }

        info->state = 5;
        decrementFuel(info);
        do {
            respondInfoRequest(info);
            incrementTime(&ts);

            res = pthread_cond_timedwait(&solicitations[info->destination][solicitation], &mutex[info->destination], &ts);

            decrementFuel(info);
        } while (res == ETIMEDOUT && info->condition == 0);
    }
    --(numberPlanesWaiting[info->destination][solicitation]);

    if (info->fuel <= CRITICALFUEL) {
        info->condition = 1;
        info->state = 6;
    }

    if (!(*preferedRunwayFree) && !largeRunwayFree[info->destination] && info->condition != 0) {
        solicitation = info->large ? PRIORITIZED_LARGE_PLANE_LANDING : PRIORITIZED_SMALL_PLANE_LANDING;

        ++(numberPlanesWaiting[info->destination][solicitation]);
        do {
            if (logging) {
                printf("Avion %03i attend %s piste de %s pour atterrir d'urgence\n",
                        info->id, info->large ? "large" : "une", airportNames[info->destination]);
                fflush(stdout);
            }

            decrementFuel(info);
            do {
                respondInfoRequest(info);
                incrementTime(&ts);

                res = pthread_cond_timedwait(&solicitations[info->destination][solicitation], &mutex[info->destination], &ts);

                decrementFuel(info);
            } while (res == ETIMEDOUT);
        } while (!(*preferedRunwayFree) && !largeRunwayFree[info->destination]);
        --(numberPlanesWaiting[info->destination][solicitation]);
    }

    if (*preferedRunwayFree) {
        *preferedRunwayFree = 0;
        info->runwayNumber = preferedRunwayFree == &(smallRunwayFree[info->destination]) ? 0 : 1;
    } else {
        largeRunwayFree[info->destination] = 0;
        info->runwayNumber = 1;
    }
    if (logging) {
        printf("Avion %03i atterit sur %s piste de %s\n",
                info->id, sizes[info->runwayNumber], airportNames[info->destination]);
        fflush(stdout);
    }

    pthread_mutex_unlock(&(mutex[info->destination]));
}

void freeRunway(plane_struct *info) {
    pthread_mutex_lock(&(mutex[info->actual]));

    const char* runwaySize = sizes[info->runwayNumber];
    int i = info->runwayNumber == 0 ? 1 : 0;
    int increment = info->runwayNumber == 0 ? 2 : 1;
    int* runwayFree = info->runwayNumber == 0 ? &(smallRunwayFree[info->actual]) : &(largeRunwayFree[info->actual]);

    if (logging) {
        printf("Le %s avion %03i libère %s piste de %s\n",
                sizes[info->large], info->id, runwaySize, airportNames[info->actual]);
        fflush(stdout);
    }
    *runwayFree = 1;
    info->runwayNumber = -1;

    while (i < NUMBER_SOLICITATION_TYPES && !numberPlanesWaiting[info->actual][i]) {
        i += increment;
    }
    if (logging) {
        switch (i) {
            case PRIORITIZED_LARGE_PLANE_LANDING:
                printf("Donne la %s piste de %s à un gros avion prioritaire pour atterir\n",
                        runwaySize, airportNames[info->actual]);
                break;
            case PRIORITIZED_SMALL_PLANE_LANDING:
                printf("Donne la %s piste de %s à un petit avion prioritaire pour atterir\n",
                        runwaySize, airportNames[info->actual]);
                break;
            case LARGE_PLANE_LANDING:
                printf("Donne la %s piste de %s à un gros avion pour atterir\n",
                        runwaySize, airportNames[info->actual]);
                break;
            case SMALL_PLANE_LANDING:
                printf("Donne la %s piste de %s à un petit avion pour atterir\n",
                        runwaySize, airportNames[info->actual]);
                break;
            case LARGE_PLANE_TAKEOFF:
                printf("Donne la %s piste de %s à un gros avion pour décoler\n",
                        runwaySize, airportNames[info->actual]);
                break;
            case SMALL_PLANE_TAKEOFF:
                printf("Donne la %s piste de %s à un petit avion pour décoler\n",
                        runwaySize, airportNames[info->actual]);
                break;
            default:
                break;
        }
        fflush(stdout);
    }

    pthread_cond_signal(&(solicitations[info->actual][i]));
    pthread_mutex_unlock(&(mutex[info->actual]));
}

void requestTakeoff(plane_struct *info){
    pthread_mutex_lock(&(mutex[info->depart]));

    int res;
    const char* size = sizes[info->large];
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    int solicitation = info->large ? LARGE_PLANE_TAKEOFF : SMALL_PLANE_TAKEOFF;
    int* preferedRunwayFree = info->large ? &(largeRunwayFree[info->depart]) : &(smallRunwayFree[info->depart]);

    if (logging) {
        printf("Demande de décolage %s avion %03i à %s\n",
                size, info->id, airportNames[info->depart]);
        fflush(stdout);
    }

    ++(numberPlanesWaiting[info->depart][solicitation]);
    while (!(*preferedRunwayFree) && !largeRunwayFree[info->depart]){
        if (logging) {
            printf("Avion %03i attend %s piste de %s pour décoler\n",
                    info->id, info->large ? "large" : "une", airportNames[info->depart]);
            fflush(stdout);
        }

        do {
            respondInfoRequest(info);
            incrementTime(&ts);

            res = pthread_cond_timedwait(&(solicitations[info->depart][solicitation]), &(mutex[info->depart]), &ts);
        } while (res == ETIMEDOUT);
    }
    --(numberPlanesWaiting[info->depart][solicitation]);

    if (*preferedRunwayFree) {
        *preferedRunwayFree = 0;
        info->runwayNumber = preferedRunwayFree == &(smallRunwayFree[info->depart]) ? 0 : 1;
    } else {
        largeRunwayFree[info->depart] = 0;
        info->runwayNumber = 1;
    }
    if (logging) {
        printf("Avion %03i décole sur %s piste de %s\n",
                info->id, sizes[info->runwayNumber], airportNames[info->depart]);
        fflush(stdout);
    }

    pthread_mutex_unlock(&(mutex[info->depart]));
}
