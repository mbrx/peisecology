/** \file tuple_generator.c
    A test agent that produces fake tuples 
*/
/*
   Copyright (C) 2005-2011  Mathias Broxvall

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

/** \page TupleGenerator Tuple generator
    A test program that produces fake sensor readings (tuples). Also
    servers as a minimalisitic example of how to use the
    PeisKernel. It also demonstartes the use of the with_ack_hook
    functionality for receiving callbacks when a tuple have been
    received or when a timeout have been received, when sending tuples
    to receipients. 
 */

#include <stdio.h>

#include <stdio.h>
#include <getopt.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/time.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/select.h>
#include <math.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "peiskernel.h"
#include "peiskernel_private.h"


void printUsage(FILE *stream,short argc,char **args) {
  fprintf(stream,"%s <tuplename> options:\n",args[0]);
  fprintf(stream," --help                 Print usage information\n");
  peisk_printUsage(stream,argc,args);
  exit(0);
}

void tuple_acked(int success,int dataLen,PeisPackage *package,void *user) {
  /* You can use this code to get an acknowledgement whenever some
     PEIS have received one of the tuples produces by this component
  */ 

  /*printf("%d %s len: %d\n",success,package->data,dataLen);
    peisk_hexDump(package->data,dataLen);*/
}

int main(int argc,char **args) {
  char *tuplename;
  int len; char *value;
  double freq=1.0;

  /* If you set this to 1 then we will use indirect writes instead */
  int indirect=0;

  if(argc < 2) printUsage(stderr,argc,args);
  tuplename = args[1];
  if(argc > 1 && strcmp(args[1],"--help") == 0) printUsage(stderr,argc,args);
  if(argc > 2) freq = atof(args[2]);

  /* Initialize everything */
  peisk_initialize(&argc,args);

  /* See comment below */
  peisk_setStringTuple("do-quit","nil");

  /* Loop until peiskernel has been shutdown (eg. by CtrlC signal) */
  double t0=peisk_gettimef();
  int cnt=0;

  peisk_setStringTuple(tuplename,"");
  while(peisk_isRunning()) { 
    peisk_step(); usleep(100); 
    if(peisk_gettimef() > t0 + freq) {
      char str[256];
      //snprintf(str,sizeof(str),"data-%d:%d",peisk_id,cnt++);
      snprintf(str,sizeof(str),"%d ",cnt++);
      if(indirect)
	peisk_setIndirectStringTuple(tuplename,peisk_id,str);
      else {
	/* The with_ack_hook part is only needed if you need to
	receive acknowledgements when some have received or given up
	on recveiving tuples that you produce. */
	/*with_ack_hook(tuple_acked,NULL,{
	    peisk_setStringTuple(tuplename,str);
	    });*/
	peisk_appendStringTuple(peisk_id,tuplename,str);	
      }
      t0 += freq;
    }
  }
  peisk_shutdown();
}
