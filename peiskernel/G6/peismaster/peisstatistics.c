/** \file peisstatistics.c
    Collects and prints some statistics information for the current ecology.
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

/** \page peisstatitics PeisStatistics
    Used for logging and printing some statistics information.
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include "peiskernel.h"
#include "tuples.h"


void printUsage(FILE *stream,short argc,char **args) {
  fprintf(stream,"peisstatistics options:\n");
  fprintf(stream," --help                 Print usage information\n");
  peisk_printUsage(stream,argc,args);
  exit(0);
}

int main(int argc,char **args) {
  int len; char *value;
  PeisTupleResultSet *rs;
  PeisTuple prototype;
  PeisTuple *tuple;
  char *str;
  int id, in, out;
  float drop;

  int numPeis, numConnections;
  int thisIn, thisOut;
  int totalIn, totalOut;
  int maxIn, maxOut;
  double maxDrop, avgDrop, totalDrop;

  double smoothIn, smoothOut;

  if(argc > 1 && strcmp(args[1],"--help") == 0) printUsage(stderr,argc,args);

  /* Initialize everything */
  peisk_initialize(&argc,args);
  rs = peisk_createResultSet();

  /* Setup a prototype tuple to subscribe to */
  peisk_initAbstractTuple(&prototype);
  prototype.owner = -1;
  peisk_setTupleName(&prototype,"kernel.connections");
  peisk_subscribeByAbstract(&prototype);

  smoothIn = 0.0;

  /* Loop until peiskernel has been shutdown (eg. by CtrlC signal) */
  peisk_setStringTuple("do-quit","nil");
  while(peisk_isRunning()) { 
    peisk_wait(3000000);

    maxIn = 0;
    maxOut = 0;
    totalIn = 0;
    totalOut = 0;
    maxDrop = 0.0;
    avgDrop = 0.0;
    numPeis = 0;
    numConnections=0;

    /* Iterate over all PEIS that have given us connection tuples and compute statistics */
    peisk_resultSetReset(rs);
    peisk_getTuplesByAbstract(&prototype,rs);
    for(peisk_resultSetFirst(rs);peisk_resultSetNext(rs);) {
      numPeis++;
      tuple = peisk_resultSetValue(rs);
      str = tuple->data;
      /* Parse the connection string */
      if(*str != '(') { printf("bad string: %s\n",tuple->data); continue; }
      str++;
      thisIn = 0; thisOut=0;
      while(*str != ')') {
	while(*str == ' ' || *str == '\n') str++;
	if(*str != '(' || sscanf(str,"(%d %d %d %f)",&id,&in,&out,&drop) != 4) { 
	  printf("bad string: %s at pos: %d '%s'\n",tuple->data,str-tuple->data,str); continue; 
	}
	/* Iterate  */
	while(*str != ')') str++; str++; 
	while(*str == ' ' || *str == '\n') str++;

	thisIn += in;
	thisOut += out;
	numConnections++;
	totalDrop += drop;
	if(drop > maxDrop) maxDrop = drop;
      }
      if(thisIn > maxIn) maxIn = thisIn;
      if(thisOut > maxOut) maxOut = thisOut;
      totalIn += thisIn;
      totalOut += thisOut;
    }
    if(numConnections > 0) {
      smoothIn = smoothIn * 0.9 + (0.1 * totalIn)/numPeis;
      smoothOut = smoothIn * 0.9 + (0.1 * totalOut)/numPeis;

      printf("Total in:   %6d bytes, out: %6d bytes, peis: %3d connections: %3d\n",
	     totalIn,totalOut,numPeis,numConnections);
      printf("Max in:     %6d bytes, out: %6d bytes, drop: %3.2f%%\n",maxIn,maxOut,maxDrop*100.0);
      printf("Average in: %6d bytes, out: %6d bytes, drop: %3.2f%% connections: %3.1f\n",totalIn/numPeis,totalOut/numPeis,totalDrop/numConnections,((double)numConnections)/numPeis);
      printf("Smooth in:  %6.0f bytes, out: %6.0f bytes\n",smoothIn,smoothOut);
      printf("\n");
    }
  }
  peisk_shutdown();
}
