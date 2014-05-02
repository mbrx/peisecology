/** \file peisprofiler.c
    Help for generating profiling data about the performance of the
    peiskernel. To use this, compile GCC profiling flags (ie. -g -pg),
    run through eg. gprof and analyse the results. This program is
    required since most of these tools to do handle profiling of
    linked libraries as well as profiling of directly linked object
    code.  
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

/** \page peisprofiler PeisProfiler
    Help for generating profiling data about the performance of the
    peiskernel. To use this, compile GCC profiling flags (ie. -g -pg),
    run through eg. gprof and analyse the results. This program is
    required since most of these tools to do handle profiling of
    linked libraries as well as profiling of directly linked object
    code.  
 */

#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include "peiskernel.h"

void printUsage(FILE *stream,short argc,char **args) {
  fprintf(stream,"peismaster options:\n");
  fprintf(stream," --help                 Print usage information\n");
  peisk_printUsage(stream,argc,args);
  /*exit(0);*/
}

void ballast() {
  int i;
  float a,b,c;
  a=1.0, b=1.0;
  for(i=0;i<1000;i++) {
    c=a+b;
    a=b; 
    b=c;
  }
}

int main(int argc,char **args) {
  int len; char *value;

  if(argc > 1 && strcmp(args[1],"--help") == 0) printUsage(stderr,argc,args);

  /* Initialize everything */
  peisk_initialize(&argc,args);

  /* Loop until peiskernel has been shutdown (eg. by CtrlC signal) */
  peisk_setStringTuple("do-quit","nil");

  peisk_wait(10*1000000);
  double t0 = peisk_gettimef();

  while(peisk_isRunning()) { 
    if(peisk_gettimef() - t0 > 60.0*10) break;
    /*ballast();*/
    peisk_wait(10000);
  }
  peisk_shutdown();
}
