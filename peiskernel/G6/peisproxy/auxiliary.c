#include <stdio.h>
#include <peiskernel/peiskernel_mt.h>
#include <stdlib.h>
#include <string.h>
#include "auxiliary.h"

void printBuffer(unsigned char *buffer, int len, char* msg){
  int i;

  printf("%s:",msg);
  
  for (i=0;i<len; i++){
    printf("%02x ", buffer[i]);
  }
  printf("\n");
}


unsigned char charsToBinary(unsigned char a,unsigned char b) {
  int res=0;
  if(a >= '0' && a <= '9') res = (a - '0') << 4;
  else if(a >= 'A' && a <= 'F') res = (a - 'A' + 10) << 4;  
  else if(a >= 'a' && a <= 'f') res = (a - 'a' + 10) << 4;

  if(b >= '0' && b <= '9') res |= (b - '0');
  else if(b >= 'A' && b <= 'F') res |= (b - 'A' + 10);  
  else if(b >= 'a' && b <= 'f') res |= (b - 'a' + 10);

  return res;
}

PeisTuple *readTupleFromLocalSpace(char *strKey){

  PeisTuple prototype, *tuple;
  
  peiskmt_initAbstractTuple(&prototype);
  peiskmt_setTupleName(&prototype,strKey);
  prototype.owner = peiskmt_peisid();
  prototype.isNew = -1; 
  
  tuple=peiskmt_getTupleByAbstract(&prototype);

  return tuple;

}

PeisTuple *readProxiedDataFromInterface(char *tupleKey, int owner){
  char strKey[256];
  PeisTuple prototype, *tuple;
  
  
  strcpy(strKey,tupleKey);
  
  peiskmt_initAbstractTuple(&prototype);
  peiskmt_setTupleName(&prototype,strKey);
  prototype.owner = owner;
  prototype.isNew =-1; 
  
  tuple=peiskmt_getTupleByAbstract(&prototype);
  
  return tuple;
}
