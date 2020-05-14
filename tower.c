#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include "pthread.h"

#include "tower.h"

#define PRIORITIZED_LARGE_PLANE_LANDING 0
#define PRIORITIZED_SMALL_PLANE_LANDING 1
#define LARGE_PLANE_LANDING 2
#define SMALL_PLANE_LANDING 3
#define LARGE_PLANE_TAKEOFF 4
#define SMALL_PLANE_TAKEOFF 5

#define NUMBER_SOLICITATION_TYPES 6

int numberPlanesWaiting[NUMBER_SOLICITATION_TYPES] = {0};
pthread_cond_t conditions[NUMBER_SOLICITATION_TYPES];

int largeRunwayFree = 1;
int smallRunwayFree = 1;

pthread_mutex_t mutex;

void initRunways() {
    pthread_mutex_init(&mutex,0);

    for (int i = 0; i < NUMBER_SOLICITATION_TYPES; ++i) {
        pthread_cond_init(&conditions[i], 0);
    }
}

void deleteRunways() {
    for (int i = 0; i < NUMBER_SOLICITATION_TYPES; ++i) {
        printf("Destruction cond %i... ", i);
        fflush(stdout);
        if (pthread_cond_destroy(&conditions[i]) != 0) {
            perror("Problème destruction cond");
        } else {
            printf("ok\n");
        }
    }

    printf("Destruction mutex... ");
    fflush(stdout);
    pthread_mutex_destroy(&mutex);
    printf("ok\n");
}

void cleanupHandler(void *arg) {
    pthread_mutex_unlock(&mutex);
}

void incrementTime(struct timespec *ts) {
    ts->tv_nsec += (RESPONDEVERY + (rand() % 50)) * 1000 * 1000;
    if (ts->tv_nsec >= 1000 * 1000 * 1000) {
        ts->tv_nsec %= 1000 * 1000 * 1000;
        ts->tv_sec += 1;
    }
}

void decrementFuel(plane_struct *info) {
    --(info->fuel);
}

void requestLanding(plane_struct *info){
    pthread_mutex_lock(&mutex);

    int res;
    const char* size = sizes[info->large];
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    int solicitation = info->large ? LARGE_PLANE_LANDING : SMALL_PLANE_LANDING;
    int* preferedRunwayFree = info->large ? &largeRunwayFree : &smallRunwayFree;

    printf("Demande d'atterrissage %s avion (%i)\n", size, info->id);
    fflush(stdout);

    ++numberPlanesWaiting[solicitation];
    while (!(*preferedRunwayFree) && !largeRunwayFree && info->fuel > CRITICALFUEL) {
        printf("Avion %i attend %s piste pour atterrir\n", info->id, info->large ? "large" : "une");
        fflush(stdout);
        decrementFuel(info);
        do {
            respondInfoRequest(info);
            incrementTime(&ts);

            res = pthread_cond_timedwait(&conditions[solicitation], &mutex, &ts);

            decrementFuel(info);
        } while (res == ETIMEDOUT && info->fuel > CRITICALFUEL);
    }
    --numberPlanesWaiting[solicitation];

    if (!(*preferedRunwayFree) && !largeRunwayFree && info->fuel <= CRITICALFUEL) {
        solicitation = info->large ? PRIORITIZED_LARGE_PLANE_LANDING : PRIORITIZED_SMALL_PLANE_LANDING;

        ++numberPlanesWaiting[solicitation];
        do {
            printf("Avion %i attend %s piste pour atterrir d'urgence\n", info->id, info->large ? "large" : "une");
            fflush(stdout);

            decrementFuel(info);
            do {
                respondInfoRequest(info);
                incrementTime(&ts);

                res = pthread_cond_timedwait(&conditions[solicitation], &mutex, &ts);

                decrementFuel(info);
            } while (res == ETIMEDOUT);
        } while (!(*preferedRunwayFree) && !largeRunwayFree);
        --numberPlanesWaiting[solicitation];
    }

    if (*preferedRunwayFree) {
        *preferedRunwayFree = 0;
        info->runwayNumber = preferedRunwayFree == &smallRunwayFree ? 0 : 1;
    } else {
        largeRunwayFree = 0;
        info->runwayNumber = 1;
    }
    printf("Avion %i atterit sur %s piste\n", info->id, sizes[info->runwayNumber]);
    fflush(stdout);

    pthread_mutex_unlock(&mutex);
}

void freeRunway(plane_struct *info) {
    pthread_mutex_lock(&mutex);

    const char* runwaySize = sizes[info->runwayNumber];
    int i = info->runwayNumber == 0 ? 1 : 0;
    int increment = info->runwayNumber == 0 ? 2 : 1;
    int* runwayFree = info->runwayNumber == 0 ? &smallRunwayFree : &largeRunwayFree;

    printf("Avions %i (%s) libère %s piste\n", info->id, sizes[info->large], runwaySize);
    fflush(stdout);
    *runwayFree = 1;
    info->runwayNumber = -1;

    while (i < NUMBER_SOLICITATION_TYPES && !numberPlanesWaiting[i]) {
        i += increment;
    }
    switch (i) {
        case PRIORITIZED_LARGE_PLANE_LANDING:
            printf("Donne la %s piste à un gros avion prioritaire pour atterir\n", runwaySize);
            break;
        case PRIORITIZED_SMALL_PLANE_LANDING:
            printf("Donne la %s piste à un petit avion prioritaire pour atterir\n", runwaySize);
            break;
        case LARGE_PLANE_LANDING:
            printf("Donne la %s piste à un gros avion pour atterir\n", runwaySize);
            break;
        case SMALL_PLANE_LANDING:
            printf("Donne la %s piste à un petit avion pour atterir\n", runwaySize);
            break;
        case LARGE_PLANE_TAKEOFF:
            printf("Donne la %s piste à un gros avion pour décoler\n", runwaySize);
            break;
        case SMALL_PLANE_TAKEOFF:
            printf("Donne la %s piste à un petit avion pour décoler\n", runwaySize);
            break;
        default:
            printf("Aucun avion en attente\n");
    }
    fflush(stdout);

    pthread_cond_signal(&conditions[i]);
    pthread_mutex_unlock(&mutex);
}

void requestTakeoff(plane_struct *info){
    pthread_mutex_lock(&mutex);

    int res;
    const char* size = sizes[info->large];
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    int solicitation = info->large ? LARGE_PLANE_TAKEOFF : SMALL_PLANE_TAKEOFF;
    int* preferedRunwayFree = info->large ? &largeRunwayFree : &smallRunwayFree;

    printf("Demande de décolage %s avion (%i)\n", size, info->id);
    fflush(stdout);

    ++numberPlanesWaiting[solicitation];
    while (!(*preferedRunwayFree) && !largeRunwayFree){
        printf("Avion %i attend %s piste pour décoler\n", info->id, info->large ? "large" : "une");
        fflush(stdout);

        do {
            respondInfoRequest(info);
            incrementTime(&ts);

            res = pthread_cond_timedwait(&conditions[solicitation], &mutex, &ts);
        } while (res == ETIMEDOUT);
    }
    --numberPlanesWaiting[solicitation];

    if (*preferedRunwayFree) {
        *preferedRunwayFree = 0;
        info->runwayNumber = preferedRunwayFree == &smallRunwayFree ? 0 : 1;
    } else {
        largeRunwayFree = 0;
        info->runwayNumber = 1;
    }
    printf("Avion %i décole sur %s piste\n", info->id, sizes[info->runwayNumber]);
    fflush(stdout);

    pthread_mutex_unlock(&mutex);
}

void tower(const int *planeIds) {
    plane_struct planeInfo;
    int cx;
    char infos[PLANENUMBER * BUFFER];

    printf("Tour de contrôle créée\n\n");
    fflush(stdout);

    while (1) {
        cx = 0;
        for (int i = 0; i < PLANENUMBER; ++i) {
            planeInfo.id = planeIds[i];
            getInfoPlane(&planeInfo);

            cx += snprintf(infos + cx, PLANENUMBER * BUFFER - cx,
                    "Recu info avion no %i, de type %s (%s) ayant %i places, %i%% de fuel\n",
                    planeInfo.id, planeInfo.model, sizes[planeInfo.large], planeInfo.seats, planeInfo.fuel);
        }
        printf("\n%s\n", infos);
        sleep(5);
    }
}
