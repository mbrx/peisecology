/** \file peismaster.c
    Main file for the peismaster.
*/
/*
   Copyright (C) 2005 - 2012  Mathias Broxvall

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

/** \page peismaster PeisMaster
    This is a simple program that acts as a single node in the peis ecology. It provides the basic peis services
    (routing, ping responces etc.) but does not implement any additional services. The sourcecode for it is very simple
    and well suited for demonstrated what is required to participate in the peis ecology. See peismaster.c
 */

#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include "peiskernel.h"

void printUsage(FILE *stream,short argc,char **args) {
  fprintf(stream,"peismaster options:\n");
  fprintf(stream," --help                 Print usage information\n");
  peisk_printUsage(stream,argc,args);
  exit(0);
}

int main(int argc,char **args) {
  int len; char *value;

  if(argc > 1 && strcmp(args[1],"--help") == 0) printUsage(stderr,argc,args);

  /* Initialize everything */
  peisk_initialize(&argc,args);
  /* Example of how to setup and subscribe to input tuples */
  /*
    peisk_declareMetaTuple(peisk_peisid(),"mi-input");
    peisk_subscribeIndirectly(peisk_peisid(),"mi-input");
  */

  /* Loop until peiskernel has been shutdown (eg. by CtrlC signal) */
  while(peisk_isRunning()) { 
    peisk_wait(1000000);
  }
  peisk_shutdown();
}
