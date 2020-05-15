#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <pthread.h>

#include "main.h"
#include "tower.h"
#include "plane.h"

#define MAXID 999

int msgid;
int logging = 0;

int usedIds[MAXID + 1] = {0};
int planeIds[PLANENUMBER];

char infos[PLANENUMBER * BUFFER];

pthread_t planes[PLANENUMBER];
plane_struct planeInfos[PLANENUMBER];

void traitantSIGINT(int signo) {
    printf("\nSignal SIGINT reçu, destruction !\n");

    printf("Destruction file de messages %i... ", msgid);
    fflush(stdout);
    msgctl(msgid, IPC_RMID, NULL);
    printf("ok\n");

    void* ret;
    for (int i = 0; i < PLANENUMBER; ++i) {
        printf("Destruction avion %03i... ", planeIds[i]);
        fflush(stdout);
        if (pthread_cancel(planes[i]) != 0) {
            perror("Problème thread cancel");
            exit(1);
        }
        pthread_join(planes[i], &ret);
        if (ret == PTHREAD_CANCELED) {
            printf("ok\n");
            fflush(stdout);
        } else {
            perror("Problème thread join");
            exit(1);
        }
    }

    deleteRunways();

    printf("Destruction réussie\n");
    exit(0);
}

int getNewId() {
    int id;
    do {
        id = rand() % MAXID;
    } while (id != 0 && usedIds[id]); // pour eviter msg type == 0
    usedIds[id] = 1;
    return id;
}

void printPlanesInfo() {
    int cx = snprintf(infos, PLANENUMBER * BUFFER,
            "VOL  MODEL           TAILLE    CAPACTIE            DEPART --> DESTINATION      ETAT              CONDITION           FUEL\n\n");
    for (int i = 0; i < PLANENUMBER; ++i) {
        cx += snprintf(infos + cx, PLANENUMBER * BUFFER - cx,
                       "%03i  %s  %-8s  %3i / %-3i    %13s --> %-13s    %s  %-18s  %3i%%\n",
                       planeInfos[i].id, planeInfos[i].model, sizes[planeInfos[i].large],
                       planeInfos[i].passengers, planeInfos[i].seats,
                       airportNames[planeInfos[i].depart], airportNames[planeInfos[i].destination],
                       states[planeInfos[i].state], conditions[planeInfos[i].condition], planeInfos[i].fuel);
    }
    printf("\033[H");
    printf("%s", infos);
}

int main(int argc, char **argv) {
    signal(SIGINT, traitantSIGINT);

    key_t clef;
    int i, j, opt;
    srand(getpid());

    while ((opt = getopt(argc, argv, "l")) != -1) {
        if (opt == 'l') {
            logging = 1;
        } else {
            printf("Argument inconnu \"%c\"\n", opt);
        }
    }

    printf("Création file de messages... ");
    fflush(stdout);
    if ((clef = ftok(argv[0], '0')) == -1) {
        perror("Problème ftok");
        exit(1);
    }
    if ((msgid = msgget(clef, IPC_CREAT | IPC_EXCL | 0600)) == -1) {
        perror("Problème msgget");
        exit(1);
    }
    printf("(msgid %i) ok\n", msgid);

    printf("Creation des pistes... ");
    fflush(stdout);
    initRunways();
    printf("ok\n");

    printf("Creation de %i avions... ", PLANENUMBER);
    fflush(stdout);
    for (i = 0; i < PLANENUMBER; ++i) {
        planeIds[i] = getNewId();
        pthread_create(&planes[i], NULL, plane, &(planeIds[i]));
    }
    printf("ok\n");

    printf("Tour de contrôle créée\n\n");
    fflush(stdout);

    sleep(1);

    printf("\033[1;1H\033[2J");
    plane_struct temp;
    while (1) {
        if (!logging) {
            for (i = 0; i < PLANENUMBER; ++i) {
                planeInfos[i].id = planeIds[i];
                sendRequestInfo(&(planeInfos[i]));
            }
            for (i = 0; i < PLANENUMBER; ++i) {
                temp = getRequestResponse(planeInfos);
                j = 0;
                while (planeInfos[j].id != temp.id) ++j;
                planeInfos[j] = temp;
            }
            printPlanesInfo();
        }
    }
    return 0;
}
