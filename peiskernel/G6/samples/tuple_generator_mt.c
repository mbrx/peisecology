/** \file tuple_generator_mt.c
    A test agent that produces sensor readings accessible for anyone using the multithreaded version of the peiskernel.
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

/** \page TupleGeneratorMT Multithreaded tuple generator
    A test program that produces fake sensor readings (tuples). Also
    servers as a minimalisitic example of how to use the
    Multithreaded version of the PeisKernel. 
 */

#include <stdio.h>
#include "peiskernel.h"
#include "peiskernel_mt.h"

void printUsage(FILE *stream,int argc,char **args) {
  fprintf(stream,"%s options:\n",args[0]);
  fprintf(stream," --help                 Print usage information\n");
  peisk_printUsage(stream,argc,args);
  exit(0);
}

int main(int argc,char **args) {
  if(argc > 1 && strcmp(args[1],"--help") == 0) printUsage(stderr,argc,args);

  /* Initialize everything */
  peiskmt_initialize(&argc,args);

  /* Loop until peiskernel has been shutdown (eg. by CtrlC signal) */
  double t0=peisk_gettimef();
  int cnt=0;
  while(peiskmt_isRunning()) { 
      /*peisk_step(); usleep(100); */
      usleep(10);
    if(peisk_gettimef() > t0 + 1.0) {
      char str[256];
      snprintf(str,sizeof(str),"data-%d:%d",peisk_peisid(),cnt++);
      peiskmt_setTuple("sensor",strlen(str)+1,str,"text/plain",PEISK_ENCODING_ASCII);
      t0 += 1.0; // = peisk_gettimef();
    }
  }
}
