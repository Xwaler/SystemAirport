#ifndef LO41_MAIN_H
#define LO41_MAIN_H

#include <stdbool.h>

#define DISPLAYED_LINES 19
#define PLANE_NUMBER (12 * DISPLAYED_LINES)
#define BUFFER 200

extern int msgid;
extern bool logging;

void traitantSIGINT(int signo);

void traitantSIGTSTP(int signo);

int getNewId();

void printPlanesInfo();

void help();

void usage();

int main(int argc, char **argv);

#endif //LO41_MAIN_H
