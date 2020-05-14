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
int usedIds[MAXID] = {0};
int planeIds[PLANENUMBER];
pthread_t planes[PLANENUMBER];

void traitantSIGINT(int signo) {
    printf("\nSignal SIGINT reçu, destruction !\n");

    printf("Destruction file de messages %i... ", msgid);
    fflush(stdout);
    msgctl(msgid, IPC_RMID, NULL);
    printf("ok\n");

    void* ret;
    for (int i = 0; i < PLANENUMBER; ++i) {
        printf("Destruction avion %i... ", planeIds[i]);
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

    printf("\nExit\n");
    exit(0);
}

int getNewId() {
    int i = 1; // pour eviter msg type == 0
    while (i < MAXID && usedIds[i]) {
        ++i;
    }
    usedIds[i] = 1;
    return i;
}

int main(int argc, char **argv) {
    signal(SIGINT, traitantSIGINT);

    key_t clef;
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

    printf("Creation de %i avions... ", PLANENUMBER);
    fflush(stdout);
    for (int i = 0; i < PLANENUMBER; ++i) {
        planeIds[i] = getNewId();
        pthread_create(&planes[i], NULL, plane, &(planeIds[i]));
    }
    printf("ok\n");

    tower(planeIds);
    return 0;
}
