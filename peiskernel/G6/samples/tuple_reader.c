/** \file tuple_reader.c
    A test agent that reads some tuples and acts on them (prints them
    to the user)

    This is a suitable example for how to get multiple tuples using
    abstract prototypes using the new PeisKernel G4 
*/
/*
   Copyright (C) 2005  Mathias Broxvall

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/** \page tuple_reader tuple_reader
    This is a sample program which subscribes to tuples and print them
    to the screen. Note that this is a naive implementation that uses
    tuple reads instead of callbacks, thus it may miss values of
    tuples that are changed too quickly.

    It is primarily intended as a naive example of how a minimalisitic
    PEIS component can look, but it can also sometimes be used to log
    tuples to a file. It can read multiple sets of tuples, with
    possible wildcards both inside the tuple names and the owner.  

    See also \ref tuple_reader2
 */

#include <stdio.h>
#include <stdlib.h>
#include "peiskernel.h"

void printUsage(FILE *stream,short argc,char **args) {
  fprintf(stream,"%s options:\n",args[0]);
  fprintf(stream," --help                    Print usage information\n");
  fprintf(stream," --tuple <owner> <key>     Specify a tuple to read\n");
  fprintf(stream," --indirect <owner> <key>  Specify an indirect tuple to read\n");
  peisk_printUsage(stream,argc,args);
  exit(0);
}

int main(int argc,char **args) {
  int i;
  int ntuples=0;
  char *message;
  int len;
  PeisTupleResultSet *rs;
  PeisTuple prototype, *tuple;  
  char str[256];
  char *value;

  /* variables to hold the names etc. of tuples to read */
  char *keys[64];
  int owners[64];
  int indirect[64];

  /* Read commandline arguments */
  if(argc > 1 && strcmp(args[1],"--help") == 0) printUsage(stderr,argc,args);
  for(i=1;i<argc;i++) {
    if(strcmp(args[i],"--tuple") == 0) {
      owners[ntuples]=atoi(args[++i]);
      keys[ntuples]=args[++i];
      indirect[ntuples]=0;
      ntuples++;
    }
    else if(strcmp(args[i],"--indirect") == 0) {
      owners[ntuples]=atoi(args[++i]);
      keys[ntuples]=args[++i];
      indirect[ntuples]=1;
      ntuples++;
    }
  }

  if(ntuples == 0) { printf("No tuples specified!\n"); exit(0); }

  /* Initialize everything */
  peisk_initialize(&argc,args);

  /* See comment below */
  peisk_setStringTuple("do-quit","nil");

  /* Subscribe to all keys */
  for(i=0;i<ntuples;i++) {
    if(indirect[i] == 0) {
      printf("Subscribing to: %d.%s\n",owners[i],keys[i]);
      peisk_subscribe(owners[i],keys[i]);
    } else {
      printf("Creating meta subscription: %d.%s\n",owners[i],keys[i]);
      peisk_subscribeIndirectly(owners[i],keys[i]);
    }
  }

  /* Loop until peiskernel has been shutdown (eg. by CtrlC signal) */
  rs = peisk_createResultSet();



  while(peisk_isRunning()) { 
    peisk_wait(1000);

    /* All normal tuples are collected allowing wildcards etc. Hence
       the use of a resultSet */

    peisk_resultSetReset(rs);
    /* Find all tuples which we are looking for */
    for(i=0;i<ntuples;i++) {
      if(indirect[i] == 0) {
	peisk_initAbstractTuple(&prototype);
	peisk_setTupleName(&prototype,keys[i]);
	prototype.owner = owners[i];
	prototype.isNew = 1;  /* Only give us tuples which are fresh */
	peisk_getTuplesByAbstract(&prototype,rs);
      }
    }
    /* And now print them */
    for(;peisk_resultSetNext(rs);) {
      tuple = peisk_resultSetValue(rs);
      peisk_getTupleName(tuple,str,sizeof(str));
      printf("%d.%s = %s\n",tuple->owner,str,tuple->data);
    }
   
    /* For all indirect tuples, we get them one by one and print
       immediatly */
    for(i=0;i<ntuples;i++)
      if(indirect[i] == 1) {
	tuple = peisk_getTupleIndirectly(owners[i],keys[i],0);
	if(tuple) {
	  printf("[%d.%s] = %s\n",owners[i],keys[i],tuple->data);
	}
      }   
  }
  peisk_shutdown();
}
