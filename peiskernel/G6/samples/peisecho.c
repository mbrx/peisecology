/** \file peisecho.c
    Copies stdin to stdout
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

/** \page peisecho PeisEcho
    Echo stdin to stdout, useful in some peisinit scripts
 */

#include <stdio.h>
#include "peiskernel.h"
#include "peiskernel_mt.h"

void printUsage(FILE *stream,short argc,char **args) {
  fprintf(stream,"%s options:\n",args[0]);
  fprintf(stream," --help                 Print usage information\n");
  peisk_printUsage(stream,argc,args);
  exit(0);
}

int main(int argc,char **args) {
  if(argc > 1 && strcmp(args[1],"--help") == 0) printUsage(stderr,argc,args);

  /* Initialize everything */
  peiskmt_initialize(&argc,args);

  while(1) {
    char c=fgetc(stdin);
    if(feof(stdin)) exit(0);
    printf("%c",c);
  }

}
