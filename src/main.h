#ifndef LO41_MAIN_H
#define LO41_MAIN_H

#define DISPLAYED_LINES 21
#define BUFFER 200

extern int msgid;
extern int logging;

void traitantSIGINT(int);

void traitantSIGTSTP(int);

int getNewId();

void printPlanesInfo();

int main(int, char **);

#endif //LO41_MAIN_H
