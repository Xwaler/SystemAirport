#ifndef LO41_MAIN_H
#define LO41_MAIN_H

#define DISPLAYED_LINES 19
#define PLANE_NUMBER (12 * DISPLAYED_LINES)
#define BUFFER 200

extern int msgid;
extern int logging;

void traitantSIGINT(int);

void traitantSIGTSTP(int);

int getNewId();

void printPlanesInfo();

int main(int, char **);

#endif //LO41_MAIN_H