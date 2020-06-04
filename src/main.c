#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include "main.h"
#include "tower.h"
#include "plane.h"

#define MAX_ID 999
#define MIN_CONSOLE_WIDTH 147
#define MIN_CONSOLE_HEIGHT (2 + LINES_PER_PAGE + 4 + 2)

unsigned int msgid;
unsigned int firstLine = 0;
unsigned int i, j, cx;
bool logging;

char infos[(LINES_PER_PAGE + 2) * LINE_BUFFER];
char buf_d1[9], buf_d2[9], buf_d3[4], buf_d4[4];
time_t now;
char date_format[] = "%H:%M:%S";
plane_struct planesBuffer[LINES_PER_PAGE];
char progress[5];

pthread_t planes[PLANE_NUMBER];
bool usedIds[MAX_ID + 1] = {false};
unsigned int planeIds[PLANE_NUMBER];

void traitantSIGINT(const int signo) {
    printf("\033[2K");
    printf("\nSignal SIGINT reçu, destruction !\n");

    printf("Destruction file de messages %i... ", msgid);
    fflush(stdout);
    msgctl(msgid, IPC_RMID, NULL);
    printf("ok\n");

    void *ret;
    for (int k = 0; k < PLANE_NUMBER; ++k) {
        printf("Destruction avions %i / %i\r", k + 1, PLANE_NUMBER);
        fflush(stdout);
        usleep(500);

        if (pthread_cancel(planes[k]) != 0) {
            perror("Problème thread cancel");
            exit(1);
        }
        pthread_join(planes[k], &ret);
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
    firstLine = (firstLine + LINES_PER_PAGE) % PLANE_NUMBER;
}

int getNewId() {
    int id;
    do { id = (rand() % (MAX_ID - 1)) + 2; } while (usedIds[id]); // pour eviter msg type == 0 (erreur) ou 1 (tour de controle)
    usedIds[id] = true;
    return id;
}

void printPlanesInfo(int page) {
    plane_struct info;
    int *waiting;
    time(&now);

    cx = snprintf(infos, LINES_PER_PAGE * LINE_BUFFER,
                  "  IDF   MODELE    TAILLE                      ORIGINE --> DESTINATION                           ETAT             REDIGIGE VERS    ALERTE     FUEL\n\n");

    for (i = 0; i < LINES_PER_PAGE; ++i) {
        info = planesBuffer[i];

        if (info.progress == 0.f) {
            strcpy(progress, "");
        } else {
            sprintf(progress, "%3.0f%%", roundf(info.progress));
        }

        strftime(buf_d1, sizeof(buf_d1), date_format, localtime(&info.targetTimeTakeoff));
        strftime(buf_d2, sizeof(buf_d2), date_format, localtime(&info.targetTimeLanding));

        if (info.timeTakeoff) {
            int diff = (int) (info.timeTakeoff - info.targetTimeTakeoff);
            if (diff <= 0) sprintf(buf_d3, "-%02i", abs(diff));
            else sprintf(buf_d3, "+%02i", diff);
        } else {
            strcpy(buf_d3, " ");
        }

        if (info.timeLanding) {
            int diff = (int) (info.timeLanding - info.targetTimeLanding);
            if (diff <= 0) sprintf(buf_d4, "-%02i", abs(diff));
            else sprintf(buf_d4, "+%02i", diff);
        } else {
            strcpy(buf_d4, " ");
        }

        cx += snprintf(infos + cx, LINES_PER_PAGE * LINE_BUFFER - cx,
                       "| %03i | %-7s | %-5s | \033[0;3%im%s %3s\033[0;37m  \033[0;3%im%13s\033[0;37m --> %-13s  \033[0;3%im%3s %s\033[0;37m | %4s %-17s | %-13s | %-9s | %3.0f%% |\n",
                       info.id, info.model, sizes[info.large],
                       info.lateTakeoff ? 3 : info.timeTakeoff ? 2 : 7, buf_d1, buf_d3,
                       info.hasBeenRedirected ? 3 : 7, airports[info.origin].name,
                       airports[info.destination].name,
                       info.lateLanding ? 3 : info.timeLanding ? 2 : 7, buf_d4, buf_d2,
                       progress, states[info.state],
                       info.redirection <= NOT_REDIRECTED ? "" : airports[info.redirection].name,
                       info.alert == NONE ? "" : alerts[info.alert], roundf(info.fuel));
    }

    cx += snprintf(infos + cx, LINES_PER_PAGE * LINE_BUFFER - cx,
                   "|------------------------------------------------------------------------------------"
                   "-------------------------------------------------------------|");

    waiting = (int *) &numberPlanesWaiting[page];
    pthread_mutex_lock(&(mutex[page]));
    cx += snprintf(infos + cx, LINES_PER_PAGE * LINE_BUFFER - cx,
                   "\n|     Aeroport          |      Atterissage large       |      Atterissage petit       |      Decolage large          |      Decolage petit        |\n"
                   "|     %-13s     |      %02i (%02i prioritaire)     |      %02i (%02i prioritaire)     |      %02i (%02i prioritaire)     |      %02i (%02i prioritaire)   |\n",
                   airports[page].name,
                   waiting[4] + waiting[0], waiting[0], waiting[5] + waiting[1], waiting[1],
                   waiting[6] + waiting[2], waiting[2], waiting[7] + waiting[3], waiting[3]);
    pthread_mutex_unlock(&(mutex[page]));

    cx += snprintf(infos + cx, LINES_PER_PAGE * LINE_BUFFER - cx,
                   "|------------------------------------------------------------------------------------"
                   "-------------------------------------------------------------|");

    strftime(buf_d1, sizeof(buf_d1), date_format, localtime(&now));
    cx += snprintf(infos + cx, LINES_PER_PAGE * LINE_BUFFER - cx,
                   "\nPage %2i / %02i (CTRL-Z pour passer a la page suivante) - Heure locale %s\n", page + 1, PAGES,
                   buf_d1);

    printf("\033[H");
    printf("%s", infos);
    fflush(stdout);
}

void help() {
    printf("SystemAirport -h pour afficher l'aide\n");
    exit(1);
}

void usage() {
    printf("Usage: SystemAirport -h | -l\n"
           "-h : affiche l'aide\n"
           "-l : remplace l'affichage en tableau par du texte\n");
    exit(1);
}

int main(int argc, char **argv) {
    int opt;
    while ((opt = getopt(argc, argv, "hl")) != -1) {
        switch (opt) {
            case 'l':
                logging = true;
                break;
            case 'h':
                usage();
            default:
                help();
        }
    }
    if (optind < argc) help();

    signal(SIGINT, traitantSIGINT);
    signal(SIGTSTP, traitantSIGTSTP);

    key_t clef;
    unsigned int id, start, end;
    srand(getpid());

    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    if (!logging && (w.ws_col < MIN_CONSOLE_WIDTH || w.ws_row < MIN_CONSOLE_HEIGHT)) {
        printf("La taille de la fenetre est trop petite (%i x %i), merci d'agrandir la console, >= (%i x %i)\n",
                w.ws_col, w.ws_row, MIN_CONSOLE_WIDTH, MIN_CONSOLE_HEIGHT);
        exit(1);
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

    initRunways();

    fflush(stdout);
    for (i = 0; i < PLANE_NUMBER; ++i) {
        printf("Creation avions %i / %i\r", i + 1, PLANE_NUMBER);
        fflush(stdout);
        usleep(500);

        id = getNewId();
        planeIds[i] = id;
        pthread_create(&planes[i], NULL, plane, &(planeIds[i]));
    }
    printf("\n");

    printf("Tour de contrôle créée\n\n");
    fflush(stdout);
    usleep(100 * 1000);

    printf("\033[1;1H\033[2J");
    fflush(stdout);

    plane_struct temp;
    while (true) {
        if (!logging) {
            start = firstLine;
            end = start + LINES_PER_PAGE;

            for (i = start; i < end; ++i) sendRequestInfo(planeIds[i]);
            for (i = start; i < end; ++i) {
                temp = getRequestResponse();
                j = start;
                while (planeIds[j] != temp.id) ++j;
                planesBuffer[j % LINES_PER_PAGE] = temp;
            }
            printPlanesInfo((int) (start / LINES_PER_PAGE));
        } else {
            usleep(1000);
        }
    }
    return 0;
}
