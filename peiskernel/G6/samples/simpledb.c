/** \file simpledb.c
    Implents a very simplistic scripting language to:

    - Read tuple from the tuplespace    
    - Loads tuples from a file an enter them into our own or someone elses tuples
     
	Also, remains as member of tuplespace until aborted.
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

/** \page simpledb Simpledb
	Loads tuples from a file an enter them into our own or someone elses tuples
	Also, can remain as member of tuplespace until aborted.

	See \ref simpledb.c
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include "peiskernel.h"

#define MAX_LEN 10240


void printUsage(FILE *stream,short argc,char **args) {
  fprintf(stream,"%s options:\n",args[0]);
  fprintf(stream," --help                 Print usage information\n");
  fprintf(stream," --load <file>          Load args from file\n");
  fprintf(stream," --set <key> <value>    Sets a tuple to default owner\n");
  fprintf(stream," --append <key> <value> Appends a tuple to default owner\n");
  fprintf(stream," --owner <id>           Sets default owner of tuples (default self)\n");
  fprintf(stream," --hold                 keep running (default quits after 3 seconds)\n");
  fprintf(stream," --wait <%%f seconds>    Specify a time to wait before continuing\n");
  fprintf(stream,"Note that although the --load options accepts '-' as stdin, it is not intended as a REPL\n\n");
  peisk_printUsage(stream,argc,args);
  exit(0);
}

int main(int argc,char **args) {
  int i,j;
  int owner=-1;     /* Default owner used when setting tuples */
  int hold=0;       /* If we should hold (stay alive) after settings tuples or quit */
  int cited;        /* True if we are currently processig a word within " mark's */
  if(argc > 1 && strcmp(args[1],"--help") == 0) printUsage(stderr,argc,args);

  int myArgc=argc;
  char *myArgs[4096];

  /* Initialize tuplespace */
  peiskmt_initialize(&argc,args);
  sleep(3);

  /* Copy commandline args (translating leading -- to nothing) */
  for(i=0;i<myArgc;i++) {
    if(strncmp(args[i],"--",2) == 0)
      myArgs[i]=strdup(args[i]+2);
    else myArgs[i]=strdup(args[i]);
  }
 
  /* Read all tuples from commandline */
  for(i=1;i<myArgc;i++) {
    fflush(stdout);
    if(strcmp(myArgs[i],"load") == 0) {
      i++;
      FILE *fp;
      if(strcmp(myArgs[i],"-") == 0) fp = stdin;
      else fp = fopen(myArgs[i],"rb");
      if(!fp) continue;
      i=i+1;
      int pos=i;
      while(!feof(fp)) {
	char c;
	char word[MAX_LEN];
	c=fgetc(fp);
	if(feof(fp)) break;
	/* Remove all leading comments */
	if(c == '#') do { c=fgetc(fp); } while(!feof(fp) && c != '\n');
	/* Remove all leading whitespace */
	else if(isspace(c)) { }
	else {
	  /* Read a word */
	  if(c == '\"') { cited=1; c=fgetc(fp); }
	  else cited=0;

	  for(j=0;j<MAX_LEN && !feof(fp);) {
	    if(feof(fp)) break;
	    if(c == '\\') { 
	      /* Backquoted character - treat next character litteraly */
	      c=fgetc(fp);
	      word[j++] = c;
	    } else if(c == '\"') {
	      /* It's a citation mark, might end a word */
	      if(cited) break;
	      else { fprintf(stderr,"Error - bad word %s\"",word); exit(0); }
	    } else if(isspace(c) && !cited) break;
	    else word[j++]=c;
	    c=fgetc(fp);
	  }
	  word[j++]=0;

	  /* Add it to the current position of the arguments list */
	  myArgc++;
	  for(j=pos+1;j<myArgc;j++) myArgs[j]=myArgs[j-1];
	  myArgs[pos]=strdup(word);
	  pos++;
	}
      }
      i=i-1;
    }
    else if(strcmp(myArgs[i],"set") == 0) {
      char *key = myArgs[++i];
      char *value = myArgs[++i];
      if(owner == -1) peiskmt_setTuple(key,strlen(value)+1,value);
      else peiskmt_setRemoteTuple(owner,key,strlen(value)+1,value,"fake/mime",PEISK_ENCODING_ASCII);
    } else if(strcmp(myArgs[i],"owner") == 0) {
      errno=0;
      owner=strtol(myArgs[++i],NULL,10);
      if(errno) { fprintf(stderr,"Error - bad owner string '%s'\n",myArgs[i]); owner=-1; }	  
    } else if(strncmp(myArgs[i],"peis-",5) == 0) {}
    else if(strcmp(myArgs[i],"append") == 0) {
      char *key = myArgs[++i];
      char *value = myArgs[++i];
      PeisTuple abstract;
      peiskmt_initTuple(&abstract);
      peiskmt_setTupleName(&abstract,key);
      if(owner == -1) abstract.owner = peisk_peisid();
      else abstract.owner = owner;
      /* TODO... should be MT!! */
      abstract.creator = -1;
      abstract.ts_write[0] = -1;
      abstract.ts_write[1] = -1;
      abstract.ts_expire[0] = -1;
      abstract.ts_expire[1] = -1;
      abstract.ts_user[0] = -1;
      abstract.ts_user[1] = -1;
      peiskmt_appendTupleByAbstract(&abstract,strlen(value)+1,value);
    }
    else if(strcmp(myArgs[i],"hold") == 0) hold=1;
    else if(strcmp(myArgs[i],"dont-hold") == 0) hold=0;
    else if(strcmp(myArgs[i],"wait") == 0) {
      float f;
      int ti;
      char *time = myArgs[++i];
      if(sscanf(time,"%f",&f) == 1) 
	usleep((int)f*1000000);
      else if(sscanf(time,"%d",&ti) == 1) 
	usleep(ti*1000000);
    }
    else {
      fprintf(stderr,"Error - unknown option '%s'\n",myArgs[i]);
    }
  }

  if(hold)
    while(peisk_isRunning) { usleep(100); }
  else 
    sleep(3);
  peiskmt_shutdown();
}
