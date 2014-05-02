/** \file services.c
    Implements most builtin services in the peis kernel
 */
/*
    Copyright (C) 2005 - 2012  Mathias Broxvall

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA
*/

#include <stdio.h>
#include <getopt.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <tuples.h>
#include <tuples_private.h>

#define PEISK_PRIVATE
#include "peiskernel.h"
#include "peiskernel_tcpip.h"


#define pmin(x,y) ((x)<(y)?(x):(y))

void peisk_registerDefaultServices() {
  /* Clear buffer for long messages from outdated partial messages */
  peisk_registerPeriodic(PEISK_TIMEOUT_LONG_MESSAGE/5.0,NULL,peisk_periodic_clearAssemblyBuffers);

  /* Acknowledgements of important packages */
  peisk_registerPeriodic(0.2,NULL,peisk_sendAcknowledgementsNow);
  peisk_registerHook(PEISK_PORT_ACKNOWLEDGEMENTS,peisk_hook_acknowledgments);

  /* Handle queries for hostInfo structures */
  peisk_registerHook(PEISK_PORT_QUERY_HOST,peisk_hook_queryHostInfo);
  peisk_registerHook(PEISK_PORT_HOSTINFO,peisk_hook_hostInfo);

  /* Update routing tables periodically */
  peisk_registerPeriodic(PEISK_INET_BROADCAST_PERIOD,NULL,peisk_periodic_inet_broadcast);
  peisk_registerPeriodic(PEISK_ROUTE_BROADCAST_PERIOD,NULL,peisk_periodic_routing);
  peisk_registerPeriodic(PEISK_FLUSH_PERIOD,NULL,peisk_periodic_flushPipes);
  peisk_registerHook(PEISK_PORT_ROUTING,peisk_hook_routing);

  /* Synchronize timing information periodically */
  peisk_registerPeriodic(PEISK_TIMESYNC_PERIOD,NULL,peisk_periodic_timeSync);
  peisk_registerHook(PEISK_PORT_TIMESYNC,peisk_hook_timeSync);

  /* Dead host service */
  peisk_registerHook(PEISK_PORT_DEAD_HOST,peisk_hook_deadHost);

  /* Traceroute services */
  peisk_registerHook(PEISK_PORT_TRACE,peisk_hook_trace);
  peisk_registerHook(PEISK_PORT_TRACE_REPLY,peisk_hook_trace);

  /* Connection manager */
  peisk_registerPeriodic(PEISK_CONNECTION_MANAGER_PERIOD,NULL,peisk_periodic_connectionManager);
  peisk_registerPeriodic(PEISK_CONNECT_CLUSTER_PERIOD,NULL,peisk_periodic_connectCluster);

  /* Debug information */
  peisk_registerPeriodic(1.0,NULL,peisk_periodic_debug);

  /* Manage speed control on connections */
  peisk_registerPeriodic(PEISK_CONNECTION_CONTROL_PERIOD,NULL,peisk_periodic_connectionControl1);
  peisk_registerPeriodic(PEISK_CONNECTION_CONTROL_PERIOD_2,NULL,peisk_periodic_connectionControl2);
  peisk_registerHook(PEISK_PORT_UDP_SPEED,peisk_hook_udp_speed);

  /* Kernel information */
  peisk_registerPeriodic(PEISK_KERNELINFO_PERIOD,NULL,peisk_periodic_kernelInfo);

  /* Tuplespaces */
  /* Adds more services and hook than what is defined here */
  peisk_tuples_initialize();
}

void peisk_registerDefaultServices2() {
  peisk_registerTupleCallback(peisk_peisid(),"kernel.do-quit",NULL,peisk_callback_kernel_quit);
}

/******************************************************************************/
/*                                                                            */
/* SERVICE: kernelInfo                                                        */
/*                                                                            */
/* STATUS: Working                                                            */
/*                                                                            */
/* PORTS:                                                                     */
/* VARIABLES:                                                                 */
/*                                                                            */
/*                                                                            */
/******************************************************************************/

int peisk_in_packages[50];
int peisk_out_packages[50];

extern int peisk_printPortStatistics;
void peisk_periodic_kernelInfo(void *data) {
  char str[8192];
  char *s;
  int i,a;
  PeisConnection *connection;

  snprintf(str,sizeof(str),"%.4f",peiskernel.avgStepTime);
  peisk_setStringTuple("kernel.step-time",str);

  static double avgTraffic[2]={0.0,0.0};
  avgTraffic[0] = avgTraffic[0]*0.9+0.1*peiskernel.incomingTraffic/2.0;
  avgTraffic[1] = avgTraffic[1]*0.9+0.1*peiskernel.outgoingTraffic/2.0;
  snprintf(str,sizeof(str),"(%.1f %.1f)",avgTraffic[0],avgTraffic[1]);
  peisk_setStringTuple("kernel.traffic",str);
  peiskernel.incomingTraffic=0;
  peiskernel.outgoingTraffic=0;


  /* Iterate over all connections and print statistics information for
     them. */
  s=str;
  sprintf(s,"("); s+=strlen(s);
  for(i=0,a=0;i<PEISK_MAX_CONNECTIONS;i++)
    if(peiskernel.connections[i].id != -1) {
      connection = &peiskernel.connections[i];
      sprintf(s,"(%d %d %d %d %2.2f)",connection->neighbour.id,
	      (int)(connection->totalIncomming/PEISK_KERNELINFO_PERIOD),
	      (int) (connection->totalOutgoing/PEISK_KERNELINFO_PERIOD),
	      connection->type, 
	      connection->estimatedPacketLoss);
      connection->totalOutgoing = 0;
      connection->totalIncomming = 0;
      s+=strlen(s);
      a++;
      if((a % 5) == 0) { sprintf(s,"\n"); s++; }
      if(s >= str+8000) break;
    }
  sprintf(s,")"); s+=strlen(s);
  *s = 0;
  peisk_setStringTuple("kernel.connections",str);


  if(peisk_printPortStatistics) {
    static int cnt=0;
    int tot;
    if(cnt++ % 5 == 0) {
    printf("IN:  ");
    for(i=0,tot=0;i<15;i++) {
      printf("%d: %03d ",i,peisk_in_packages[i]);
      tot += peisk_in_packages[i];
    }
    printf("tot: %d\n",tot);
    printf("OUT: ");
    for(i=0,tot=0;i<15;i++) {
      printf("%d: %03d ",i,peisk_out_packages[i]);
      tot += peisk_out_packages[i];
    }
    printf("tot: %d\n",tot);
    for(i=0;i<15;i++) {
      peisk_in_packages[i]=0;
      peisk_out_packages[i]=0;
    }
    for(i=0,tot=0;i<PEISK_MAX_CONNECTIONS;i++)
      if(peiskernel.connections[i].id != -1) tot++;
    printf("Connections: %d\n",tot);
    }
  }
}

/******************************************************************************/
/*                                                                            */
/* SERVICE: Debug                                                             */
/*                                                                            */
/* STATUS: Working                                                            */
/*                                                                            */
/* PORTS:                                                                     */
/* VARIABLES: debugRoutes                                                     */
/*                                                                            */
/*                                                                            */
/******************************************************************************/

int peisk_debugRoutes=0;

void peisk_periodic_debug(void *data) {
  int i,j,cnt;
  PeisQueuedPackage *qpackage;
  PeisQueuedPackage **prevQPackage;
  double t0=peisk_gettimef();
  PeisHashTableIterator iterator;
  PeisRoutingInfo *routingInfo;
  intA id;

  /* Check for sanity of connection structures */
  for(i=0;i<PEISK_MAX_CONNECTIONS;i++) {
    if(peiskernel.connections[i].id == -1) continue; /* This connection is unused */
    /* Check last pointer and total package count on all connection queues */
    for(j=0;j<PEISK_NQUEUES;j++) {
      cnt=0;
      for(qpackage=peiskernel.connections[i].outgoingQueueFirst[j],cnt=0,prevQPackage=&peiskernel.connections[i].outgoingQueueFirst[j];
	  qpackage;
	  prevQPackage=&qpackage->next,qpackage=qpackage->next,cnt++) { }
      PEISK_ASSERT(cnt==peiskernel.connections[i].nQueuedPackages[j],
		   ("error in package count, queue=%d, real cnt=%d, con cnt=%d\n",j,cnt,peiskernel.connections[i].nQueuedPackages[j]));
      peiskernel.connections[i].nQueuedPackages[j] = cnt;
    }
  }

  if(peisk_debugRoutes) {

    printf("Cluster: %d\n",peiskernel.hostInfo.networkCluster);

    /* Print out connections for debugging */
    printf("Connections { \n");
    for(i=0;i<PEISK_MAX_CONNECTIONS;i++)
      if(peiskernel.connections[i].id != -1)
	printf("  conn=%d, id=%d, neighbour=%d, age=%3.2f\n",i,peiskernel.connections[i].id,peiskernel.connections[i].neighbour.id,t0-peiskernel.connections[i].timestamp);
    printf("}\n");

    /* Print out routing tables */
    printf("Routing {\n");
    for(peisk_hashTableIterator_first(peiskernel.routingTable,&iterator);peisk_hashTableIterator_next(&iterator);) {
      if(peisk_hashTableIterator_value_generic(&iterator,&id,&routingInfo)) {
	printf("  id: %5d hops: %d connection: %d\n",routingInfo->id,routingInfo->hops,routingInfo->connection?routingInfo->connection->id:-1);
      } else {
	printf("  Error, invalid entry in routing table\n");
      }
    }

    printf("}\n");
  }

}

/******************************************************************************/
/*                                                                            */
/* SERVICE: Flush stdout/stderr                                               */
/*                                                                            */
/* STATUS: Working                                                            */
/*                                                                            */
/* PORTS:                                                                     */
/* VARIABLES:                                                                 */
/*                                                                            */
/*                                                                            */
/******************************************************************************/

void peisk_periodic_flushPipes(void *data) {
  /* Fix to get the pipes connected to inittab to be flushed
     continously. Question: how much CPU is wasted because of this? */
  fflush(stdout);
  fflush(stderr);
}

/******************************************************************************/
/*                                                                            */
/* SERVICE: Routing                                                           */
/*                                                                            */
/* STATUS: Working                                                            */
/*                                                                            */
/* PORTS: *_ROUTING                                                           */
/* VARIABLES: peisk_knownHosts, peisk_sequenceNo                              */
/*                                                                            */
/*                                                                            */
/******************************************************************************/

int peisk_sequenceNo=0;

/* Clusters:
   1. Each PEIS have a cluster number that is the lowest PeisID that it can route to.
   2. This cluster number is used in all broadcasted (multicasted)
   knownHost information
   -> We always know our "correct" cluster ID (check the routing table)
   -> Everyone that is visible (knownHosts) to us, but currently
   routable either (a) have a correct cluster ID, or (b) will soon be
   deleted. (a) if they are broadcasting (lowest ID on their machine)
   or (b) if we have recently lost the route to them (they will be
   deleted soon).

   Connecting clusters: If we know of any PEIS with a cluster ID that
   is not the same as ours, and if they are not routable then try to
   connect to them.
*/

void peisk_checkClusters() {
  int clusterId;
  PeisHashTableIterator iterator;
  PeisRoutingInfo *routingInfo;
  int destination;

  /* Compute which clusterId we belong to */
  clusterId = peiskernel.id;
  peisk_hashTableIterator_first(peiskernel.routingTable,&iterator);
  while(peisk_hashTableIterator_next(&iterator)) {
    peisk_hashTableIterator_value_fast(&iterator,&destination,&routingInfo);
    if(routingInfo->hops < 254 && destination < clusterId) clusterId = destination;
  }
  if(clusterId != peiskernel.hostInfo.networkCluster) {
    peiskernel.hostInfo.networkCluster=clusterId;

    if (peisk_printLevel & PEISK_PRINT_STATUS)
      printf("Updated cluster ID to %d, re-broadcasting\n",clusterId);

    /*peisk_periodic_routing(NULL);*/
    peisk_periodic_inet_broadcast(NULL);
  }
}

void peisk_periodic_inet_broadcast(void *data) {
  static int sock=-1;
  static struct sockaddr_in saddr;
  static struct in_addr iaddr;
  int status;
  unsigned char ttl = 3;
  unsigned char one = 1;
  PeisUDPBroadcastAnnounceMessage message;

  /* Keep socket open after first successfull creation. */
  if(sock < 0) {

    /* Broadcast our hostinfo on the MULTICAST socket */
    memset(&saddr, 0, sizeof(struct sockaddr_in));
    memset(&iaddr, 0, sizeof(struct in_addr));
    sock=socket(PF_INET, SOCK_DGRAM, 0);
    if(sock<0) {
      perror("Error creating multicast socket");
      return;
    }

    saddr.sin_family = PF_INET;
    saddr.sin_port = htons(0);                 /* Use any port */
    saddr.sin_addr.s_addr = htonl(INADDR_ANY); /* and any interface */
    status = bind(sock, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));

    /* use default interface */
    iaddr.s_addr = INADDR_ANY;
    /* Set outgoing interface to default */
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, &iaddr, sizeof(struct in_addr));
    /* send multicast traffic to include ourselves */
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP,&one, sizeof(one));
    /* Set multicast packet TTL (3), default is too limited (1) */
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
    /* set destination multicast address */
    saddr.sin_family = PF_INET;
    saddr.sin_addr.s_addr = inet_addr(PEISK_MULTICAST_IP);
    saddr.sin_port = htons(PEISK_MULTICAST_PORT);
  }

  if(peiskernel.broadcastingCounter < 3) {
    /* Increment counter, when it reaches 3 we will start broadcasting.
       If the acceptBroadcasts function seens any other component from our
       host and with a lower peis ID number, then reset counter to zero. 
    */
    peiskernel.broadcastingCounter++;
  } else {

  /* Prepare broadcast package */
  message.protocollVersion = htonl(peisk_protocollVersion);
  memset((void*)message.networkString,0,sizeof(message.networkString));
  strcpy(message.networkString,peisk_networkString);
  message.hostinfo = peiskernel.hostInfo;
  peisk_hton_hostInfo(&message.hostinfo);

  status = sendto(sock, (void*)&message, sizeof(message), MSG_DONTWAIT,(struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
  /*printf("Sending message: \n");
    peisk_hexDump((void*)&message,sizeof(message));*/

  }

  /*
  shutdown(sock, 2);
  close(sock); */

}

typedef struct PeisHostQueryPackage {
  /** The source of the query */
  int source;
  /** Used when query is a confirmation that this specific instance of
      the host is currently reachable, otherwise -1 */
  int magic;
} PeisHostQueryPackage;

/** Sends a query to the given destination to find his host
    information. */
void peisk_queryHostInfo(int destination) {
  PeisHostQueryPackage query;
  PeisHostInfo *hostInfo;
  /*if (peisk_printLevel & PEISK_PRINT_WARNINGS)    */
  /*printf("querying %d - does he exist?\n",destination);*/
  query.source = htonl(peiskernel.id);
  hostInfo = peisk_lookupHostInfo(destination);
  if(hostInfo)
    query.magic = htonl(hostInfo->magic);
  else
    query.magic = -1;
  peisk_sendMessage(PEISK_PORT_QUERY_HOST,destination,sizeof(PeisHostQueryPackage),(void*)&query,0);
}
void peisk_sendHostInfo(int destination,PeisHostInfo *hostInfo) {
  PeisHostInfoPackage package;

  package.isCached = (hostInfo->id == peiskernel.id)?0:1;
  package.hostInfo = *hostInfo;
  peisk_hton_hostInfo(&package.hostInfo);

  peisk_sendMessage(PEISK_PORT_HOSTINFO,destination,sizeof(PeisHostInfoPackage),(void*)&package,PEISK_PACKAGE_RELIABLE);
}
void peisk_sendLinkHostInfo(PeisConnection *connection) {
  PeisHostInfoPackage package;
  package.isCached = 0;
  package.hostInfo = peiskernel.hostInfo;
  peisk_hton_hostInfo(&package.hostInfo);

  peisk_sendLinkPackage(PEISK_PORT_HOSTINFO,connection,sizeof(PeisHostInfoPackage),(void*)&package);
}
void peisk_broadcastHostInfo() {
  PeisHostInfoPackage package;

  package.isCached=0;
  package.hostInfo = peiskernel.hostInfo;
  peisk_hton_hostInfo(&package.hostInfo);

  peisk_broadcast(PEISK_PORT_HOSTINFO,sizeof(PeisHostInfoPackage),(void*)&package);
}
int peisk_hook_queryHostInfo(int port,int destination,int sender,int datalen,void *data) {
  PeisHostInfo *hostInfo;
  PeisHostQueryPackage *package;
  int magic;
  package = (PeisHostQueryPackage*) data;

  /* For queries with a magic ID, always route the messages.
     For queries with no magic ID, answer if we have the host,
     otherwise route them. */
  hostInfo = peisk_lookupHostInfo(destination);
  if(hostInfo) {
    magic = ntohl(package->magic);
    if(magic == -1) { peisk_sendHostInfo(sender,hostInfo); return 1; }
  }
  return 0;
}
int peisk_hook_hostInfo(int port,int destination,int sender,int datalen,void *data) {
  int i,j;
  PeisHostInfoPackage *packageRaw = (PeisHostInfoPackage *) data;
  PeisHostInfoPackage package;
  PeisHostInfo *hostInfo;
  PeisConnectionMgrInfo *connMgrInfo;

  if(datalen < sizeof(PeisHostInfoPackage)) {
    fprintf(stderr,"Warning - ignoring bad hostInfo package\n");
    return -1; /* Broken package */
  }

  /* We work on a copy of the package since we are doing destructive
     changes between network and host byte order. */
  package = *packageRaw;

  /* Convert package to be in HOST byte order */
  peisk_ntoh_hostInfo(&package.hostInfo);

  /*printf("received hostinfo for %d (%x) (dest: %d, cluster %d)\n",package.hostInfo.id,package.hostInfo.id,destination,package.hostInfo.networkCluster);*/

  /*printf("Got hostinfo from %d magic: %x\n",package.hostInfo.id,package.hostInfo.magic);*/

  /* Update knownHosts info */
  /* See if we already had this hostinfo */
  hostInfo = peisk_lookupHostInfo(package.hostInfo.id);
  if(hostInfo && hostInfo->magic != package.hostInfo.magic) {
    /* This host was previously known with another magic number */

    printf("We noticed the death + rebirth of %d\n",package.hostInfo.id);
    /* Delete any connections going to this host with incorrect magic since they are all stale leftovers
       from the previous incarnation */
    for(j=0;j<PEISK_MAX_CONNECTIONS;j++) {
      if(peiskernel.connections[j].id != -1 &&
	 j != peisk_lastConnection &&
	 peiskernel.connections[j].neighbour.id == package.hostInfo.id &&
	 peiskernel.connections[j].neighbour.magic != package.hostInfo.magic)  {
	/*printf("Closing STALE connection %d to %d (magic was: %d)\n",peiskernel.connections[j].id,package.hostInfo.id,peiskernel.connections[j].neighbour.magic);*/
	peisk_closeConnection(peiskernel.connections[j].id);
      }
    }

    /* Delete old host information
       This will also remove him from all routingTables (thus we do
       not need to rewind the seqno ourselves) */
    peisk_deleteHost(package.hostInfo.id,PEISK_DEAD_REBORN);
    hostInfo=NULL;
  }

  if(!hostInfo) {
    /* Host not known, (re)insert it */
    hostInfo = (PeisHostInfo*) malloc(sizeof(PeisHostInfo));
    peisk_insertHostInfo(package.hostInfo.id,hostInfo);
  }
  *hostInfo = package.hostInfo;
  hostInfo->lastSeen = peisk_timeNow;

  connMgrInfo = peisk_lookupConnectionMgrInfo(hostInfo->id);
  if(!connMgrInfo) {
    /* Allocate and insert a new connMgrInfo unless we already have one */
    connMgrInfo = (PeisConnectionMgrInfo*) malloc(sizeof(PeisConnectionMgrInfo));
    peisk_initConnectionMgrInfo(connMgrInfo);
    peisk_insertConnectionMgrInfo(hostInfo->id,connMgrInfo);
    connMgrInfo->nTries=0;
    connMgrInfo->nextRetry=peisk_timeNow;
  }
  /*connMgrInfo->nTries=0;
    connMgrInfo->nextRetry=peisk_gettimef();*/

  /* If this was a direct neighbour, add hostinfo to connection */
  if(peisk_lastPackage->hops == 1 && package.isCached==0)
    for(i=0;i<PEISK_MAX_CONNECTIONS;i++)
      if(peiskernel.connections[i].id == peisk_lastConnection) {
	/*printf("adding hostinfo to connection #%d, hostInfo id %d\n",peiskernel.connections[i].id,package.hostInfo.id);*/
	peiskernel.connections[i].neighbour = package.hostInfo;
	/* Recompute metric cost of connection */
	peisk_recomputeConnectionMetric(&peiskernel.connections[i]);
	connMgrInfo = peisk_lookupConnectionMgrInfo(package.hostInfo.id);
	if(connMgrInfo) {
	  connMgrInfo->nTries = 0;
	  connMgrInfo->nextRetry = peisk_timeNow;
	  connMgrInfo->directlyConnected = 1;
	  /*printf("Resetting tries/nextRetry for %d\n",package.hostInfo.id);*/
	}
	/*printf("new metric is %d\n",peiskernel.connections[i].metricCost);*/	
	break;
      }


  return 0;
}

int peisk_isRoutable(int id) {
  PeisRoutingInfo *routingInfo;
  return peisk_hashTable_getValue(peiskernel.routingTable,(void*)(long)id,(void**)(void*) &routingInfo) == 0 && routingInfo->connection;
}

void peisk_periodic_routing(void *arg) {
  peisk_do_routing(0,NULL);
  peisk_updateRoutingTuple();
}
void peisk_do_routing(int kind,PeisConnection *targetConnection) {
  int i,j;

  PeisRoutingPackage *package;
  PeisHashTableIterator iterator;
  PeisRoutingInfo *routingInfo;
  unsigned char data[PEISK_ROUTING_PAGE_SIZE],*p;
  int destination;
  int sequenceNumber;
  int magic;
  PeisHostInfo *hostInfo;
  int skipnext;
  int nConnections;
  PeisConnectionMgrInfo *connMgrInfo;

  int entries,pages,page;
  entries = peisk_hashTable_count(peiskernel.routingTable);
  pages = (entries+PEISK_ROUTING_PER_PAGE-1) / PEISK_ROUTING_PER_PAGE;
  package = (PeisRoutingPackage*) data;

  int localSequenceNumber=0;

  /*printf("Do routing, kind = %d, conn: %x\n",kind,targetConnection);*/

  if(peisk_hashTable_getValue(peiskernel.routingTable,(void*)(long)peiskernel.id,(void**)(void*)&routingInfo) == 0) {
    PEISK_ASSERT(routingInfo->connection == NULL,("Route to ourselves points to nonzero connection\n"));
  } /*else
      PEISK_ASSERT(0,("No routing to ourselves yet\n"));*/

  /*********************************************/
  /* Add ourselves to the global routing table */
  /*********************************************/
  if(peisk_hashTable_getValue(peiskernel.routingTable,(void*)(long)peiskernel.id,(void**)(void*) &routingInfo) == 0) {
    routingInfo->hops = 0;
    routingInfo->sequenceNumber++;
    routingInfo->timeToQuery=0;
    /*printf("Setting seqno for myself %d to %d\n",routingInfo->id,routingInfo->sequenceNumber);
      printf("updating routinginfo: %x\n",routingInfo);*/
    routingInfo->connection = NULL;
    routingInfo->magic = peiskernel.magicId;
  } else {
    routingInfo = (PeisRoutingInfo*) malloc(sizeof(struct PeisRoutingInfo));
    routingInfo->id = peiskernel.id;
    routingInfo->magic = peiskernel.magicId;
    routingInfo->hops = 0;
    routingInfo->sequenceNumber = 0;
    routingInfo->connection = NULL;
    routingInfo->timeToQuery=0;
    peisk_hashTable_insert(peiskernel.routingTable,(void*)(long)peiskernel.id,(void*) routingInfo);
  }
  localSequenceNumber = routingInfo->sequenceNumber;

  /******************************************/
  /* Send routing information to neighbours */
  /******************************************/

  /* Compute how many pages we are sending */
  pages = (peisk_hashTable_count(peiskernel.routingTable) - 1) / PEISK_ROUTING_PER_PAGE + 1;
  /* Iterate over all destinations and split into pages */
  peisk_hashTableIterator_first(peiskernel.routingTable,&iterator);
  short pages_net = htons(pages);
  long localSequenceNumber_net = htonl(localSequenceNumber);
  for(page=0;page<pages;page++) {
    package->npages = pages_net;
    package->page = htons(page);
    package->entries = 0;
    package->sequenceNumber = localSequenceNumber_net;
    entries -= PEISK_ROUTING_PER_PAGE;

    /*printf("sending page %d of %d, %d entries\n",page,pages,ntohs(package->entries));*/

    /* Pack routing data suitable for transmission */
    p=data+sizeof(PeisRoutingPackage);

    for(i=0;i<PEISK_ROUTING_PER_PAGE && peisk_hashTableIterator_next(&iterator);i++) {
      peisk_hashTableIterator_value_fast(&iterator,&destination,&routingInfo);
	/*printf("sending seqno %d for dest
	  %d\n",routingInfo->sequenceNumber,destination);*/

	/*printf("Sending dest %d seq %d hops %d\n",destination,routingInfo->sequenceNumber,routingInfo->hops+1);*/

	package->entries++;

	/* See how many connections this host have */
	if(destination == peiskernel.id) {
	  for(j=0,nConnections=0;j<=peiskernel.highestConnection;j++)
	    if(peiskernel.connections[j].id != -1) nConnections++;
	} else {
	  connMgrInfo = peisk_lookupConnectionMgrInfo(destination);
	  if(connMgrInfo) nConnections = connMgrInfo->nConnections;
	  else nConnections = 255; /* Aka. -1 */
	}

	destination = htonl(destination);
	sequenceNumber = htonl(routingInfo->sequenceNumber);
	magic = htonl(routingInfo->magic);
	memcpy((void*)&p[0],(void*)&destination,4);
	memcpy((void*)&p[4],(void*)&sequenceNumber,4);
	memcpy((void*)&p[8],(void*)&magic,4);
	/*if(routingInfo->hops == 255) p[12] = 255;
	  else */
	p[12] = routingInfo->hops;
	p[13] = nConnections;

	p += PEISK_ROUTING_BYTES_PER_ENTRY;
    }
    /*printf("Sending %d bytes (%d entries, page %d)\n",p-data,package->entries,page);*/
    package->entries=htons(package->entries);
    /*peisk_hexDump(data,p-data);*/

    PEISK_ASSERT(p-data <= PEISK_ROUTING_PAGE_SIZE,("Attempting to send too large package %d bytes. entires: %d page: %d\n",(int)(p-data),ntohs(package->entries),page));

    /* Send routing data along each connection */
    if(kind == 0 || kind == 2) {
      for(i=0;i<PEISK_MAX_CONNECTIONS;i++)
	if(peiskernel.connections[i].id != -1)
	  peisk_sendLinkPackage(PEISK_PORT_ROUTING,&peiskernel.connections[i],p-data,data);
    } else if(kind == 1) {
      peisk_sendLinkPackage(PEISK_PORT_ROUTING,targetConnection,p-data,data);
    }
  }


  /*****************************************************/
  /* Handle outdated routes and routes to unknown hosts*/
  /*****************************************************/
  if(kind == 0) {
    /* Iterate over each destination in our global routing table */
    peisk_hashTableIterator_first(peiskernel.routingTable,&iterator);
    skipnext=0;
    while(skipnext || peisk_hashTableIterator_next(&iterator)) {
      skipnext=0;
      peisk_hashTableIterator_value_fast(&iterator,&destination,&routingInfo);      
      if(destination == peiskernel.id) continue; /* No point in checking routes to ourself */
      if(routingInfo->hops == 0) {
	printf("Found bad entry in routing table hops: %d dest: %d!\n",routingInfo->hops,destination);
	exit(0);
      }

      if(routingInfo->hops >= 250 && routingInfo->hops < 254) {
	/* This route is outdated */
	/* Any new routes to him will do, regardless of sequence
	   number */
	/*routingInfo->sequenceNumber = 0;*/
	/* Count down until the host will be "deleted" */
	routingInfo->hops++;
	if(routingInfo->hops == 254) {
	  /* Mark this route as "deleted" */
	  if(peisk_debugRoutes)
	    printf("peisk: deleting route to %d since it is outdated\n",destination);
	  /* We need to skip the iterator past this node when we delete it */
	  skipnext = peisk_hashTableIterator_next(&iterator);
	  peisk_deleteHost(destination,PEISK_DEAD_ROUTE);
	  peisk_checkClusters();
	}
      } else if(routingInfo->hops < 250) {
	/* See if we have a valid hostInfo for this route, otherwise
	   send a query for it. */
	hostInfo = peisk_lookupHostInfo(destination);
	if(!hostInfo) {
	  /* No such host found, query for it */
	  if(peisk_debugRoutes) {
	    printf("peisk: Querying hostinfo for peis %d (%x). Hops: %d\n",destination,destination,routingInfo->hops);
	    printf("   connection->hostInfo.id = %d\n",routingInfo->connection?routingInfo->connection->neighbour.id:-1);
	  }
	  peisk_queryHostInfo(destination);
	  /*
	  if(routingInfo->timeToQuery == 0) {
	  TODO - only if it actually succeeded...
	    peisk_queryHostInfo(destination);
	    routingInfo->timeToQuery=10;
	  } else routingInfo->timeToQuery--;
	  */
	}
      }
    }
  }
}

extern int peisk_decodeRoutingPages(int npages,PeisConnection *connection);

int peisk_hook_routing(int port,int dest,int sender,int datalen,void *data) {
  int page;
  PeisRoutingPackage *packageRaw;
  PeisRoutingPackage package;
  PeisConnection *connection;
  
  if(datalen < sizeof(PeisRoutingPackage)) {
    fprintf(stderr,"peisk::hook_routing - bad length of received package\n");
    return -1;
  }
  
  packageRaw = (PeisRoutingPackage*)data;
  package = *packageRaw;
  package.page = ntohs(package.page);
  package.npages = ntohs(package.npages);
  package.entries = ntohs(package.entries);
  package.sequenceNumber = ntohl(package.sequenceNumber);
  
  connection = peisk_lookupConnection(peisk_lastConnection);
  if(!connection->routingPages[package.page]) {
    connection->routingPages[package.page] = (unsigned char *) malloc(PEISK_ROUTING_PAGE_SIZE);
  }
  if(datalen > PEISK_ROUTING_PAGE_SIZE) {
    fprintf(stderr,"Warning, ignoring extra data in routing package (got %d, expected %d)\n",datalen,(int) PEISK_ROUTING_PAGE_SIZE);
    datalen=PEISK_ROUTING_PAGE_SIZE;
  }
  if(package.page != package.npages-1 && datalen != PEISK_ROUTING_PAGE_SIZE) {
    fprintf(stderr,"Warning, too little data in routing package page %d\n",package.page);
  }
  if(package.page > PEISK_MAX_ROUTING_PAGES) {
    fprintf(stderr,"Error, received bad routing page %d\n",package.page);
    return -1;
  }
  /* Store this routing information for future reference */
  memcpy((void*)connection->routingPages[package.page],(void*)data,datalen);
  /* Also store the converted routing package in these pages... helps
     checking that we got all the data we needed. */
  *((PeisRoutingPackage*)connection->routingPages[package.page]) = package;
  
  /* Check to see if we have received all pages with the same
     sequence number for this routingInfo. */
  for(page=0;page<package.npages;page++)
    if(!connection->routingPages[page] ||
       ((PeisRoutingPackage*)connection->routingPages[page])->sequenceNumber != package.sequenceNumber) break;

  if(page == package.npages) {
    peisk_decodeRoutingPages(package.npages,connection);
  }
  return 0;
}
int peisk_decodeRoutingPages(int npages,PeisConnection *connection) {
  int j, e, skipnext, entry, page;
  PeisConnection *bestConnection;
  int nentries;
  unsigned char *p;
  intA destination;
  unsigned char metric;

  PeisHashTableIterator iterator;
  PeisRoutingInfo *routingInfo;
  PeisRoutingInfo *routingInfo2;
  int sequenceNumber, magic;
  PeisHostInfo *hostInfo;
  /* ConnectionMgrInfo for the host from which routing tables came */
  PeisConnectionMgrInfo *connMgrInfo;
  int nConnections;


  /* All pages received and with the same sequence number, we can
     now decode the pages. */
  
  /* First, mark all old routing information in this connection's routingTable as
     outdated (metric=255) */
  peisk_hashTableIterator_first(connection->routingTable,&iterator);
  while(peisk_hashTableIterator_next(&iterator)) {
    peisk_hashTableIterator_value_fast(&iterator,&destination,&routingInfo);
    routingInfo->hops=255;
  }
    
  /** General idea is that the routingInfo magic that is propagated always comes from the 
     shortest route informtion. */        
  /** Invariant:  This function always maximize sequenceNumber - metric. 
     This value should monotonically increase, both in connection RT and in global RT. Implications:
       - If a short route to someone is lost, longer routes won't be considered until the seqno has increased.
       - If someone is lost, metrics will increase and seqno stay same => conn RT not updated => eventual delete from global RT
  */
  /* Go through all routing info we received metric */

  connMgrInfo = connection->neighbour.id != -1 ? peisk_lookupConnectionMgrInfo(connection->neighbour.id) : NULL;
  if(connMgrInfo) connMgrInfo->nConnections=0;

  for(entry=0,page=0;page<npages;page++) {
    p=connection->routingPages[page]+sizeof(PeisRoutingPackage);
    nentries = ((PeisRoutingPackage*)connection->routingPages[page])->entries;
    for(e=0;e<nentries;e++) {	

      int foo[3];
      memcpy((void*)foo,(void*)&p[0],12);
      destination=foo[0];
      sequenceNumber=foo[1];
      magic=foo[2];

      /*
      memcpy((void*)&destination,(void*)&p[0],4);
      memcpy((void*)&sequenceNumber,(void*)&p[4],4);
      memcpy((void*)&magic,(void*)&p[8],4);
      */

      destination=ntohl(destination);
      sequenceNumber=ntohl(sequenceNumber);
      magic=ntohl(magic);
      metric = p[12];
      nConnections = p[13];
      /* TODO - only do this if we have a high enough seqno */
      if(connMgrInfo) connMgrInfo->nConnections = nConnections;

      PEISK_ASSERT(connection->metricCost>0,("Connection %d have metricCost %d\n",connection->id,connection->metricCost));
      if(metric < 250) metric += connection->metricCost;
      if(metric >= PEISK_METRIC_GIVEUP) metric=255;
      p+=PEISK_ROUTING_BYTES_PER_ENTRY;
      PEISK_ASSERT(metric > 0,("Invalid metric %d received in routing package, for dest %d\n",metric,(int)destination));
      
      /*printf("Routing decoded: dest %d seq %d hops %d\n",destination,sequenceNumber,metric);*/

      /*printf("conn %d got hops %d seqno %d magic %x for dest %d\n",connection->id,metric,sequenceNumber,magic,destination);*/
      
      /* Detect duplicate PEIS with same ID */
      if(destination == peiskernel.id && magic != peiskernel.magicId) {
	if((peisk_hashTable_getValue(peiskernel.routingTable,
				     (void*)(long)peiskernel.id,(void**)(void*) &routingInfo) == 0 &&
	    routingInfo->hops > 3)) {
	  /* Some other guy with our ID might exist on the network... very bad! */
	  fprintf(stderr,"peisk::routing hook: Warning! Another PEIS with our ID (%d) seem to exist on network.\n Suggest doing a proper (soft) shutdown and restart.\n",peiskernel.id);
	} /* Else: This might just old routingInformation lying around. */
      }
      /* Check if we have a possible conflict of multiple PEIS with same ID and different magic */
      /* This is checked by the information in the knownHosts information */
      hostInfo = peisk_lookupHostInfo(destination);
      if(hostInfo && 
	 destination != peiskernel.id &&
	 hostInfo->magic != magic) {
	
	/* Magic info for this host does not correspond with old known host information,
	   send a query to this new host using a new routing to him.
	   If he responds (it is truly a new host) then 
	   the response will trigger a deleteHost and a refresh. If not (he is just an 
	   old remnant in the routes), then no harm done. */
	/* We are forced to decrease the seqno value down to what this information provides.
	   This makes the seqno - metric decrease, but it only happens due to a restarted host so that should be ok. 
	   As soon as the hostinfo query has responded back to us we will be monotonically increasing again... 
	*/
	
	/** Heuristic:
	   1. Temporarily update route to use this new path
	   2. Sends a hostQuery (this will append the query message
	   to this connection)
	   3. Restore route to be what it was previously.
	   
	   4. When we receive the reply from the host, then
	   hostinfo, route and magic will be updated
	   permanently. Host will be reborn. (dead+alive)
	   
	   Case A: Host has disappeared and a new host has appeared.
	   * First time we see the new host magic, we will query him
	   using the new path (since old path is older than X
	   seconds). After a while he responds, makes him reborn etc. 
	   
	   Case B: We get an incorrect route to a "new" instance of
	   the host. 
	   * We send a query to him, but do not change routing table
	   * Hence, we will not propagate further any incorrect path
	   to this host. 
	*/

	PeisConnection *prevConnection = NULL;
	routingInfo=NULL;
	if(peisk_hashTable_getValue(peiskernel.routingTable,(void*)(long) destination,
				    (void**)(void*)&routingInfo) == 0) {
	  prevConnection = routingInfo->connection;
	  /* Set a new temporary route */
	  routingInfo->connection = connection;
	  /* We don't care to update hops, seqno, magic etc. */
	}
	/* Actual query is done after changes to routing */

	/* Only perform query if it is better then the current best global routing, 
	   or if it is the current global routing. This reduces the total number of queries significantly */
	if(routingInfo && ((metric < routingInfo->hops) || prevConnection == connection)) {
	  int oldMagic = hostInfo->magic;
	  hostInfo->magic = magic;
	  peisk_queryHostInfo(destination);
	  hostInfo->magic = oldMagic;
	}
	if(prevConnection && routingInfo) {
	  /* Restore old route */
	  routingInfo->connection = prevConnection;
	}
      }
      /* Insert into connection routing table */
      if(metric < 250) {
	if(peisk_hashTable_getValue(connection->routingTable,(void*)(long) destination,
				    (void**)(void*)&routingInfo) == 0) {
	  /* Old destination found */
	  /* update metric value if this routingInfo is fresh */
	  if(sequenceNumber-metric >= routingInfo->sequenceNumber-routingInfo->hops) {
	    
	    routingInfo->hops = metric;
	    /*printf("updating connection %d metric to %d to be %d\n",connection->id,destination,metric);*/
	    
	    /*PEISK_ASSERT(sequenceNumber>=routingInfo->sequenceNumber,
	      ("receiving older sequence number for host %d was: %d (%x) received: %d, metric: %d?\n",destination,
	      routingInfo->sequenceNumber,routingInfo,sequenceNumber,metric));*/
	    /*if(sequenceNumber<routingInfo->sequenceNumber) exit(0);*/ /* DEBUG */

	    routingInfo->sequenceNumber = sequenceNumber;

	    /*printf("(1) updating routinginfo: %x to %dn",routingInfo,sequenceNumber);*/
	  }
	  
	} else {
	  /*printf("setting connection %d metric to %d to be %d\n",connection->id,destination,metric);*/
	  
	  /* This destination didn't exist previously in connection routing table */
	  routingInfo = (PeisRoutingInfo*) malloc(sizeof(struct PeisRoutingInfo));
	  routingInfo->timeToQuery=0;
	  routingInfo->id = destination;
	  routingInfo->hops = metric;
	  routingInfo->connection = connection;
	  routingInfo->magic = magic;
	  PEISK_ASSERT(routingInfo->connection->id != -1,("Bad connection that we received new routing info from. peisk_lastConnection = %d\n",peisk_lastConnection));
	  routingInfo->sequenceNumber = sequenceNumber;
	  
	  /*printf("(2) updating routinginfo: %x to %d\n",routingInfo,sequenceNumber);
	    printf("setting connection dest: %d to seq %d\n",routingInfo->id,routingInfo->sequenceNumber);*/
	  peisk_hashTable_insert(connection->routingTable,(void*)(long)destination,routingInfo);
	}
	
	/* Insert/update in main routing table */
	if(peisk_hashTable_getValue(peiskernel.routingTable,(void*)(long) destination,(void**)(void*)&routingInfo) == 0) {
	  
	  /* If old route was using this connection, then update old route to whatever this connection is now. */
	  if(routingInfo->connection == connection) {
	    routingInfo->sequenceNumber = sequenceNumber;
	    routingInfo->hops = metric;
	    routingInfo->magic = magic;
	  }
	  /* Else, if this route is strictly better than the old route, update it */
	  else if(sequenceNumber-metric > routingInfo->sequenceNumber-routingInfo->hops) {
	    /*printf("Updating global metric to %d to be %d and use #%d\n",destination,metric,connection->id);*/
	    routingInfo->sequenceNumber = sequenceNumber;
	    routingInfo->hops = metric;
	    routingInfo->magic = magic;
	    routingInfo->connection = connection;
	  }
	} else {
	  /* This destination didn't exist previously in main routing
	     table */
	  if(metric < 250) {
	    routingInfo = (PeisRoutingInfo*) malloc(sizeof(struct PeisRoutingInfo));
	    routingInfo->id = destination;
	    routingInfo->hops = metric;
	    routingInfo->connection = connection;
	    routingInfo->sequenceNumber = sequenceNumber;
	    routingInfo->magic = magic;
	    routingInfo->timeToQuery=0;

	    peisk_hashTable_insert(peiskernel.routingTable,(void*)(long)destination,routingInfo);
	    if(peisk_debugRoutes)
	      printf("Creating new route to %d using connection #%d\n",(int)destination,connection->id);
	  }
	}
      }
    }
  }
  
  /* Go through all old routes, see which ones where not updated and
     remove them from connection routingTable */
  peisk_hashTableIterator_first(connection->routingTable,&iterator);
  skipnext=0;
  while(skipnext || peisk_hashTableIterator_next(&iterator)) {
    skipnext=0;
    if(!peisk_hashTableIterator_value_generic(&iterator,&destination,&routingInfo)) continue;

    if(routingInfo->hops == 255) {
      /* Remove this route from connection routingTable */
      skipnext=peisk_hashTableIterator_next(&iterator);
      peisk_hashTable_remove(connection->routingTable,(void*)(long)destination);
      free(routingInfo);
      
      /* See if it needs to be "removed" from main routing table */
      /* "Removing" a connection from the main routing table
	 corresponds to settings it's hops to 250 or higher. A periodic
	 function checks hops in the main routing table, depending
	 on the value it can treat the route as "lost" and take
	 appropriate actions. */
      
      /* This overwrites old value of the routingInfo pointer - take care, we still have destination ok */
      if(peisk_hashTable_getValue(peiskernel.routingTable,(void*)(long) destination,(void**)(void*)&routingInfo) == 0) {
	if(routingInfo->connection == connection && routingInfo->hops < 250) {
	  /*printf("Setting hops to %d in main routingtable to 250\n",destination);*/
	  routingInfo->hops=250;
	  metric = 250;
	  sequenceNumber = routingInfo->sequenceNumber;
	  bestConnection = NULL;
	  magic = routingInfo->magic;
	  
	  /* Search through all other connections and 
	     update routingInfo if there is anyone else */
	  for(j=0;j<=peiskernel.highestConnection;j++)
	    if(peiskernel.connections[j].id != -1 &&
	       peisk_hashTable_getValue(peiskernel.connections[j].routingTable,
					(void*)(long) destination,(void**)(void*) &routingInfo2) == 0 &&
	       routingInfo2->sequenceNumber-routingInfo2->hops > sequenceNumber-metric) {
	      metric=routingInfo2->hops;
	      sequenceNumber=routingInfo2->sequenceNumber;
	      bestConnection = &peiskernel.connections[j]; /*routingInfo2->connection;*/
	      magic = routingInfo2->magic;
	    }
	  routingInfo->connection = bestConnection;
	  routingInfo->hops = metric;
	  routingInfo->sequenceNumber = sequenceNumber;
	  routingInfo->magic = magic;
	  /*if(metric < 250)
	    q
	    printf("Route to %d rescued to %d hops using connection #%d instead\n",destination,metric,bestConnection->id);
	    else
	    printf("Route to %d is (temporarily?) lost\n",destination);*/
	}
      } else {
	if (peisk_printLevel & PEISK_PRINT_WARNINGS)
	  PEISK_ASSERT(0,("Removing route to %d from connection #%d routingtable, but it didn't exist in main routing table\n",(int)destination,connection->id));
      }
    }
  }
  
  /* See if our network cluster have changed due to new routing
     information */
  /*printf("CHECKING CLUSTERS\n");*/
  peisk_checkClusters();
  return 0;
}

void peisk_updateRoutingTuple() {
  int entries = peisk_hashTable_count(peiskernel.routingTable);
  char *buffer = peisk_getTupleBuffer(5+entries*32); /* was * 16 */
  char *s = buffer;
  PeisHashTableIterator iterator;
  PeisRoutingInfo *routingInfo;
  long long destination;

  sprintf(s,"(\n"); s += strlen(s);
  peisk_hashTableIterator_first(peiskernel.routingTable,&iterator);
  for(;peisk_hashTableIterator_next(&iterator);) {
    peisk_hashTableIterator_value_generic(&iterator,&destination,&routingInfo);
    sprintf(s," (%4lld %2d %4d",destination,routingInfo->hops,routingInfo->connection?routingInfo->connection->neighbour.id:-1);
    s += strlen(s);
    /*
    PeisConnection *connection = routingInfo->connection;
    if(connection && 
       peisk_hashTable_getValue(connection->routingTable,(void*)(long) destination,(void**)(void*)&routingInfo) == 0) {
      sprintf(s," %d sq %d m %d",connection->id,routingInfo->sequenceNumber,routingInfo->hops);
    } else {
      sprintf(s,"0 0");
    }
    s += strlen(s);*/

    sprintf(s,")\n");
    s += strlen(s);
  }
  sprintf(s,")\n"); s += strlen(s);  
  peisk_setStringTuple("kernel.routingTable",buffer);
}

/******************************************************************************/
/*                                                                            */
/* SERVICE: DEAD HOST                                                         */
/*                                                                            */
/* STATUS: In development                                                     */
/*                                                                            */
/* PORTS: *_DEAD_HOST                                                         */
/* INTERFACE: peisk_sendExitNotification(), peisk_hook_deadHost()             */
/*                                                                            */
/*                                                                            */
/******************************************************************************/

int peisk_hook_deadHost(int port,int destination,int sender,int datalen,void *data) {
  int id;

  PeisDeadHostMessage *message = (PeisDeadHostMessage *) data;

  /* Ignore deadHost notifications direct to a specific host not us. 
     These are only used for ensuring symmetry in which hosts know of which others are considered "lost". */
  if(destination != -1 && destination != peiskernel.id) 
    return 0;

  /* Convert message to host byte order */
  id = ntohl(message->id);
  
  /*printf("received deadHost from %d (sender %d destination %d) why is %d\n",id,sender,destination,ntohl(message->why));*/

  if(ntohl(message->why) == PEISK_DEAD_ROUTE) {
    /*printf("Deleting host %d\n",id);*/
    peisk_deleteHost(id,PEISK_DEAD_MESSAGE);
  }
  else if(ntohl(message->why == PEISK_DEAD_REBORN)) {
    peisk_deleteHostFromSentSubscriptions(id);
  }
  return 0;
}

void peisk_sendExitNotification() {
  PeisDeadHostMessage message;
  message.id = htonl(peiskernel.id);
  message.why = htonl(PEISK_DEAD_ROUTE);
  peisk_broadcastSpecial(PEISK_PORT_DEAD_HOST,sizeof(message),&message,PEISK_BPACKAGE_HIPRI);
  /*printf("sending dead host notification\n");*/
}


/******************************************************************************/
/*                                                                            */
/* SERVICE: Trace                                                             */
/*                                                                            */
/* STATUS: Working                                                            */
/*                                                                            */
/* PORTS: *_TRACE, *_TRACE_REPLY                                              */
/* INTERFACE: -                                                               */
/*                                                                            */
/*                                                                            */
/******************************************************************************/

PeisTracePackage peisk_traceResponse;

void peisk_sendTrace(int target) {
  printf("peisk: Sending trace\n");
  peisk_traceResponse.hops=htonl(1);
  peisk_traceResponse.hosts[0]=htonl(peiskernel.id);
  peisk_sendMessage(PEISK_PORT_TRACE,target,sizeof(PeisTracePackage),(void*)&peisk_traceResponse,0);
}

int peisk_hook_trace(int port,int destination,int sender,int datalen,void *data) {
  int hops;

  printf("peisk: got trace\n");
  PeisTracePackage *tracePackage=(PeisTracePackage *) data;
  if(datalen < sizeof(PeisTracePackage)) return 1; /* Bad package length, abort it */
  hops = ntohl(tracePackage->hops);
  if(hops < 0) return 1;             /* Bad package, abort it */

  /*
  printf("peisk: passing TRACEROUTE to %d:%d: ",destination,port);
  for(i=0;i<tracePackage->hops;i++) printf("%d ",tracePackage->hosts[i]);
  printf("\n");
  */

  /* Add our ID to the traceroute */
  if(hops < PEISK_MAX_HOPS) {
    tracePackage->hosts[hops++]=htonl(peiskernel.id);
    tracePackage->hops = htonl(hops);
  }

  /* If target is us, send back package as a reply */
  if(destination == peiskernel.id && port == PEISK_PORT_TRACE) {
    /*printf("Sending reply to %d:%d\n",PEISK_PORT_TRACE_REPLY,sender);*/
    peisk_sendMessage(PEISK_PORT_TRACE_REPLY,sender,sizeof(PeisTracePackage),tracePackage,0);
  }
  /* If package originated from us, save as response */
  else if(destination == peiskernel.id && port == PEISK_PORT_TRACE_REPLY) {
	peisk_traceResponse=*tracePackage;
  }
  /* Otherwise, send along modified package */
  else {
    peisk_sendMessageFrom(sender,port,destination,sizeof(PeisTracePackage),tracePackage,0);
  }

  /* Always interrupt old package */
  return 1;
}









/******************************************************************************/
/*                                                                            */
/* SERVICE: Connection Control                                                */
/*                                                                            */
/* STATUS:                                                                    */
/*                                                                            */
/* PORTS: PEISK_PORT_UDP_SPEED                                                */
/* VARIABLES: Manages congestion control on connections requiring it.         */
/*                                                                            */
/*                                                                            */
/******************************************************************************/

void peisk_periodic_connectionControl1(void *data) {
  int i;
  PeisConnection *connection;

  for(i=0;i<PEISK_MAX_CONNECTIONS;i++)
    if(peiskernel.connections[i].id != -1) {
      connection=&peiskernel.connections[i];
      connection->outgoing -= PEISK_CONNECTION_CONTROL_PERIOD * connection->maxOutgoing;
      if(connection->outgoing < 0.0) connection->outgoing=0.0;
    }
}
void peisk_periodic_connectionControl2(void *data) {
  int i;
  PeisConnection *connection;
  int message[2];
  for(i=0;i<PEISK_MAX_CONNECTIONS;i++)
    if(peiskernel.connections[i].id != -1) {
      connection=&peiskernel.connections[i];
      if(peiskernel.connections[i].type == eUDPConnection) {
	/* Send a message containing total number of expected packages (hi - lo) and successfully received packages (succ)
	   for the last time period. */
	message[0] = connection->incomingIdHi - connection->incomingIdLo;
	message[1] = connection->incomingIdSuccess;
	peisk_sendLinkPackage(PEISK_PORT_UDP_SPEED, connection, sizeof(message), (void*) message);
	printf("Sending UDP speed: %d/%d=%f\n",message[1],message[0],((float)message[1])/message[0]);
      }
      if(connection->incomingIdHi - connection->incomingIdLo > 0)
	connection->estimatedPacketLoss = connection->estimatedPacketLoss*0.9 +
	  0.1 * (1.0 - ((double) connection->incomingIdSuccess) / (connection->incomingIdHi - connection->incomingIdLo));
      /*printf("Lo: %d, Hi: %d, got: %d\n",connection->incomingIdLo,connection->incomingIdHi,connection->incomingIdSuccess);*/

      connection->incomingIdLo = connection->incomingIdHi;
      connection->incomingIdSuccess = 0;
    }
}
int peisk_hook_udp_speed(int port,int destination,int sender,int datalen,void *data) {
  int *message = (int*) data;
  double ratio = ((double) message[1]) / message[0];
  PeisConnection *connection = peisk_lookupConnection(peisk_lastConnection);
  if(!connection) return 1;
  printf("Got UDP speed control: %d/%d=%f, thr: %d maxOutgoing=%f\n",message[1],message[0],ratio,connection->isThrottled,connection->maxOutgoing);

  if(ratio < 0.75 && connection->isThrottled) connection->maxOutgoing = PEISK_MIN_CONTROL_SPEED > connection->maxOutgoing/1.5 ? PEISK_MIN_CONTROL_SPEED : connection->maxOutgoing/1.1;
  else if(ratio > 0.95 && connection->isThrottled) connection->maxOutgoing *= 1.1;

  connection->isThrottled=0;

  return 1;
}

/******************************************************************************/
/*                                                                            */
/* SERVICE: timesync                                                          */
/*                                                                            */
/* STATUS: Testing                                                            */
/*                                                                            */
/* PORTS:                                                                     */
/* VARIABLES: peiskernel.timeOffset, peiskernel.isTimeMaster, etc.            */
/*                                                                            */
/*                                                                            */
/******************************************************************************/

typedef struct PeisTimeSync {
  int globalTime[2];
  char isTimeMaster;
  unsigned char padding[3];
} PeisTimeSync;

/* Counter to avoid triggering our timesync the first X periods so
   we increase chance of listening to old timeservers before
   starting our own notion of time after booting up.

*/
int peisk_timeSync_inhibit=-1;

void peisk_periodic_timeSync(void *data) {
  PeisTimeSync message;
  int timenow[2];

  if(peisk_timeSync_inhibit == -1) {
    if(peiskernel.isTimeMaster) peisk_timeSync_inhibit=0;
    else peisk_timeSync_inhibit=3;
  }
  if(peisk_timeSync_inhibit>0) { peisk_timeSync_inhibit--; return; }

  peisk_gettime2(&timenow[0],&timenow[1]);
  message.globalTime[0] = htonl(timenow[0]);
  message.globalTime[1] = htonl(timenow[1]);
  message.isTimeMaster = peiskernel.isTimeMaster;
  /*printf("sending timesync\n");*/

  peisk_broadcast(PEISK_PORT_TIMESYNC,sizeof(message),(void*)&message);
}

int peisk_hook_timeSync(int port,int destination,int sender,int datalen,void *data) {
  int i;
  PeisTimeSync *message = (PeisTimeSync*) data;
  if(datalen < sizeof(PeisTimeSync)) return -1;
  int offset[2];
  int timenow[2];
  int globalTime[2];
  double delta;
  globalTime[0] = ntohl(message->globalTime[0]);
  globalTime[1] = ntohl(message->globalTime[1]);

  /*printf("got timesync\n");*/

  offset[0]=0; offset[1]=0;
  peisk_gettime2(&timenow[0],&timenow[1]);
  if(!peiskernel.isTimeMaster) {
    if(message->isTimeMaster) {
      /*printf("case A\n");*/
      /* Always accept the time sent by a timemaster */
      offset[0] = globalTime[0] - timenow[0];
      offset[1] = globalTime[1] - timenow[1];
      offset[0] += offset[1] / 1000000;
      offset[1] %= 1000000;
      /* Inibit broadcasting our time as long as we get time from a
	 master */
      peisk_timeSync_inhibit=4;
    } else {
      /*printf("case B\n");*/

      /* Time comes from another non trusted source, only update if it
	 is newer than our own time by an integer second. */
      offset[0] = globalTime[0] - timenow[0];
      offset[1] = globalTime[1] - timenow[1];
      offset[0] += offset[1] / 1000000;
      offset[1] %= 1000000;
      delta = (double) offset[0] + offset[1]*1e-6;

      if(delta > 0) {
	/* Accept new offset from this source since it is higher than
	   us, and no need for us to broadcast anymore if he has
	   higher ID  */
	if(sender > peiskernel.id)
	  peisk_timeSync_inhibit=2+(rand()%3);
      } else {
	/* Don't accept this offset */
	offset[0]=0;
	offset[1]=0;

	/*printf("Delta: %f, great? %s\n",delta,sender>peiskernel.id?"yes":"no");*/
	/* But disable broadcasting if he is close enough to our time
	   (0.5 sec) and higher ID than us */
	if(delta > -0.5 && sender > peiskernel.id)
	  peisk_timeSync_inhibit=2+(rand()%3);
      }
    }
  } else if(message->isTimeMaster) {
    /*printf("case C\n");*/

    /* We are both time masters */
    offset[0] = globalTime[0] - timenow[0];
    offset[1] = globalTime[1] - timenow[1];
    offset[0] += offset[1] / 1000000;
    offset[1] %= 1000000;
    delta = (double) offset[0] + offset[1]*1e-6;
    if(delta > 0) {
      /* He has higher time than us, update ourselves and stop
	 broadcasting if he has higher ID */
      if(sender > peiskernel.id)
	peisk_timeSync_inhibit=2+(rand()%3);
    } else {
      /* Don't accept this offset */
      offset[0]=0;
      offset[1]=0;
      /* But disable broadcasting if he is close enough to our time
	 (0.5 sec) and higher ID than us */
      if(delta > -0.5 && sender > peiskernel.id)
	peisk_timeSync_inhibit=2+(rand()%3);
    }
  }

  delta = offset[0] + 1e-6*offset[1];
  if(delta > 0.5 || delta < -0.5) {
    printf("Adjusting time with %.2f seconds\n",delta);
    peiskernel.timeOffset[0] += offset[0];
    peiskernel.timeOffset[1] += offset[1];

    /* Adjust next calling time of all periodic functions */
    for(i=0;i<=peiskernel.highestPeriodic;i++)
      if(peiskernel.periodics[i].periodicity != -1.0)
	peiskernel.periodics[i].last += delta;

    /* Adjust timestamps of connections */
    for(i=0;i<=peiskernel.highestConnection;i++)
      peiskernel.connections[i].timestamp += delta;
    
    /** \todo Add mechanism to adjust time of tuples when clock is changed */

    peisk_timeNow = peisk_gettimef();
  }

  /*printf("sleeping %d cycles still\n",peisk_timeSync_inhibit);*/

  return 0;
}

/******************************************************************************/
/*                                                                            */
/* SERVICE: callback kernel_quit                                              */
/*                                                                            */
/******************************************************************************/

void peisk_callback_kernel_quit(PeisTuple *tuple,void *arg) {
  if(tuple->data && *(tuple->data) && strcasecmp(tuple->data,"no") && strcasecmp(tuple->data,"nil")) {
    if(peisk_printLevel & PEISK_PRINT_STATUS)
      printf("Exiting on request by %d.kernel.do-quit\n",tuple->owner);
    peisk_shutdown();
  }
}

/******************************************************************************/
/*                                                                            */
/* SERVICE: Acknowledgemetns                                                  */
/*                                                                            */
/******************************************************************************/

void peisk_sendAcknowledgementsNow() {
  int i,j,total;
  int v;
  int destination;
  unsigned char buff[1024];
  unsigned char *data;
  int priority;

  /* Iterate over all ack's to send */
  for(i=0;i<peiskernel.nAcknowledgementPackages;i++)
    if(peiskernel.acknowledgementPackages[i].destination > 0) {
      destination = peiskernel.acknowledgementPackages[i].destination;
      data = buff;
      data += sizeof(int);
      /* Iterate over all remaining ack's, add them to the list to send
	 and mark as already sent (.dest=-1) */
      for(total=0,j=i,priority=0;j<peiskernel.nAcknowledgementPackages&&total<200;j++) {
	if(peiskernel.acknowledgementPackages[j].destination == destination) {
	  total++;
	  priority += peiskernel.acknowledgementPackages[j].priority;
	  memcpy(data,&peiskernel.acknowledgementPackages[j].ackID,sizeof(int));
	  data += sizeof(int);
	  peiskernel.acknowledgementPackages[j].destination=-1;
	}
      }
      v = htonl(total);
      memcpy(buff,&v,sizeof(int));
      
      /*printf("Sending %d acks to %d\n",total,destination);*/

      /* Depending on how many packages was marked as a priority-to-acknowledge package
	 we send in acknowledged or just normal mode.
	 Note that pure acknowledgement packages always register as non priorty-to-ack packages. 
      */
      if(priority > 0)
	peisk_sendMessage(PEISK_PORT_ACKNOWLEDGEMENTS,destination,sizeof(int)*(1+total),
			  (void*)buff,PEISK_PACKAGE_RELIABLE);
      else
	peisk_sendMessage(PEISK_PORT_ACKNOWLEDGEMENTS,destination,sizeof(int)*(1+total),(void*)buff,0);
    }
  peiskernel.nAcknowledgementPackages=0;
  /*
  ack.header.id = htonl(rand() & 0x7fffffff);
  ack.header.ackID = ackID;
  ack.header.flags = htons(PEISK_PACKAGE_IS_ACK);
  ack.header.type = ePeisDirectPackage;
  ack.header.hops=0;
  ack.header.source=htonl(peiskernel.id);
  ack.header.destination=destination;
  ack.header.port=0;
  ack.header.datalen=htons(0);
  ack.header.seqlen=htons(0);
  ack.header.seqid=0;
  ack.header.seqnum=htons(0);
  peisk_connection_sendPackage(connection->id,&ack.header,0,NULL,PEISK_PACKAGE_HIPRI);
  */
}
int peisk_hook_acknowledgments(int port,int destination,int sender,int datalen,void *data) {
  PeisConnection *connection2;
  PeisQueuedPackage *qpackage;
  PeisQueuedPackage **prev;
  int i;

  int nAcks, ackID;
  if(destination != peiskernel.id) return 0;
  char *buff = (char*) data;
  memcpy(&nAcks,buff,sizeof(int));
  nAcks = ntohl(nAcks); buff += sizeof(int); datalen -= sizeof(int);
  /*printf("Got %d acks\n",nAcks);*/
  for(;nAcks>0;nAcks--) {
    datalen -= sizeof(int);
    if(datalen < 0) {
      PEISK_ASSERT(0,("Error parsing acknowledgement package, nAcks=%d datalen=%d\n",nAcks,datalen));
      return 0;
    }
    memcpy(&ackID,buff,sizeof(int));
    buff += sizeof(int);

    /* Handle this ack package. Remove correspoding entry from "pending" queue from *ALL* connections */
    for(i=0;i<PEISK_MAX_CONNECTIONS;i++)
      if(peiskernel.connections[i].id != -1) {
	connection2=&peiskernel.connections[i];
	qpackage = connection2->outgoingQueueFirst[PEISK_QUEUE_PENDING];
	prev = &connection2->outgoingQueueFirst[PEISK_QUEUE_PENDING];
	while(qpackage) {
	  /* Remove qpackage if it matches the ACK */
	  if(qpackage->package.header.ackID == ackID) {
	    /* If an acknowledgement hook was registered for this
	       package, invoke it. */
	    if(qpackage->nHooks) {
	      /*printf("Found a package with %d ackHooks. datalen: %d data: %s\n",qpackage->nHooks,ntohs(qpackage->package.header.datalen),qpackage->package.data);*/
	      PEISK_ASSERT(qpackage->nHooks<PEISK_MAX_ACKHOOKS,("Found a queue package with %d hooks\n",qpackage->nHooks));
	      for(i=0;i<qpackage->nHooks;i++)	      
		(qpackage->hook[i])(1,ntohs(qpackage->package.header.datalen),&qpackage->package,qpackage->hookData[i]);
	    }
	    /*printf("Removing a Pending package. ackID = %d\n",ackID);*/
	    connection2->nQueuedPackages[PEISK_QUEUE_PENDING]--;
	    *prev=qpackage->next;
	    /* Be carefull when removing the lastmost element of the queues */
	    if(&qpackage->next == connection2->outgoingQueueLast[PEISK_QUEUE_PENDING])
	      connection2->outgoingQueueLast[PEISK_QUEUE_PENDING]=prev;
	    qpackage->next = peiskernel.freeQueuedPackages;
	    peiskernel.freeQueuedPackages = qpackage;
	    qpackage=*prev;
	  } else {
	    prev=&qpackage->next;
	    qpackage=qpackage->next;
	  }
	  if(connection2->outgoingQueueFirst[PEISK_QUEUE_PENDING] == NULL)
	    PEISK_ASSERT(connection2->outgoingQueueLast[PEISK_QUEUE_PENDING] == &connection2->outgoingQueueFirst[PEISK_QUEUE_PENDING],
			 ("I've just created a bad pending queue\n"));
	}
      }

  }
  return 0;
}
