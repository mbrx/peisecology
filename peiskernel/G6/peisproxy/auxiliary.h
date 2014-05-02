#ifndef AUXILIARY_H
#define AUXILIARY_H

#include <peiskernel/peiskernel_mt.h>

void printBuffer(unsigned char *buffer, int len, char *msg);
unsigned char charsToBinary(unsigned char a,unsigned char b);
PeisTuple *readTupleFromLocalSpace(char *strKey);
PeisTuple *readProxiedDataFromInterface(char *tupleKey, int interfaceOwnerID);


#endif
