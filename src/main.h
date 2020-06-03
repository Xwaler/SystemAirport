#ifndef LO41_MAIN_H
#define LO41_MAIN_H

#include <stdbool.h>

#define LINES_PER_PAGE 19
#define PAGES 30
#define PLANE_NUMBER (PAGES * LINES_PER_PAGE)
#define LINE_BUFFER 300

extern unsigned int msgid;
extern bool logging;

void traitantSIGINT(int signo);

void traitantSIGTSTP(int signo);

int getNewId();

void printPlanesInfo(int page);

void help();

void usage();

int main(int argc, char **argv);

#endif //LO41_MAIN_H
