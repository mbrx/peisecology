/** \file peistest.c
	Performs some simple tests on the PEIS network
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

/** \page peistest PeisTest
    This is an ugly hack to test some operations on the peis ecology and which also can be used
    to perform some usefull topology and service tests. Invoke as peistest --help for more information.
 */


#include <stdio.h>
#include <unistd.h>
#include "general.h"
#include "peiskernel.h"

double package_lastRequested;

void printUsage(FILE *stream,short argc,char **args) {
  fprintf(stream,"peismaster options:\n");
  fprintf(stream," --help                   Print usage information\n");
  fprintf(stream," --init-delay <time>      Sets the time to wait before sending request for topology\n");
  fprintf(stream," --topology-delay <time>  Sets the time to wait for all topology answers\n");
  fprintf(stream," --simple-nodes           Gives only the id of known hosts\n");
  fprintf(stream," --undirected             Draw undirected graph\n");
  fprintf(stream," --use-dot                Use dot for drawing\n");
  fprintf(stream," --use-circo              Use circo for drawing (default)\n");
  fprintf(stream," --traceroute <target>    Request traceroute to target\n");
  fprintf(stream," --no-auto                Disable automatic connection management\n");
  fprintf(stream," --upload                 Speed test by uploading data to downloaders\n");
  fprintf(stream," --download <addr>        Download data from given address and measure speed\n");
  fprintf(stream," --produce <key> <owner>  Produce values of the given key\n");
  fprintf(stream," --consume <key> <owner>  Subscribe to values of the given key\n");
  peisk_printUsage(stream,argc,args);
  exit(0);
}

#define DATA_TRANSFER_PORT 100

double maxPing=0.0, minPing=100.0, totPing=0.0;

int sendDataHook(int port,int destination,int sender,int datalen,void *data) {
  char buffer[1024];

  if(destination != peisk_knownHosts[0].id) return 0;
  peisk_sendMessage(DATA_TRANSFER_PORT,sender,1024,buffer,0);
}

int receiveDataHook(int port,int destination,int sender,int datalen,void *data) {
  double thisPing;

  if(destination != peisk_knownHosts[0].id) return 0;
  thisPing=peisk_gettimef() - package_lastRequested;
  totPing += thisPing;
  if(thisPing > maxPing) maxPing = thisPing;
  if(thisPing < minPing) minPing = thisPing;
  package_lastRequested=0.0;  
  return 0;
}

int main(int argc,char **args) {
  int i,j;
  int undirected=0;
  int use_circo=1;
  int traceTarget=-1;
  char *arrow="->";
  int useSimpleNodes=0;

  int initDelay=3;
  int topologyDelay=3;

  int do_upload=0;
  int do_download=0;
  int download_addr=0;

  char *produce_key, *consume_key;
  int do_produce=0, do_consume=0;
  int produce_owner, consume_owner;

  int cnt=0, ok=0, failed=0;

  double t0;
  char message[256]; 

  if(argc > 1 && strcmp(args[1],"--help") == 0) printUsage(stderr,argc,args);
  for(i=1;i<argc;i++) {
	if(strcmp(args[i],"--undirected") == 0) { undirected=1; arrow="--"; }
	else if(strcmp(args[i],"--simple-nodes") == 0) useSimpleNodes=1;
	else if(strcmp(args[i],"--use-dot") == 0) use_circo=0;
	else if(strcmp(args[i],"--use-circo") == 0) use_circo=1;
	else if(strcmp(args[i],"--traceroute") == 0) traceTarget=atoi(args[++i]);
	else if(strcmp(args[i],"--no-auto") == 0) peisk_useConnectionManager=0;
	else if(strcmp(args[i],"--init-delay") == 0) initDelay=atoi(args[++i]);
	else if(strcmp(args[i],"--topology-delay") == 0) topologyDelay=atoi(args[++i]);
	else if(strcmp(args[i],"--upload") == 0) do_upload=1;
	else if(strcmp(args[i],"--download") == 0) { do_download=1; download_addr=atoi(args[++i]); }
	else if(strcmp(args[i],"--produce") == 0) { do_produce=1; produce_key=args[++i]; produce_owner=atoi(args[++i]); }
	else if(strcmp(args[i],"--consume") == 0) { do_consume=1; consume_key=args[++i]; consume_owner=atoi(args[++i]); }
  }

  /* Initiailize everything */
  peisk_initialize(&argc,args);
  if(produce_owner == -1) produce_owner = peisk_id;

#ifdef OLD  
  if(do_upload) {
    printf("Running as a SERVER on address %d\n",peisk_knownHosts[0].id);
    peisk_registerHook(DATA_TRANSFER_PORT,sendDataHook);
    while(peisk_isRunning()) { peisk_step(); usleep(1000); }
    exit(0);
  } else if(do_download) {
    package_lastRequested=0.0;
    peisk_registerHook(DATA_TRANSFER_PORT,receiveDataHook);

    while(peisk_isRunning()) { 
      if(package_lastRequested == 0.0) {
	cnt++;
	ok++;
	printf("."); fflush(stdout);
	/* Send a new request for data since no request is outstanding */
	package_lastRequested=peisk_gettimef();
	peisk_sendMessage(DATA_TRANSFER_PORT,download_addr,0,NULL,0);
      } else if(package_lastRequested + 3.0 < peisk_gettimef()) {
		cnt++;
	failed++;
	printf("x"); fflush(stdout);
	package_lastRequested=peisk_gettimef();
	peisk_sendMessage(DATA_TRANSFER_PORT,download_addr,0,NULL,0);
      }
      if(cnt >= 40) { printf("ok: %d fail: %d max: %3.2fs min: %3.2fs avg: %3.2fs\n",ok,failed,maxPing,minPing,totPing/(ok?(double)ok:1.0)); maxPing=0.0; minPing=100.0; totPing=0.0; ok=0; failed=0; cnt=0; }
      peisk_step(); usleep(100); 
    }
    exit(0);
  } else if(do_produce || do_consume) {
	cnt=0;
	t0=peisk_gettimef();
	if(do_consume) {
	  printf("subscribe to %s from %d\n",consume_key,consume_owner);
	  peisk_subscribe(consume_key,consume_owner);
	}
	while(peisk_isRunning()) { 
	  peisk_step(); usleep(100); 
	  if(do_produce && peisk_gettimef() > t0 + 3.0) {
	    t0=peisk_gettimef();
	    snprintf(message,256,"%d",cnt++);
	    printf("SET %d.%s to %s\n",produce_owner,produce_key,message);
	    peisk_setRemoteTuple(produce_key,produce_owner,strlen(message)+1,message);
	  }
	  if(do_consume) {
	    int len, producer;
	    char *message;
		if(consume_owner == -1) {
		  if(peisk_getTuple(consume_key,consume_owner,&len,(void**)&message,0))
			printf("GOT %d.%s: %s\n",consume_owner,consume_key,message);
		} else {
		  int handle=0;
		  while(peisk_getTuples(consume_key,&producer,&len,(void**)&message,&handle,0))
			printf("GOT %d,%s: %s\n",producer,consume_key,message);
		}
	  }	  
	}
	exit(0);
  } 
#endif

  /* Wait a few seconds to let routing tables be updated properly */
  printf("waiting\n");
  peisk_wait(initDelay*1000000);
  /* Update topology */
  printf("update topology\n");
  peisk_updateTopology();
  printf("waiting\n");
  peisk_wait(topologyDelay*1000000);
  printf("done\n");

  /* Calculate how long time different peiskernel operations take */
#ifdef OLD
  t0=peisk_gettimef();
  for(i=0;i<20;i++) peisk_setTuple("Hello",6,"World");
  printf("setTuple: %3.4f ms\n",(peisk_gettimef()-t0)/20 * 1000.0);
  double t1=peisk_gettimef();
  int len;
  char *str;
  for(i=0;i<100;i++) peisk_getTuple("Hello",peisk_id,&len,&str,PEISK_TFLAG_OLDVAL);
  printf("getTuple: %3.4f ms\n",(peisk_gettimef()-t1)/100 * 1000.0);
#endif

  /* Send a traceroute request if applicable*/
  if(traceTarget != -1) {
    printf("PING!\n");
    peisk_sendTrace(traceTarget);
    printf("peisk_TraceResponse: %d (%x)\n",ntohl(peisk_traceResponse.hops),ntohl(peisk_traceResponse.hops));


    /* Wait for all answers to arrive */
    double t0 = peisk_gettimef();
    int tries=5;
    while(1) {
      if(!peisk_isRunning()) break;
      peisk_step();
      if(ntohl(peisk_traceResponse.hops) > 1) break;
      if(peisk_gettimef() > t0 + 10.0) {
	if(tries-- < 0) { printf("Giving up\n"); break; } 
	else {
	  printf("Trying again\n");
	  t0 = peisk_gettimef();
	  peisk_sendTrace(traceTarget);
	}
      }
      if(peisk_gettimef() > t0 + 60.0) break;
	usleep(1000);
    }
    printf("peisk_TraceResponse: %d (%x)\n",peisk_traceResponse.hops,peisk_traceResponse.hops);
    double pingTime=peisk_gettimef()-t0;
    printf("PING TIME: %f\n",pingTime);
  }

  /* Print topology */
  FILE *fp=fopen("graph.dot","wb");
  if(!fp) { printf("Failed to open graph.dot\n"); exit(0); }

  if(undirected)
    fprintf(fp,"graph {\n  mindist=1.0; splines=true;\n");
  else
    fprintf(fp,"digraph {\n  mindist=1.0; splines=true;\n");

  /* Give detailed information for all known hosts */
  if(useSimpleNodes) {
	for(i=0;i<PEISK_MAX_NODES;i++)
	  if(peisk_knownHosts[i].id != -1)
		fprintf(fp,"  %d;\n",peisk_knownHosts[i].id);
  } else {
	for(i=0;i<PEISK_MAX_NODES;i++)
	  if(peisk_knownHosts[i].id != -1) {
		fprintf(fp,"  %d [shape=record label=\"{%s",peisk_knownHosts[i].id,peisk_knownHosts[i].fullname);
		for(j=0;j<peisk_knownHosts[i].nLowlevelAddresses;j++)
		  fprintf(fp,"| %d.%d.%d.%d:%d",
				  peisk_knownHosts[i].lowAddr[j].addr.tcpIPv4.ip[0],peisk_knownHosts[i].lowAddr[j].addr.tcpIPv4.ip[1],
				  peisk_knownHosts[i].lowAddr[j].addr.tcpIPv4.ip[2],peisk_knownHosts[i].lowAddr[j].addr.tcpIPv4.ip[3],
				  peisk_knownHosts[i].lowAddr[j].addr.tcpIPv4.port);
		fprintf(fp,"}\"];\n");
	  }
  }

  for(i=0;i<PEISK_MAX_NODES;i++)
    if(peisk_topology[i].id != -1) {
      for(j=0;j<peisk_topology[i].nNeighbours;j++)		
		if(peisk_topology[i].id > peisk_topology[i].neighbours[j]) {
		  if(undirected) {			
			fprintf(fp,"  %d %s %d [style=bold];\n",peisk_topology[i].id,arrow,peisk_topology[i].neighbours[j]);
		  } else {
			fprintf(fp,"  %d %s %d [arrowhead=none,arrowtail=none,style=bold];\n",peisk_topology[i].id,arrow,peisk_topology[i].neighbours[j]);
		  }
		}
    }

  /* Add result of traceroute into graph */
  if(traceTarget != -1) {
	for(i=0;i<ntohl(peisk_traceResponse.hops)-1;i++) {
	  fprintf(fp,"%d %s %d [color=blue,weight=0];\n",ntohl(peisk_traceResponse.hosts[i]),arrow,ntohl(peisk_traceResponse.hosts[i+1]));
	}
	printf("TRACEROUTE to %d: ",traceTarget);
	for(i=0;i<ntohl(peisk_traceResponse.hops);i++) {
	  printf("%d ",ntohl(peisk_traceResponse.hosts[i]));
	}
	printf("\n");
  }

  fprintf(fp,"}\n");
  fclose(fp);
  if(use_circo)
    system("circo graph.dot -Tsvg -Nfontname=courier -Efontname=courier > graph.svg");
  else
	system("dot graph.dot -Tsvg -Nfontname=courier -Efontname=courier > graph.svg");
  if(fork()) system("kuickshow graph.png > /dev/null 2>/dev/null || gwenview graph.svg > /dev/null 2>/dev/null");
  /* Shutdown */
  peisk_shutdown();
  return 0;
}
