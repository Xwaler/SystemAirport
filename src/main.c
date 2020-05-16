#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <pthread.h>

#include "main.h"
#include "tower.h"
#include "plane.h"

#define MAX_ID 999

int msgid;
int logging;
int firstLine = 0;

int usedIds[MAX_ID + 1] = {0};
int planeIds[PLANE_NUMBER];

char infos[PLANE_NUMBER * BUFFER];

pthread_t planes[PLANE_NUMBER];
plane_struct planeInfos[PLANE_NUMBER];

void traitantSIGINT(const int signo) {
    printf("\033[2K");
    printf("\nSignal SIGINT reçu, destruction !\n");

    printf("Destruction file de messages %i... ", msgid);
    fflush(stdout);
    msgctl(msgid, IPC_RMID, NULL);
    printf("ok\n");

    void* ret;
    for (int i = 0; i < PLANE_NUMBER; ++i) {
        printf("Destruction avion %i / %i\r", i + 1, PLANE_NUMBER);
        fflush(stdout);
        if (pthread_cancel(planes[i]) != 0) {
            perror("Problème thread cancel");
            exit(1);
        }
        pthread_join(planes[i], &ret);
        if (ret != PTHREAD_CANCELED) {
            perror("Problème thread join");
            exit(1);
        }
    }
    printf("\n");

    deleteRunways();

    printf("Destruction réussie\n");
    exit(0);
}

void traitantSIGTSTP(const int signo) {
    printf("\033[2K");
    firstLine = (firstLine + DISPLAYED_LINES) % PLANE_NUMBER;
}

int getNewId() {
    int id;
    do {
        id = rand() % (MAX_ID + 1);
    } while (id != 0 && usedIds[id]); // pour eviter msg type == 0
    usedIds[id] = 1;
    return id;
}

void printPlanesInfo() {
    int start = firstLine;
    int end = start + DISPLAYED_LINES;
    plane_struct info;

    int cx = snprintf(infos, DISPLAYED_LINES * BUFFER,
                      "VOL  MODEL           TAILLE            DEPART --> DESTINATION  PROGRES  ETAT              REDIGIGE VERS  CONDITION           FUEL\n\n");
    for (int i = start; i < end; ++i) {
        info = planeInfos[i];
        cx += snprintf(infos + cx, DISPLAYED_LINES * BUFFER - cx,
                       "%03i  %-14s  %-8s   %13s --> %-13s   %3i%%  %-16s  %-13s  %-18s  %3i%%\n",
                       info.id, info.model, sizes[info.large], airportNames[info.start],
                       airportNames[info.destination], info.progress, states[info.state],
                       info.redirection <= NOT_REDIRECTED ? "" : airportNames[info.redirection],
                       conditions[info.condition], info.fuel);
    }
    snprintf(infos + cx, DISPLAYED_LINES * BUFFER - cx, "\nPage %2i / %02i (CTRL-Z pour passer à la page suivante)\n",
             (int) (start / DISPLAYED_LINES) + 1, (int) PLANE_NUMBER / DISPLAYED_LINES);

    printf("\033[H");
    printf("%s", infos);
}

void help() {
    printf("SystemAirport -h pour afficher l'aide\n");
    exit(1);
}

void usage() {
    printf("Usage: SystemAirport -h | -l [tab, log]\n");
    exit(1);
}

int main(int argc, char **argv) {
    int opt;
    while ((opt = getopt(argc, argv, "hl:")) != -1) {
        switch (opt) {
            case 'l':
                if (strcmp(optarg, "tab") == 0) {
                    logging = 0;
                    break;
                } else if (strcmp(optarg, "log") == 0) {
                    logging = 1;
                    break;
                }
            case 'h':
                usage();
            default:
                help();
        }
    }
    if (argc == 1 || optind < argc) help();

    signal(SIGINT, traitantSIGINT);
    signal(SIGTSTP, traitantSIGTSTP);

    key_t clef;
    int i, j, id;
    srand(getpid());

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

    printf("Creation de %i avions... ", PLANE_NUMBER);
    fflush(stdout);
    for (i = 0; i < PLANE_NUMBER; ++i) {
        id = getNewId();
        planeIds[i] = id;
        planeInfos[i].id = id;
        pthread_create(&planes[i], NULL, plane, &(planeIds[i]));
    }
    printf("ok\n");

    printf("Tour de contrôle créée\n\n");
    printf("\033[1;1H\033[2J");
    fflush(stdout);

    plane_struct temp;
    while (1) {
        if (!logging) {
            for (i = 0; i < PLANE_NUMBER; ++i) sendRequestInfo(&(planeInfos[i]));
            for (i = 0; i < PLANE_NUMBER; ++i) {
                temp = getRequestResponse();
                j = 0;
                while (planeInfos[j].id != temp.id) ++j;
                planeInfos[j] = temp;
            }
            printPlanesInfo();
        } else {
            usleep(1000);
        }
    }
    return 0;
}
