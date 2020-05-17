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
        "Le Mans",

        "Londres",
        "Amsterdam",
        "Frankfort",
        "Madrid",
        "Barcelone",
        "Istanbul",
        "Moscou",
        "Munich",
        "Rome",
        "Dublin",
};
const int airportPostitions[][2] = {{4886, 229}, {4329, 536}, {4575, 483}, {4360, 144}, {4370, 726}, {4721, -155},
                                    {4858, 775}, {4544, -28}, {4450, -36}, {5063, 306}, {4811, -168}, {4925, 403},
                                    {4559, 330}, {4312, 593}, {4949, 10}, {4518, 573}, {4732, 504}, {4747, -55},
                                    {4383, 436}, {4800, 19}, {5150, -12}, {5237, 489}, {5011, 868}, {4041, -370},
                                    {4138, 217}, {4100, 289}, {5575, 376}, {4813, 115}, {4189, 124}, {5334, -626}};

int numberPlanesWaiting[NUMBER_AIRPORT][NUMBER_SOLICITATION_TYPES] = {[0 ... (NUMBER_AIRPORT - 1)] = {}};
pthread_cond_t solicitations[NUMBER_AIRPORT][NUMBER_SOLICITATION_TYPES];

bool largeRunwayFree[NUMBER_AIRPORT] = {[0 ... (NUMBER_AIRPORT - 1)] = true};
bool smallRunwayFree[NUMBER_AIRPORT] = {[0 ... (NUMBER_AIRPORT - 1)] = true};

pthread_mutex_t mutex[NUMBER_AIRPORT];

void initRunways() {
    for (int i = 0; i < NUMBER_AIRPORT; ++i) {
        printf("Creation aéroports %i / %i\r", i + 1, NUMBER_AIRPORT);
        fflush(stdout);
        usleep(1000);

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
        usleep(1000);
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

void vectorPlane(float *vector, const float latitude, const float longitude, const int *destination) {
    vector[LATITUDE] = (float) destination[LATITUDE] - latitude;
    vector[LONGITUDE] = (float) destination[LONGITUDE] - longitude;
}

void vectorAirport(float *vector, const int *start, const int *destination) {
    vector[LATITUDE] = (float) (destination[LATITUDE] - start[LATITUDE]);
    vector[LONGITUDE] = (float) (destination[LONGITUDE] - start[LONGITUDE]);
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
    const char* size = sizes[info->large];
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    int solicitation = info->large ? LARGE_PLANE_LANDING : SMALL_PLANE_LANDING;
    bool* preferedRunwayFree = info->large ? &(largeRunwayFree[info->actual]) : &(smallRunwayFree[info->actual]);

    if (logging) {
        printf("%s: Demande d'atterrissage %s avion %03i\n",
                airportNames[info->actual], size, info->id);
        fflush(stdout);
    }

    ++(numberPlanesWaiting[info->actual][solicitation]);
    while (info->alert == NONE && !(*preferedRunwayFree) && !largeRunwayFree[info->actual]) {
        if (logging) {
            printf("%s: Avion %03i attend %s piste pour atterrir\n",
                   airportNames[info->actual], info->id, info->large ? "large" : "une");
            fflush(stdout);
        }

        info->state = WAITING_IN_FLIGHT;
        decrementFuel(info);
        do {
            respondInfoRequest(info);
            incrementTime(&ts);

            res = pthread_cond_timedwait(&solicitations[info->actual][solicitation], &mutex[info->actual], &ts);

            decrementFuel(info);
        } while (res == ETIMEDOUT && info->alert == NONE);
    }
    --(numberPlanesWaiting[info->actual][solicitation]);

    if (info->alert != NONE && !(*preferedRunwayFree) && !largeRunwayFree[info->actual]) {
        solicitation = info->large ? PRIORITIZED_LARGE_PLANE_LANDING : PRIORITIZED_SMALL_PLANE_LANDING;

        ++(numberPlanesWaiting[info->actual][solicitation]);
        do {
            if (logging) {
                printf("%s: Avion %03i attend %s piste pour atterrir d'urgence\n",
                       airportNames[info->actual], info->id, info->large ? "large" : "une");
                fflush(stdout);
            }

            decrementFuel(info);
            do {
                respondInfoRequest(info);
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
               airportNames[info->actual], info->id, sizes[info->runwayNumber]);
        fflush(stdout);
    }

    pthread_mutex_unlock(&(mutex[info->actual]));
}

void freeRunway(plane_struct *info) {
    pthread_mutex_lock(&(mutex[info->actual]));

    const char* runwaySize = sizes[info->runwayNumber];
    int i = info->runwayNumber == SMALL_RUNWAY ? 1 : 0;
    int increment = info->runwayNumber == SMALL_RUNWAY ? 2 : 1;
    bool* runwayFree = info->runwayNumber == SMALL_RUNWAY ? &(smallRunwayFree[info->actual]) : &(largeRunwayFree[info->actual]);

    if (logging) {
        printf("%s: Le %s avion %03i libère %s piste\n",
               airportNames[info->actual], sizes[info->large], info->id, runwaySize);
        fflush(stdout);
    }
    *runwayFree = true;
    info->runwayNumber = NO_RUNWAY;

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
            default:
                break;
        }
        fflush(stdout);
    }

    pthread_cond_signal(&(solicitations[info->actual][i]));
    pthread_mutex_unlock(&(mutex[info->actual]));
}

void requestTakeoff(plane_struct *info){
    pthread_mutex_lock(&(mutex[info->actual]));

    int res;
    const char* size = sizes[info->large];
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    int solicitation = info->large ? LARGE_PLANE_TAKEOFF : SMALL_PLANE_TAKEOFF;
    bool* preferedRunwayFree = info->large ? &(largeRunwayFree[info->actual]) : &(smallRunwayFree[info->actual]);

    if (logging) {
        printf("%s: Demande de décolage %s avion %03i\n",
               airportNames[info->actual], size, info->id);
        fflush(stdout);
    }

    ++(numberPlanesWaiting[info->actual][solicitation]);
    while (!(*preferedRunwayFree) && !largeRunwayFree[info->actual]){
        if (logging) {
            printf("%s: Avion %03i attend %s piste pour décoler\n",
                   airportNames[info->actual], info->id, info->large ? "large" : "une");
            fflush(stdout);
        }

        do {
            respondInfoRequest(info);
            incrementTime(&ts);

            res = pthread_cond_timedwait(&(solicitations[info->actual][solicitation]), &(mutex[info->actual]), &ts);
        } while (res == ETIMEDOUT);
    }
    --(numberPlanesWaiting[info->actual][solicitation]);

    if (*preferedRunwayFree) {
        *preferedRunwayFree = false;
        info->runwayNumber = preferedRunwayFree == &(smallRunwayFree[info->actual]) ? SMALL_RUNWAY : LARGE_RUNWAY;
    } else {
        largeRunwayFree[info->actual] = true;
        info->runwayNumber = LARGE_RUNWAY;
    }
    if (logging) {
        printf("%s: Avion %03i décole sur %s piste\n",
               airportNames[info->actual], info->id, sizes[info->runwayNumber]);
        fflush(stdout);
    }

    pthread_mutex_unlock(&(mutex[info->actual]));
}
