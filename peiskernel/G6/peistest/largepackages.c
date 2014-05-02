/** \file largepackages.c
   Test to send and receive large packages
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

#include <stdio.h>
#include <unistd.h>
#include "peiskernel.h"

#define DATA_TRANSFER_PORT 100

int receiveDataHook(int port,int destination,int sender,int datalen,void *data) {
  printf("Got package of length: %d\n",datalen);
  return 0;
}


int main(short argc,char **args) {
  int i;
  int isSender=0;
  int destination=8000;

  for(i=1;i<argc;i++) {
	if(strcmp(args[i],"--send") == 0) { isSender=1; destination=atoi(args[++i]); }
  }

  peisk_initialize(&argc,args);
  peisk_wait(3*1000000);

  peisk_registerHook(DATA_TRANSFER_PORT,receiveDataHook);

  if(isSender) {
	int size=2000000;
	void *buffer = malloc(size);

	while(peisk_isRunning) {
	  printf("Sending %d bytes\n",size);
	  peisk_sendMessage(DATA_TRANSFER_PORT,destination,size,buffer,PEISK_PACKAGE_REQUEST_ACK);	  
	  peisk_wait(5*1000000);   
	}
  } else
	while(peisk_isRunning)
	  peisk_wait(1000000);   
}
