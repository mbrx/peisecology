/** \file tuple_reader2.c
    A test agent that reads some tuples and acts on them (prints them to the user). This is a second version demonstrating how
    callbacks can be used to access the tuples.
*/
/*
   Copyright (C) 2005 - 2011  Mathias Broxvall

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

/** \page tuple_reader2 tuple_reader2
    This is an alternative version of tuple_reader, a sample program
    which subscribes to tuples and print them to the screen. The
    difference between these two programs is that this one uses
    callbacks to read the tuples. Thus this illustrates of tuples can
    be guaranteed to read exactly once for each generated value (the
    other program might miss values if they are generated too
    quickly). 

    It is primarily intended as a naive example of how a minimalisitic
    PEIS component can look, but it can also sometimes be used to log
    tuples to a file. It can read multiple sets of tuples, with
    possible wildcards both inside the tuple names and the owner.  

    See also \ref tuple_reader
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "peiskernel.h"

void printUsage(FILE *stream,short argc,char **args) {
  fprintf(stream,"%s options:\n",args[0]);
  fprintf(stream," --help                 Print usage information\n");
  fprintf(stream," --tuple <owner> <key>  Specify a tuple to read\n");
  peisk_printUsage(stream,argc,args);
  exit(0);
}

void actCallback(PeisTuple *tuple, void *userdata) {
  char key[256];
  int owner = tuple->owner;
  void *data = tuple->data;

  if(peisk_getTupleName(tuple,key,sizeof(key))) 
    printf("error getting name of tuple\n");

  double t0=peisk_gettimef();
  static double last=0.0;
  //printf("%d.%s = %s (td=%f)\n",owner,key,(char*)data,t0-last);
  printf("%d.%s = %s (td=%f)\n",owner,key,(char*)data,t0-last);
  //printf("expire: %f\n",tuple->ts_expire[0]+1e-6*tuple->ts_expire[1]);
  last=t0;
}

int main(int argc,char **args) {
  int i;
  int ntuples=0;
  char *keys[64];
  int owners[64];
  
  /* Read commandline arguments */
  if(argc > 1 && strcmp(args[1],"--help") == 0) printUsage(stderr,argc,args);
  for(i=1;i<argc;i++) {
	if(strcmp(args[i],"--tuple") == 0) {
	  owners[ntuples]=atoi(args[++i]);
	  keys[ntuples]=args[++i];
	  ntuples++;
	}
  }
  if(ntuples == 0) { printf("No tuples specified!\n"); exit(0); }

  /* Initialize everything */
  peisk_initialize(&argc,args);
  peisk_wait(3*1000000);

  /* Subscribe to all keys _and_ register callbacks */
  for(i=0;i<ntuples;i++) {
    printf("Subscribing to: %d.%s\n",owners[i],keys[i]);
    peisk_subscribe(owners[i],keys[i]);
    peisk_registerTupleCallback(owners[i],keys[i],NULL,actCallback);
  }

  /* Loop until peiskernel has been shutdown (eg. by CtrlC signal) */
  while(peisk_isRunning) { 
    peisk_wait(100000);
  }
}
