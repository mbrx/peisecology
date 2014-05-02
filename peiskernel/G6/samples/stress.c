/** \file stress.c
    PEIS component for stress testing the network. Contains options for generating/receiving large amounts of data tuples.
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

/** \page stress Stress
    PEIS component for stress-testing the network. Contains options for generating/receiving large amounts of data tuples in short time intervals to provoke failures. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "peiskernel.h"

void printUsage(FILE *stream,short argc,char **args) {
  fprintf(stream,"%s options:\n",args[0]);
  fprintf(stream," --help                 Print usage information\n");
  fprintf(stream," --generate             Print usage information\n");
  fprintf(stream," --consume <owner>      Print usage information\n");
  fprintf(stream," --datasize <size>      Print usage information\n");
  fprintf(stream," --freq <float>         Print usage information\n");
  peisk_printUsage(stream,argc,args);
  exit(0);
}

int main(int argc,char **args) {
  int i;
  int isGenerating=0;
  int isConsuming=0;
  int owner=-1;
  int datasize=50*1024;
  char *data;
  float freq=1.0;

  int recvLen;
  char *recvData=NULL;
  PeisTuple *tuple;
  double totalLatency = 0.0;
  int recvCount;

  /* Read commandline arguments */
  if(argc > 1 && strcmp(args[1],"--help") == 0) printUsage(stderr,argc,args);
  for(i=1;i<argc;i++) {
	if(strcmp(args[i],"--generate") == 0) {
	  isGenerating=1;
	} else if(strcmp(args[i],"--datasize") == 0) {
	  i++;
	  datasize=atoi(args[i]);
	} else if(strcmp(args[i],"--consume") == 0) {
	  isConsuming=1;
	  i++;
	  owner=atoi(args[i]);
	  printf("Consuming data from: %d.foo\n",owner);
	} else if(strcmp(args[i],"--freq") == 0) {
	  i++;
	  sscanf(args[i],"%f",&freq);
	  printf("Freq=%f\n",freq);
	}
  }
  if(!(isGenerating || isConsuming)) exit(0);
  if(datasize < 100) datasize=100;
  if(freq<=0.0) exit(0);

  data=(char*)malloc(datasize);

  /* Initialize everything */
  peisk_initialize(&argc,args);
  peisk_setStringTuple("doQuit","no");

  /* Subscribe to all keys _and_ register callbacks */
  if(isConsuming) {
    peisk_subscribe(owner,"foo");
  }

  /* Loop until peiskernel has been shutdown (eg. by CtrlC signal) */
  i=0;
  double nextPkg=peisk_gettimef();
  while(peisk_isRunning()) { 
    if(isGenerating && peisk_gettimef() > nextPkg) {
      nextPkg += freq;
      i++;
      sprintf(data,"%d",i);
      printf("setting %d.foo to %d\n",peisk_peisid(),i);
      peisk_setTuple("foo",datasize,data,"x-application/unknown",PEISK_ENCODING_ASCII);
    }

    /*peisk_step(); usleep(0); */
    peisk_wait(1000);

    if(isConsuming) {
      tuple = peisk_getTuple(owner,"foo",0);
      recvData = tuple->data;
      recvLen = tuple->datalen;
      if(tuple) {
	recvData[20]=0;
	int t0,t1;
	peisk_gettime2(&t0,&t1);
	double latency = (t0-tuple->ts_write[0]) + 1e-6*(t1-tuple->ts_write[1]);
	totalLatency += latency;
	recvCount++;
	printf("Got package: %s, latency: %3.3fms, avgLatency: %3.3fms\n",recvData,
	       latency*1000.0,totalLatency*1000.0/recvCount);
	
      }
    }
  }
  peisk_shutdown();
}
