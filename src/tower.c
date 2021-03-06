#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/time.h>
#include <pthread.h>
#include <math.h>

#include "main.h"
#include "tower.h"

const airport airports[] = {
        {"Paris",         {4886, 229}},
        {"Marseille",     {4329, 536}},
        {"Lyon",          {4575, 483}},
        {"Toulouse",      {4360, 144}},
        {"Nice",          {4370, 726}},
        {"Nantes",        {4721, -155}},
        {"Montpellier",   {4858, 775}},
        {"Strasbourg",    {4544, -28}},
        {"Bordeaux",      {4450, -36}},
        {"Lille",         {5063, 306}},
        {"Rennes",        {4811, -168}},
        {"Reims",         {4925, 403}},
        {"Saint-Etienne", {4559, 330}},
        {"Toulon",        {4312, 593}},
        {"Le Havre",      {4949, 10}},
        {"Grenoble",      {4518, 573}},
        {"Dijon",         {4732, 504}},
        {"Angers",        {4747, -55}},
        {"Nimes",         {4383, 436}},
        {"Le Mans",       {4800, 19}},

        {"Londres",       {5150, -12}},
        {"Amsterdam",     {5237, 489}},
        {"Frankfort",     {5011, 868}},
        {"Madrid",        {4041, -370}},
        {"Barcelone",     {4138, 217}},
        {"Istanbul",      {4100, 289}},
        {"Moscou",        {5575, 376}},
        {"Munich",        {4813, 115}},
        {"Rome",          {4189, 124}},
        {"Dublin",        {5334, -626}}
};

unsigned int numberPlanesWaiting[NUMBER_AIRPORT][NUMBER_SOLICITATION_TYPES] = {[0 ... (NUMBER_AIRPORT - 1)] = {}};
pthread_cond_t solicitations[NUMBER_AIRPORT][NUMBER_SOLICITATION_TYPES];

bool largeRunwayFree[NUMBER_AIRPORT] = {[0 ... (NUMBER_AIRPORT - 1)] = true};
bool smallRunwayFree[NUMBER_AIRPORT] = {[0 ... (NUMBER_AIRPORT - 1)] = true};

pthread_mutex_t mutex[NUMBER_AIRPORT];

void initRunways() {
    for (int i = 0; i < NUMBER_AIRPORT; ++i) {
        printf("Creation aéroports %i / %i\r", i + 1, NUMBER_AIRPORT);
        fflush(stdout);
        usleep(500);

        pthread_mutex_init(&(mutex[i]), 0);

        for (int j = 0; j < NUMBER_SOLICITATION_TYPES; ++j) {
            pthread_cond_init(&(solicitations[i][j]), 0);
        }
    }
    printf("\n");
}

void deleteRunways() {
    for (int i = 0; i < NUMBER_AIRPORT; ++i) {
        printf("Destruction aéroports %i / %i\r", i + 1, NUMBER_AIRPORT);
        fflush(stdout);
        usleep(500);
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
    ts->tv_nsec += (UPDATE_EVERY + (rand() % 50)) * 1000 * 1000;
    if (ts->tv_nsec >= 1000 * 1000 * 1000) {
        ts->tv_nsec %= 1000 * 1000 * 1000;
        ts->tv_sec += 1;
    }
}

void getVector(float vector[], const position *origine, const position *dest) {
    vector[LATITUDE] = (float) dest->latitude - origine->latitude;
    vector[LONGITUDE] = (float) dest->longitude - origine->longitude;
}

float distance(float vector[]) {
    return sqrtf(powf(vector[LATITUDE], 2) + powf(vector[LONGITUDE], 2));
}

void normalize(float normalizedVector[], const float vector[], float d) {
    normalizedVector[0] = vector[0] / d * SPEED;
    normalizedVector[1] = vector[1] / d * SPEED;
}

void requestLanding(plane_struct *info) {
    pthread_mutex_lock(&(mutex[info->actual]));

    int res;
    const char *size = sizes[info->type.large];
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    unsigned int solicitation = info->type.large ? LARGE_PLANE_LANDING : SMALL_PLANE_LANDING;
    bool *preferedRunwayFree = info->type.large ? &(largeRunwayFree[info->actual]) : &(smallRunwayFree[info->actual]);

    if (logging) {
        printf("%s: Demande d'atterrissage %s avion %03i\n",
               airports[info->actual].name, size, info->id);
        fflush(stdout);
    }

    ++(numberPlanesWaiting[info->actual][solicitation]);
    while (info->alert == NONE && !info->lateLanding && !(*preferedRunwayFree) && !largeRunwayFree[info->actual]) {
        if (logging) {
            printf("%s: Avion %03i attend %s piste pour atterrir\n",
                   airports[info->actual].name, info->id, info->type.large ? "large" : "une");
            fflush(stdout);
        }

        info->state = WAITING_LANDING;
        decrementFuel(info);
        do {
            respondInfoRequest(info);
            incrementTime(&ts);

            res = pthread_cond_timedwait(&solicitations[info->actual][solicitation], &mutex[info->actual], &ts);

            decrementFuel(info);
        } while (res == ETIMEDOUT && info->alert == NONE && !info->lateLanding);
    }
    --(numberPlanesWaiting[info->actual][solicitation]);

    if ((info->alert != NONE || info->lateLanding) && !(*preferedRunwayFree) && !largeRunwayFree[info->actual]) {
        solicitation = info->type.large ? PRIORITIZED_LARGE_PLANE_LANDING : PRIORITIZED_SMALL_PLANE_LANDING;

        ++(numberPlanesWaiting[info->actual][solicitation]);
        do {
            if (logging) {
                printf("%s: Avion %03i attend %s piste pour atterrir d'urgence\n",
                       airports[info->actual].name, info->id, info->type.large ? "large" : "une");
                fflush(stdout);
            }

            decrementFuel(info);
            do {
                checkIfLate(info);
                respondInfoRequest(info);
                if (info->alert == CRASHED) {
                    --(numberPlanesWaiting[info->actual][solicitation]);
                    pthread_mutex_unlock(&(mutex[info->actual]));
                    return;
                }
                incrementTime(&ts);

                res = pthread_cond_timedwait(&solicitations[info->actual][solicitation], &mutex[info->actual], &ts);

                decrementFuel(info);
            } while (res == ETIMEDOUT);
        } while (!(*preferedRunwayFree) && !largeRunwayFree[info->actual]);
        --(numberPlanesWaiting[info->actual][solicitation]);
    }

    if (*preferedRunwayFree) {
        *preferedRunwayFree = false;
        info->runwayNumber = preferedRunwayFree == &(smallRunwayFree[info->actual]) ? SMALL_RUNWAY : LARGE_RUNWAY;
    } else {
        largeRunwayFree[info->actual] = false;
        info->runwayNumber = LARGE_RUNWAY;
    }
    if (logging) {
        printf("%s: Avion %03i atterit sur %s piste\n",
               airports[info->actual].name, info->id, sizes[info->runwayNumber]);
        fflush(stdout);
    }

    pthread_mutex_unlock(&(mutex[info->actual]));
}

void freeRunway(plane_struct *info) {
    pthread_mutex_lock(&(mutex[info->actual]));

    const char *runwaySize = sizes[info->runwayNumber];
    signed char i = info->runwayNumber == SMALL_RUNWAY ? 1 : 0;
    signed char increment = info->runwayNumber == SMALL_RUNWAY ? 2 : 1;
    bool *runwayFree = info->runwayNumber == SMALL_RUNWAY ? &(smallRunwayFree[info->actual]) : &(largeRunwayFree[info->actual]);

    if (logging) {
        printf("%s: Le %s avion %03i libère %s piste\n",
               airports[info->actual].name, sizes[info->type.large], info->id, runwaySize);
        fflush(stdout);
    }
    *runwayFree = true;
    info->runwayNumber = NO_RUNWAY;

    while (i < NUMBER_SOLICITATION_TYPES && !numberPlanesWaiting[info->actual][i]) { i = (signed char) (i + increment); }
    if (logging) {
        switch (i) {
            case PRIORITIZED_LARGE_PLANE_LANDING:
                printf("%s: Donne la %s piste à un gros avion prioritaire pour atterir\n",
                       airports[info->actual].name, runwaySize);
                break;
            case PRIORITIZED_SMALL_PLANE_LANDING:
                printf("%s: Donne la %s piste à un petit avion prioritaire pour atterir\n",
                       airports[info->actual].name, runwaySize);
                break;
            case PRIORITIZED_LARGE_PLANE_TAKEOFF:
                printf("%s: Donne la %s piste à un gros avion prioritaire pour décoler\n",
                       airports[info->actual].name, runwaySize);
                break;
            case PRIORITIZED_SMALL_PLANE_TAKEOFF:
                printf("%s: Donne la %s piste à un petit avion prioritaire pour décoler\n",
                       airports[info->actual].name, runwaySize);
                break;
            case LARGE_PLANE_LANDING:
                printf("%s: Donne la %s piste à un gros avion pour atterir\n",
                       airports[info->actual].name, runwaySize);
                break;
            case SMALL_PLANE_LANDING:
                printf("%s: Donne la %s piste à un petit avion pour atterir\n",
                       airports[info->actual].name, runwaySize);
                break;
            case LARGE_PLANE_TAKEOFF:
                printf("%s: Donne la %s piste à un gros avion pour décoler\n",
                       airports[info->actual].name, runwaySize);
                break;
            case SMALL_PLANE_TAKEOFF:
                printf("%s: Donne la %s piste à un petit avion pour décoler\n",
                       airports[info->actual].name, runwaySize);
            default:
                break;
        }
        fflush(stdout);
    }

    pthread_cond_signal(&(solicitations[info->actual][i]));
    pthread_mutex_unlock(&(mutex[info->actual]));
}

void requestTakeoff(plane_struct *info) {
    pthread_mutex_lock(&(mutex[info->actual]));

    int res;
    const char *size = sizes[info->type.large];
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    unsigned int solicitation = info->type.large ? LARGE_PLANE_TAKEOFF : SMALL_PLANE_TAKEOFF;
    bool *preferedRunwayFree = info->type.large ? &(largeRunwayFree[info->actual]) : &(smallRunwayFree[info->actual]);

    if (logging) {
        printf("%s: Demande de décolage %s avion %03i\n",
               airports[info->actual].name, size, info->id);
        fflush(stdout);
    }

    ++(numberPlanesWaiting[info->actual][solicitation]);
    while (!info->lateTakeoff && !(*preferedRunwayFree) && !largeRunwayFree[info->actual]) {
        if (logging) {
            printf("%s: Avion %03i attend %s piste pour décoler\n",
                   airports[info->actual].name, info->id, info->type.large ? "large" : "une");
            fflush(stdout);
        }

        info->state = WAITING_TAKEOFF;
        do {
            checkIfLate(info);
            respondInfoRequest(info);
            incrementTime(&ts);

            res = pthread_cond_timedwait(&(solicitations[info->actual][solicitation]), &(mutex[info->actual]), &ts);
        } while (res == ETIMEDOUT && !info->lateTakeoff);
    }
    --(numberPlanesWaiting[info->actual][solicitation]);

    if (info->lateTakeoff && !(*preferedRunwayFree) && !largeRunwayFree[info->actual]) {
        solicitation = info->type.large ? PRIORITIZED_LARGE_PLANE_TAKEOFF : PRIORITIZED_SMALL_PLANE_TAKEOFF;

        ++(numberPlanesWaiting[info->actual][solicitation]);
        do {
            if (logging) {
                printf("%s: Avion %03i attend %s piste pour décoler d'urgence\n",
                       airports[info->actual].name, info->id, info->type.large ? "large" : "une");
                fflush(stdout);
            }

            do {
                checkIfLate(info);
                respondInfoRequest(info);
                incrementTime(&ts);

                res = pthread_cond_timedwait(&solicitations[info->actual][solicitation], &mutex[info->actual], &ts);
            } while (res == ETIMEDOUT);
        } while (!(*preferedRunwayFree) && !largeRunwayFree[info->actual]);
        --(numberPlanesWaiting[info->actual][solicitation]);
    }

    if (*preferedRunwayFree) {
        *preferedRunwayFree = false;
        info->runwayNumber = preferedRunwayFree == &(smallRunwayFree[info->actual]) ? SMALL_RUNWAY : LARGE_RUNWAY;
    } else {
        largeRunwayFree[info->actual] = false;
        info->runwayNumber = LARGE_RUNWAY;
    }
    if (logging) {
        printf("%s: Avion %03i décole sur %s piste\n",
               airports[info->actual].name, info->id, sizes[info->runwayNumber]);
        fflush(stdout);
    }

    pthread_mutex_unlock(&(mutex[info->actual]));
}
