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
        "Saint-Denis",
};

int numberPlanesWaiting[NUMBER_AIRPORT][NUMBER_SOLICITATION_TYPES] = {[0 ... (NUMBER_AIRPORT - 1)] = {}};
pthread_cond_t solicitations[NUMBER_AIRPORT][NUMBER_SOLICITATION_TYPES];

int largeRunwayFree[NUMBER_AIRPORT] = {[0 ... (NUMBER_AIRPORT - 1)] = 1};
int smallRunwayFree[NUMBER_AIRPORT] = {[0 ... (NUMBER_AIRPORT - 1)] = 1};

pthread_mutex_t mutex[NUMBER_AIRPORT];

void initRunways() {
    for (int i = 0; i < NUMBER_AIRPORT; ++i) {
        pthread_mutex_init(&(mutex[i]), 0);

        for (int j = 0; j < NUMBER_SOLICITATION_TYPES; ++j) {
            pthread_cond_init(&(solicitations[i][j]), 0);
        }
    }
}

void deleteRunways() {
    for (int i = 0; i < NUMBER_AIRPORT; ++i) {
        printf("Destruction aéroport %i / %i\r", i + 1, NUMBER_AIRPORT);
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
    for (int i = 0; i < NUMBER_AIRPORT; ++i) {
        pthread_mutex_unlock(&(mutex[i]));
    }
}

void incrementTime(struct timespec *ts) {
    ts->tv_nsec += (RESPOND_EVERY + (rand() % 50)) * 1000 * 1000;
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
        printf("%s: Demande d'atterrissage %s avion %03i\n",
                airportNames[info->destination], size, info->id);
        fflush(stdout);
    }

    ++(numberPlanesWaiting[info->destination][solicitation]);
    while (!(*preferedRunwayFree) && !largeRunwayFree[info->destination] && info->condition == NORMAL) {
        if (logging) {
            printf("%s: Avion %03i attend %s piste pour atterrir\n",
                   airportNames[info->destination], info->id, info->large ? "large" : "une");
            fflush(stdout);
        }

        info->state = WAITING_IN_FLIGHT;
        decrementFuel(info);
        do {
            respondInfoRequest(info);
            incrementTime(&ts);

            res = pthread_cond_timedwait(&solicitations[info->destination][solicitation], &mutex[info->destination], &ts);

            decrementFuel(info);
        } while (res == ETIMEDOUT && info->condition == NORMAL);
    }
    --(numberPlanesWaiting[info->destination][solicitation]);

    if (!(*preferedRunwayFree) && !largeRunwayFree[info->destination] && info->condition != NORMAL) {
        solicitation = info->large ? PRIORITIZED_LARGE_PLANE_LANDING : PRIORITIZED_SMALL_PLANE_LANDING;

        ++(numberPlanesWaiting[info->destination][solicitation]);
        do {
            if (logging) {
                printf("%s: Avion %03i attend %s piste pour atterrir d'urgence\n",
                       airportNames[info->destination], info->id, info->large ? "large" : "une");
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
        printf("%s: Avion %03i atterit sur %s piste\n",
               airportNames[info->destination], info->id, sizes[info->runwayNumber]);
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
        printf("%s: Le %s avion %03i libère %s piste\n",
               airportNames[info->actual], sizes[info->large], info->id, runwaySize);
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
                printf("%s: Donne la %s piste à un gros avion prioritaire pour atterir\n",
                        airportNames[info->actual], runwaySize);
                break;
            case PRIORITIZED_SMALL_PLANE_LANDING:
                printf("%s: Donne la %s piste à un petit avion prioritaire pour atterir\n",
                        airportNames[info->actual], runwaySize);
                break;
            case LARGE_PLANE_LANDING:
                printf("%s: Donne la %s piste à un gros avion pour atterir\n",
                        airportNames[info->actual], runwaySize);
                break;
            case SMALL_PLANE_LANDING:
                printf("%s: Donne la %s piste à un petit avion pour atterir\n",
                        airportNames[info->actual], runwaySize);
                break;
            case LARGE_PLANE_TAKEOFF:
                printf("%s: Donne la %s piste à un gros avion pour décoler\n",
                        airportNames[info->actual], runwaySize);
                break;
            case SMALL_PLANE_TAKEOFF:
                printf("%s: Donne la %s piste à un petit avion pour décoler\n",
                        airportNames[info->actual], runwaySize);
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
        printf("%s: Demande de décolage %s avion %03i\n",
               airportNames[info->depart], size, info->id);
        fflush(stdout);
    }

    ++(numberPlanesWaiting[info->depart][solicitation]);
    while (!(*preferedRunwayFree) && !largeRunwayFree[info->depart]){
        if (logging) {
            printf("%s: Avion %03i attend %s piste pour décoler\n",
                   airportNames[info->depart], info->id, info->large ? "large" : "une");
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
        printf("%s: Avion %03i décole sur %s piste\n",
               airportNames[info->depart], info->id, sizes[info->runwayNumber]);
        fflush(stdout);
    }

    pthread_mutex_unlock(&(mutex[info->depart]));
}
