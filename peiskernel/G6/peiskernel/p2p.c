/** \file p2p.c Contains all basic p2p functionalities of the Peis
    kernel
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
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define PEISK_PRIVATE
#include "peiskernel.h"
#include "peiskernel_tcpip.h"
#include "udp.h"
#include "bluetooth.h"

/** Describes which port numbers provide only meta information and
    which transport useful data (from the perspective of higher
    layers). Used for estaimating the useful traffic transmitted on
    each link. */
const char peisk_metaPorts[PEISK_HIGHEST_PORT_NUMBER] = {
  1, 1, 1, 1, 1, 
  0, 0, 0, 1, 1, 
  0, 1, 1 /*??*/, 1 /*??*/, 0, 
  0, 0, 0, 0, 0,  
};

PeisConnection *peisk_incommingBroadcastConnection;
double peisk_timeNow;
int peisk_useConnectionManager=1;

void peisk_initP2PLayer() {
  /* Connection manager */
  peisk_registerPeriodic(PEISK_CONNECTION_MANAGER_PERIOD,NULL,peisk_periodic_connectionManager);
  peisk_registerPeriodic(PEISK_CONNECT_CLUSTER_PERIOD,NULL,peisk_periodic_connectCluster);
}


void peisk_initConnectionMgrInfo(PeisConnectionMgrInfo *connMgrInfo) {
  connMgrInfo->usefullTraffic=0;
  connMgrInfo->lastUsefullTraffic=0;
  connMgrInfo->directlyConnected=0;
  connMgrInfo->nConnections=255; /* Aka -1 */
}

void peisk_initConnection(PeisConnection *connection) {
  int i;
  PeisHashTableIterator iterator;
  PeisRoutingInfo *routingInfo;
  intA destination;

  connection->id=peiskernel.nextConnectionId++;
  /* Default hostinfo for new host is -1 = unknown host */
  connection->neighbour.id=-1;                   
  connection->timestamp=peisk_timeNow;
  connection->createdTime=peisk_timeNow;
  connection->outgoing=0.0;
  connection->isThrottled=0;
  connection->totalOutgoing=0;
  connection->totalIncomming=0;
  connection->totalOutgoing=0;
  /* Default limit of num packages per time interval (0.05s) */
  connection->maxOutgoing=PEISK_MIN_CONTROL_SPEED; 
  connection->outgoingIdCnt=0;
  connection->incomingIdLo=-1;
  connection->incomingIdSuccess=0;
  connection->forceBroadcasts=0;
  connection->incommingTraffic=0;
  connection->estimatedPacketLoss=0.0;
  connection->usefullTraffic=0;
  connection->lastUsefullTraffic=0;
  connection->isPending=1;

  /* Recompute metric cost of connection */
  peisk_recomputeConnectionMetric(connection);

  for(i=0;i<PEISK_NQUEUES;i++) {
    connection->outgoingQueueFirst[i]=NULL;
    connection->outgoingQueueLast[i]=&connection->outgoingQueueFirst[i];
    connection->nQueuedPackages[i]=0;
  }

  if(!connection->routingTable) connection->routingTable = peisk_hashTable_create(PeisHashTableKey_Integer);

  peisk_hashTableIterator_first(connection->routingTable,&iterator);
  while(peisk_hashTableIterator_next(&iterator)) {
    peisk_hashTableIterator_value_generic(&iterator,&destination,&routingInfo);
    routingInfo->hops = 255;
    routingInfo->sequenceNumber = 0;
  }
}
/** Intializes and returns the next usable connection structure. Returns NULL on error. */
PeisConnection *peisk_newConnection() {
  int index;

  /* Find a free connection to use */
  for(index=0;index<PEISK_MAX_CONNECTIONS;index++)
    if(peiskernel.connections[index].id == -1) break;
  if(index == PEISK_MAX_CONNECTIONS) {
    fprintf(stdout,"peisk: too many connections - refusing to make a new connection\n");
    return NULL;
  }
  if(index >= peiskernel.highestConnection) peiskernel.highestConnection=index+1;

  /* Initialize it properly */
  peisk_initConnection(&peiskernel.connections[index]);

  return &peiskernel.connections[index];
}
void peisk_freeConnection(PeisConnection *connection) {
  int i;

  connection->id=-1;
  if(connection->neighbour.id != -1) {
    PeisConnectionMgrInfo *connMgrInfo=peisk_lookupConnectionMgrInfo(connection->neighbour.id);
    if(connMgrInfo) {
      /* Recompute when we want to (attempt to) connect to him next time */
      PEISK_ASSERT(connMgrInfo->directlyConnected,
		   ("Closing a connection to a neighbour %d we belive we are not connected to... %x\n",
		    connection->neighbour.id,(unsigned int)(intA) connMgrInfo));
      connMgrInfo->nTries++;
      connMgrInfo->nextRetry = peisk_timeNow + 1.0 * connMgrInfo->nTries;

      /*printf("Closing a connection to %d, see if that was the last one\n",connection->neighbour.id);*/
      /* See if we have any other connections point to this host, if so keep him marked as directly connected */
      for(i=0;i<=peiskernel.highestConnection;i++)
	if(peiskernel.connections[i].id != -1 &&
	   peiskernel.connections[i].neighbour.id == connection->neighbour.id) break;
      if(i == peiskernel.highestConnection+1) {
	/*printf("Marking %d as not anymore directly connected\n",connection->neighbour.id);*/
	connMgrInfo->directlyConnected = 0;
      } /*else 
	  printf("nope, there's still more connections (eg. #%d) to him\n",i);*/
    }
  }
  connection->neighbour.id = -1;
}


void peisk_abortConnect(PeisConnection *connection) {
  int i;

  connection->id=-1;
  
  /* Check all other connections and change connectionManagerInfo 
     to be not directlyConnected if neccessary */
  PeisHostInfo *hostinfo = &connection->neighbour;
  PeisConnectionMgrInfo *connMgrInfo=peisk_lookupConnectionMgrInfo(hostinfo->id);
  if(connMgrInfo) {
    for(i=0;i<=peiskernel.highestConnection;i++) {
      if(peiskernel.connections[i].id != -1 &&
	 &peiskernel.connections[i] != connection &&
	 peiskernel.connections[i].neighbour.id == hostinfo->id) break;
    }
    if(i == peiskernel.highestConnection+1) {
      connMgrInfo->directlyConnected = 0;
      connMgrInfo->nTries++;
      connMgrInfo->nextRetry = peisk_timeNow + 1.0 * connMgrInfo->nTries;
    }
  }
  connection->neighbour.id = -1;
}

void peisk_connection_processOutgoing(PeisConnection *connection) {
  int i,len,pkgid;
  PeisQueuedPackage *qpackage, **prev;
  double t0 = peisk_timeNow;

  int queue;
  int loopCheck;

  /* Check if queue is working correctly, otherwise send no further packages on it */
  if(connection->type == eUDPConnection && connection->connection.udp.status != eUDPConnected) return;

  /* Process all outgoing queues, take special care of the PENDING queue since it is special. */
  for(queue=0;queue<PEISK_NQUEUES;queue++) {
    qpackage=connection->outgoingQueueFirst[queue];
    prev=&connection->outgoingQueueFirst[queue];

    loopCheck=0; /* For debugging */
    /* Step through all packages */
    for(;qpackage&&loopCheck<2000;loopCheck++) {

      PEISK_ASSERT(qpackage->package.header.sync==PEISK_SYNC,("malformed outgoing package"));
      /* Treat the PENDING queue in a special way, we only send packages if their 
	 deadline for ACK have passed and delete them if they are too old */
      if(queue == PEISK_QUEUE_PENDING) {
	/*printf("pending package->t0 = %.3f, my t0 = %.3f, retries: %d\n",qpackage->t0,t0,qpackage->retries);*/
	if(qpackage->t0 > t0) goto pendingContinue; /* Pending package has not yet expired */
	if(qpackage->retries > PEISK_PENDING_MAX_RETRIES) {
	  /* Give up this package and remove it from pending queue and add to list of free qpackages */
	  if(peisk_printLevel & PEISK_PRINT_PACKAGE_ERR)
	    printf("Giving up on ackID %x, retries: %d\n",ntohl(qpackage->package.header.ackID),qpackage->retries);
	  /* If there was an acknowledgement hook registered for this package,
	     invoke it */
	  if(qpackage->hook) {
	    PEISK_ASSERT(qpackage->nHooks<PEISK_MAX_ACKHOOKS,("Found a queue package with %d hooks\n",qpackage->nHooks));
	    for(i=0;i<qpackage->nHooks;i++)
	    (qpackage->hook[i])(0,qpackage->package.header.datalen,&qpackage->package,qpackage->hookData[i]);
	  }

	  connection->nQueuedPackages[queue]--;
	  *prev=qpackage->next;
	    /* Be carefull when removing the lastmost element of the queues */
	  if(&qpackage->next == connection->outgoingQueueLast[queue]) {
	    connection->outgoingQueueLast[queue]=prev;
	  }

	  qpackage->next = peiskernel.freeQueuedPackages;
	  peiskernel.freeQueuedPackages = qpackage;

	  qpackage=*prev;

	  /* OLD
	  if(connection->outgoingQueueFirst[queue] == NULL)
	    PEISK_ASSERT(connection->outgoingQueueLast[queue] == &connection->outgoingQueueFirst[queue],
	    ("I've just created a bad queue\n"));*/
	  continue;
	}
	/* Else, continue on by resending the package again */
	if(peisk_printLevel & PEISK_PRINT_PACKAGE_ERR)
	  printf("Sending package with ackID %d again\n",ntohl(qpackage->package.header.ackID));
	/* Send with a fresh identifier to avoid problems with loop detection */
	do { pkgid=rand() & 0x7fffffff; } while(peisk_isRepeated(pkgid));
	qpackage->package.header.id = htonl(pkgid);
      }

      errno=0;

      len=sizeof(PeisPackageHeader)+ntohs(qpackage->package.header.datalen);
      if(peisk_connection_sendAtomic(connection,&qpackage->package,len) != 0) 
	return;     /* Failed to send, no need to continue processing queue */

      /* Package successfully sent */
      /* Update statistics */
      /*printf("send: %d ON %d\n",ntohl(qpackage->package.header.id),queue);*/
      connection->totalOutgoing += len;
      peiskernel.outgoingTraffic += len;
      connection->outgoingTraffic += len;

      /* Append package to usefullTraffic if the port is not meta port.
	 Applies to both connection and the source and destination ConnMgrInfo's
      */
      int destination = ntohl(qpackage->package.header.destination);
      int port = ntohs(qpackage->package.header.port);
      /*int source = ntohl(qpackage->package.header.source);*/

      if(port >= 0 && port < PEISK_HIGHEST_PORT_NUMBER && peisk_metaPorts[port] == 0) {
	PeisConnectionMgrInfo *connMgrInfo = peisk_lookupConnectionMgrInfo(destination);
	if(connMgrInfo)
	  connMgrInfo->lastUsefullTraffic += len;
	if(connection->neighbour.id != destination) {
	  connMgrInfo = peisk_lookupConnectionMgrInfo(connection->neighbour.id);
	  if(connMgrInfo)
	    connMgrInfo->lastUsefullTraffic += len;
	}
	connection->lastUsefullTraffic  += len;
      }



      if(queue == PEISK_QUEUE_PENDING) {
	/* update "pending" meta information */
	qpackage->retries++;
	qpackage->t0 = t0 + PEISK_PENDING_RETRY_TIME * qpackage->retries;
      pendingContinue:
	prev=&qpackage->next;
	qpackage=qpackage->next;
      } else {
	/* Remove it from current queue */
	connection->nQueuedPackages[queue]--;
	*prev=qpackage->next;
	/* Be carefull when removing the lastmost element of the queues */
	if(&qpackage->next == connection->outgoingQueueLast[queue])
	  connection->outgoingQueueLast[queue]=prev;

	/*printf("package sent, %d %d\n",ntohs(qpackage->package.header.flags),ntohl(qpackage->package.header.source));*/

	/* Package was sent, remove it from queue and add it (back) to the free packages OR
	   to the pending queue - depending on mode */
	if((ntohs(qpackage->package.header.flags) & PEISK_PACKAGE_REQUEST_ACK) &&
	   ntohl(qpackage->package.header.source) == peisk_id) {
	  /* Add to pending packages */
	  connection->nQueuedPackages[PEISK_QUEUE_PENDING]++;
	  /*printf("add pending: %d\n",connection->nQueuedPackages[PEISK_QUEUE_PENDING]);*/
	  qpackage->t0 = t0 + PEISK_PENDING_RETRY_TIME;
	  qpackage->retries = 0;
	  qpackage->next = NULL;

	  *connection->outgoingQueueLast[PEISK_QUEUE_PENDING]=qpackage;
	  connection->outgoingQueueLast[PEISK_QUEUE_PENDING]=&qpackage->next;

	  PEISK_ASSERT(connection->outgoingQueueFirst[PEISK_QUEUE_PENDING] != NULL,("Pending queue is broken\n"));

	  /* Remove BULK flag from messages in pending queue. Should help when important bulk packages 
	     are routed. (Then they are only bulk on the first try. Those that fail 
	     are send in normal mode to increase chance of delivery). */
	  qpackage->package.header.flags &= ~PEISK_PACKAGE_BULK;
	} else {
	  /* Add to free packages */
	  qpackage->next = peiskernel.freeQueuedPackages;
	  peiskernel.freeQueuedPackages = qpackage;
	}

	/* Continue working on the next package... */
	qpackage=*prev;
      }
    }
    PEISK_ASSERT(loopCheck<20000,("Loop in outgoing packages for queue %d?\n",queue));
  }

  if(connection->outgoingQueueFirst[PEISK_QUEUE_PENDING] == NULL)
    PEISK_ASSERT(connection->outgoingQueueLast[PEISK_QUEUE_PENDING] == 
		 &connection->outgoingQueueFirst[PEISK_QUEUE_PENDING],
		 ("after processQueeu - bad pending queue\n"));

}

int peisk_connection_processIncomming(PeisConnection *connection) {
  int datalen;
  int port,destination,source;
  PeisRoutingInfo *routingInfo;
  PeisConnectionMgrInfo *connMgrInfo;
  PeisHookList *hooklist;
  PeisConnection *outConnection;
  static PeisPackage package;

  /* Read incomming package from link layer. If no package was found, return */
  if(!peisk_connection_receiveIncomming(connection,&package)) return 0;

  /* First, check if message already seen */
  if(peisk_isRepeated(ntohl(package.header.id)))
	return 1;

  /* Add message to loop detection tables */
  peisk_markAsRepeated(ntohl(package.header.id));

  /* See if ACK is requested and if the package is aimed at us */
  if(ntohs(package.header.flags) & PEISK_PACKAGE_REQUEST_ACK && ntohl(package.header.destination) == peiskernel.id) {
    if(ntohs(package.header.port) == PEISK_PORT_ACKNOWLEDGEMENTS) {
      /*peisk_sendBulkAcknowledgement(ntohl(package.header.source),package.header.ackID);*/
    } else {
      peisk_sendBulkAcknowledgement(ntohl(package.header.source),package.header.ackID);
    }
  }

  /* If message is aimed at us then discard package data if ackID already seen, otherwise mark ackID as seen */
  if(ntohl(package.header.destination) == peiskernel.id &&
     package.header.ackID != package.header.id) {
    if(peisk_isRepeated(ntohl(package.header.ackID))) return 1;
    peisk_markAsRepeated(ntohl(package.header.ackID));
  }

  /* Set global variable for last connection, in case message is
     intercepted by special service routines  that require to know
     from which connection the package came (eg. routing routines). */
  peisk_lastConnection = connection->id;

  destination = ntohl(package.header.destination);
  port = ntohs(package.header.port);
  source = ntohl(package.header.source);
  datalen = ntohs(package.header.datalen);

  if(ntohs(package.header.seqlen) > 0) {
    /* Handle long messages */
    if(destination == peiskernel.id)
      peisk_assembleLongMessage(&package.header,datalen,(void*) package.data);
  } else {
    /* It's not a long message, so allow it to be intercepted by local routines */
    peisk_lastPackage = &package.header;
    hooklist=peisk_lookupHook(port);
    while(hooklist) {
      /*printf("%s triggered. source=%d dest=%d\n",hooklist->name,source,destination);*/
      if((hooklist->hook)(port,destination,source,datalen,package.data))
	return 1; /* If nonzero return code then stop processing this
		     package */
      hooklist=hooklist->next;
    }
  }

  /* Append package to usefullTraffic if the port is not meta port.
     Applies to both connection and the source and destination ConnMgrInfo's
  */
  if(port >= 0 && port < PEISK_HIGHEST_PORT_NUMBER && peisk_metaPorts[port] == 0) {
    connection->lastUsefullTraffic  += sizeof(package.header) + datalen;
    connMgrInfo = peisk_lookupConnectionMgrInfo(source);
    if(connMgrInfo)
      connMgrInfo->lastUsefullTraffic += sizeof(package.header) + datalen;
    if(connection->neighbour.id != -1 && source != connection->neighbour.id) {
      connMgrInfo = peisk_lookupConnectionMgrInfo(connection->neighbour.id);
      if(connMgrInfo)
	connMgrInfo->lastUsefullTraffic += sizeof(package.header) + datalen;
    }
  }

  /* Don't route package if it has reached maximum number of hops */
  if(package.header.hops >= PEISK_MAX_HOPS-1) return 1;

  /* Propagate message */
  switch(package.header.type) {
  case ePeisLinkPackage:
	/* Linklevel packages are not propagated */
	break;
  case ePeisBroadcastPackage:
	/* Propagation of broadcasted packages */
	package.header.hops++;

	peisk_incommingBroadcastConnection = connection;
	peisk_sendBroadcastPackage(&package.header,ntohs(package.header.datalen),package.data,PEISK_SEND_ROUTED);
	peisk_incommingBroadcastConnection = NULL;
	break;
  case ePeisDirectPackage:
	/* Propagation of all routed packages */
	package.header.hops++;

	if(destination != peiskernel.id) {
	  /* If package is supposed to be routed then propagate it. */

	  if(peisk_hashTable_getValue(peiskernel.routingTable,(void*)(long)ntohl(package.header.destination),(void**)(void*)&routingInfo) == 0) {
	    if(routingInfo->connection == NULL) {
	      if(peisk_debugRoutes)
		printf("Dropping package to special host %d\n",ntohl(package.header.destination));
	    } else {
	      /* Found a valid route to take */
	      outConnection = routingInfo->connection;
	      /* \todo See if socket is still open */
	      /* Propagate message */
	      peisk_connection_sendPackage(outConnection->id,
					   &package.header,
					   ntohs(package.header.datalen),
					   (void*)package.data,
					   PEISK_SEND_ROUTED);
	    }
	  } else {
	    if(peisk_debugRoutes)
	      printf("Cannot route package. Unknown route to host %d\n",ntohl(package.header.destination));
	  }
	}
	break;
  }

  return 1;
}


void peisk_hton_hostInfo(PeisHostInfo *hostInfo) {
  int i;
  hostInfo->id = htonl(hostInfo->id);
  hostInfo->magic = htonl(hostInfo->magic);
  hostInfo->networkCluster = htonl(hostInfo->networkCluster);

  for(i=0;i<hostInfo->nLowlevelAddresses;i++)
    switch(hostInfo->lowAddr[i].type) {
    case ePeisTcpIPv4:
      hostInfo->lowAddr[i].addr.tcpIPv4.port = ntohl(hostInfo->lowAddr[i].addr.tcpIPv4.port);
      break;
    case ePeisUdpIPv4:
      hostInfo->lowAddr[i].addr.udpIPv4.port = ntohl(hostInfo->lowAddr[i].addr.udpIPv4.port);
      break;
    case ePeisBluetooth:
      hostInfo->lowAddr[i].addr.bluetooth.port = htons(hostInfo->lowAddr[i].addr.bluetooth.port);
      break;
    case ePeisTcpIPv6: PEISK_ASSERT(0,("TcpIPv6 support not yet implemented")); break;
    }
}

void peisk_ntoh_hostInfo(PeisHostInfo *hostInfo) {
  int i;
  hostInfo->id = ntohl(hostInfo->id);
  hostInfo->magic = ntohl(hostInfo->magic);
  hostInfo->networkCluster = ntohl(hostInfo->networkCluster);

  for(i=0;i<hostInfo->nLowlevelAddresses;i++)
    switch(hostInfo->lowAddr[i].type) {
    case ePeisTcpIPv4:
      hostInfo->lowAddr[i].addr.tcpIPv4.port = ntohl(hostInfo->lowAddr[i].addr.tcpIPv4.port);
      break;
    case ePeisUdpIPv4:
      hostInfo->lowAddr[i].addr.udpIPv4.port = ntohl(hostInfo->lowAddr[i].addr.udpIPv4.port);
      break;
    case ePeisBluetooth:
      hostInfo->lowAddr[i].addr.bluetooth.port = ntohs(hostInfo->lowAddr[i].addr.bluetooth.port);
      break;
    case ePeisTcpIPv6: PEISK_ASSERT(0,("TcpIPv6 support not yet implemented")); break;
    }
}

void peisk_handleBroadcastedHostInfo(PeisHostInfo *newHostInfo) {
  PeisHostInfo *oldHostInfo;
  PeisHostInfo *hostInfo;
  PeisConnectionMgrInfo *connMgrInfo;

  oldHostInfo = peisk_lookupHostInfo(newHostInfo->id);
  if(!oldHostInfo) {
    /* This is a new host */    
    hostInfo = (PeisHostInfo*) malloc(sizeof(PeisHostInfo));
    *hostInfo = *newHostInfo;
    hostInfo->lastSeen = peisk_timeNow;
    peisk_insertHostInfo(newHostInfo->id,hostInfo);
    
    connMgrInfo = peisk_lookupConnectionMgrInfo(hostInfo->id);
    if(!connMgrInfo) {
      /* Allocate and insert a new connMgrInfo unless we already have one */
      connMgrInfo = (PeisConnectionMgrInfo*) malloc(sizeof(PeisConnectionMgrInfo));
      peisk_initConnectionMgrInfo(connMgrInfo);
      peisk_insertConnectionMgrInfo(hostInfo->id,connMgrInfo);
    }
    connMgrInfo->nTries = 0;
    connMgrInfo->nextRetry = peisk_timeNow;
    

    /* Log information about host... if requested */
    if(peisk_printLevel & PEISK_PRINT_STATUS) {
      printf("peisk: found host ");
      peisk_printHostInfo(newHostInfo);
    }
  } else {
    /* Host existed before */
    /* TODO - handle any changes in the magic number??? */
    connMgrInfo = peisk_lookupConnectionMgrInfo(oldHostInfo->id);
    if(!connMgrInfo) {
      /* Allocate and insert a new connMgrInfo unless we already have one */
      connMgrInfo = (PeisConnectionMgrInfo*) malloc(sizeof(PeisConnectionMgrInfo));
      peisk_insertConnectionMgrInfo(oldHostInfo->id,connMgrInfo);	  
      peisk_initConnectionMgrInfo(connMgrInfo);
      connMgrInfo->nTries=0;
      connMgrInfo->nextRetry=peisk_timeNow;
      connMgrInfo->usefullTraffic=0;
      connMgrInfo->lastUsefullTraffic=0;
    }
    oldHostInfo->lastSeen=peisk_timeNow;
  }
}

void peisk_step() {
  int i;
  int received;
  int nPending;
  int maxInLoops;
  double timeElapsed;

  peisk_timeNow = peisk_gettimef();                          /** Timepoint when starting this step */
  timeElapsed = peisk_timeNow - peiskernel.lastStep;         /** Total elapsed time since start of last step */
  peiskernel.lastStep = peisk_timeNow;
  peiskernel.avgStepTime = peiskernel.avgStepTime * 0.9 + timeElapsed * 0.1;
  peiskernel.tick++;

  if(peisk_printLevel & PEISK_PRINT_STATUS) {
    PEISK_ASSERT(timeElapsed < 0.15, ("peisk_step() called to slowly. ts elapsed=%3.3f\n",timeElapsed));
  }

  /* Listen to the doShutdown request possibly setup by a signal handler */
  if(peiskernel.doShutdown == 1) { peisk_shutdown(); }

  //if(!peisk_int_isRunning) { fprintf(stderr,"peisk: peisk_step called when not running\n"); return; }

  /* Some actions are only performed unless we are in the process of shutting down */
  if(!peiskernel.doShutdown) {

    /* check for new incomming TCP and UDP connections */
    peisk_acceptTCPConnections();
    /*peisk_acceptUDPConnections();*/

    /* see if we have read new broadcasted hosts packages, but not too often */
    if(peiskernel.tick % 5 == 0)
      peisk_acceptUDPMulticasts();

    /* Attempt to connect to all autohosts if needed */
    for(i=0;i<peiskernel.nAutohosts;i++)
      if(peiskernel.autohosts[i].isConnected == -1 &&
	 peisk_timeNow - peiskernel.autohosts[i].lastAttempt > PEISK_AUTHOST_PERIOD) {
	peiskernel.autohosts[i].isConnected =
	  peisk_connect(peiskernel.autohosts[i].url,PEISK_CONNECT_FLAG_FORCE_BCAST|PEISK_CONNECT_FLAG_FORCED_CL) 
	  == NULL ? 0 : 1;
	/** \todo If connections take long time these autoconnection might make the step function unresponsive */
	peiskernel.autohosts[i].lastAttempt = peisk_timeNow;
      }
  }

  /*                                                     */
  /* Accept packages from peers and send queued messages */
  /*                                                     */

  /* For each queue, send all outgoing messages and look for incomming messages.
	 Keep processing each queue as long as we have received some message.
  */

  received=1;
  maxInLoops=10;
  while(received && --maxInLoops > 0) {
    received=0;
    nPending=0;


    /*                              */
    /* Check for incomming packages */
    /*                              */

    /* Process the incomming queue BEFORE the outgoing queue, that way we have
       the chance to respond to any new incoming messages immediatly. */
    /* We only check for packages unless we are currently shutting down. This should ensure a
       faster clearing of the queues. */
    if(!peiskernel.doShutdown)
      for(i=0;i<=peiskernel.highestConnection;i++)
	if(peiskernel.connections[i].id != -1) {
	  if(peisk_connection_processIncomming(&peiskernel.connections[i]))
	    received=1;
	}

    /*                                                */
    /* Process the outgoing queues of each connection */
    /*                                                */
    for(i=0;i<=peiskernel.highestConnection;i++)
        if(peiskernel.connections[i].id != -1) {
	  peisk_connection_processOutgoing(&peiskernel.connections[i]);
      }
  }

  /* Some actions are only performed unless we are in the process of shutting down */
  if(!peiskernel.doShutdown) {

    /** Call all periodic functions if sufficient time has passed */
    for(i=0;i<=peiskernel.highestPeriodic;i++)
      if(peiskernel.periodics[i].periodicity != -1.0 &&
	    peiskernel.periodics[i].last + peiskernel.periodics[i].periodicity <= peisk_timeNow) {
	/*printf("Invoking %s\n",peiskernel.periodics[i].name);*/
	/* Set last call to this timepoint, this will make periodics
	   run atmost once per step (avoiding multiple calls to "catch
	   up" when we have been too slow. Also allows gradual
	   degradation when CPU usage is 100% */
	peiskernel.periodics[i].last = peisk_timeNow; /*+= peiskernel.periodics[i].periodicity;*/
	peiskernel.periodics[i].hook(peiskernel.periodics[i].data);
      }
  }
}

int peisk_broadcast(int port,int len,void *data) {
  return peisk_broadcastSpecialFrom(peiskernel.id,port,len,data,0);
}
int peisk_broadcastFrom(int from,int port,int len,void *data) {
  return peisk_broadcastSpecialFrom(from,port,len,data,0);
}
int peisk_broadcastSpecial(int port,int len,void *data,int flags) {
  return peisk_broadcastSpecialFrom(peiskernel.id,port,len,data,flags);
}
int peisk_broadcastSpecialFrom(int from,int port,int len,void *data,int flags) {
  int pkgid;
  PeisPackageHeader header;

  if(len > PEISK_MAX_PACKAGE_SIZE) {
    if(peisk_printLevel & PEISK_PRINT_PACKAGE_ERR)
      printf("peisk: error - attempt to broadcast too large message (%d bytes) on port %d\n",len,port);
    return 0;
  }

  /* Generate a random, previously unused id-number for this package */
  do { pkgid=rand() & 0x7fffffff; } while(peisk_isRepeated(pkgid));
  if(!data) len=0;

  header.id=htonl(pkgid);
  header.ackID = htonl(pkgid);
  header.type=ePeisBroadcastPackage;
  header.hops=1;
  header.datalen=htons(len);
  header.port=htons(port);
  header.destination=htonl(-1);
  header.source=htonl(from);
  header.flags = 0;
  header.seqlen=0;
  header.seqid=0;
  header.seqnum=0;

  /** Spoof that a broadcasted package is one hop older than it is,
      neccessary to avoid seeing broadcasted packages as coming from a
      direct neighbour (used when eg. intercepting broadcasted
      packages, modifying them and retransmitting) */
  if(flags & PEISK_BPACKAGE_SPOOF_HOPS) header.hops += 1;

  /** Mark this package as repeated since it's from local origin */
  peisk_markAsRepeated(pkgid);

  flags = flags & (PEISK_BPACKAGE_HIPRI | PEISK_BPACKAGE_BULK);

  peisk_sendBroadcastPackage(&header,len,(void*)data,flags);
  return 0;
}

int peisk_sendBroadcastPackage(PeisPackageHeader *header,int datalen,void *data,int flags) {
  int i,j,v;
  int nConnections;
  char used[PEISK_MAX_CONNECTIONS];


  /*
  if(ntohs(header->port) == PEISK_PORT_DEAD_HOST) {
    printf("sending a broadcasted package, port: %d len %d data: \n",ntohs(header->port),datalen);
    peisk_hexDump(data,datalen);
    }*/


  /* We use a stochasthic algorithm for this and do not use true broadcasts, that way
     the amount of bandwidth used can be reduced from E * V to N * 3
     where E is the number of edges in the total graph and V the
     average degree of each node (number of connections). This is a
     big saving when avg number of connections V > 3.
  */
  nConnections=0;
  for(j=0;j<=peiskernel.highestConnection;j++)
    if(peiskernel.connections[j].id != -1 && &peiskernel.connections[j] != peisk_incommingBroadcastConnection)
      nConnections++;
  memset(used, 0, sizeof(used));

  /*TODO: if peis X has an autoconnection to peis Y then Y should
    inherit a status that this is an autoconnection */

  /* Send message on all connections marked as having a forced
     broadcast */
  for(i=0;i<=peiskernel.highestConnection;i++)
    if(peiskernel.connections[i].id != -1 && peiskernel.connections[i].forceBroadcasts &&
       peisk_incommingBroadcastConnection != &peiskernel.connections[i]) {
      used[i]=1;
      nConnections--;
      peisk_connection_sendPackage(peiskernel.connections[i].id,header,datalen, data,flags);
    }

  /* Send message on PEISK_BROADCAST_CONNECTIONS number of random connections */
  for(i=0;i<PEISK_BROADCAST_CONNECTIONS && nConnections>0;i++) {
    /* This might look like an inefficient way to select a connection randomly but it
       guarantees the same chance of selecting each connection and guarantees not to use the
       same connection more than one time. */
    v=(rand()>>3)%nConnections;
    for(j=0;j<=peiskernel.highestConnection;j++)
      if(peiskernel.connections[j].id != -1 && !used[j] && peisk_incommingBroadcastConnection != &peiskernel.connections[j]) {
	if(!v) break;
	v--;
      }
    /*PEISK_ASSERT(j < PEISK_MAX_CONNECTIONS && peiskernel.connections[j].id != -1 && !used[j],
		 ("error in random selection of connections - bad algorithm?\n"));
		 if(j >= PEISK_MAX_CONNECTIONS) exit(0);*/
    used[j]=1;
    nConnections--;
    /*printf("Sending broadcased package on %d\n",peiskernel.connections[j].neighbour.id);*/
    peisk_connection_sendPackage(peiskernel.connections[j].id,header,datalen, data,flags);
  }

  return 0;
}

int peisk_sendLinkPackage(int port,PeisConnection *connection,int len,void *data) {
  int pkgid;
  PeisPackageHeader header;

  if(len > PEISK_MAX_PACKAGE_SIZE) {
    if(peisk_printLevel & PEISK_PRINT_PACKAGE_ERR)
      printf("peisk: error - attempt to send too large link layer message on port %d\n",port);
    return 0;
  }
  /* Generate a random, previously unused id-number for this package */
  do { pkgid=rand() & 0x7fffffff; } while(peisk_isRepeated(pkgid));
  if(!data) len=0;
  header.id=htonl(pkgid);
  header.ackID = htonl(pkgid);
  header.type=ePeisLinkPackage;
  header.hops=1;
  header.datalen=htons(len);
  header.port=htons(port);
  header.destination=htonl(-1);
  header.source=htonl(peiskernel.id);
  header.flags = 0;
  header.seqlen=0;
  header.seqid=0;
  header.seqnum=0;

  /** Mark this package as repeated since it's from local origin */
  peisk_markAsRepeated(pkgid);
  peisk_connection_sendPackage(connection->id,&header,len,(void*) data,0);
  return 0;
}

int peisk_sendMessage(int port,int destination,int len,void *data,int flags) {
  return peisk_sendMessageFrom(peiskernel.id,port,destination,len,data,flags);
}

int peisk_sendMessageFrom(int from,int port,int destination,int len,void *data,int flags) {
  int pkgid,i,seqlen,thislen;
  PeisPackageHeader header;
  PeisConnection *connection;
  double fillrate, dropchance;
  PeisRoutingInfo *routingInfo;

  /* See if we have a direct connection to the destination... */
  for(i = 0;i<=peiskernel.highestConnection;i++) {
    if(peiskernel.connections[i].id != -1 &&
       peiskernel.connections[i].neighbour.id == destination) break;
  }
  if(i != peiskernel.highestConnection+1) {
    /* Direct connection found */
    connection = &peiskernel.connections[i];
  } else {
    /* Direct connection not found */
    if(peisk_hashTable_getValue(peiskernel.routingTable,(void*)(long)destination,(void**)(void*)&routingInfo) != 0) {
      /* Destination unknown */
      if(peisk_printLevel & PEISK_PRINT_PACKAGE_ERR)
	printf("peisk: warning: sending to %d failed, no route to destination (no route recorded)\n",destination);
      return -1;
    }
    connection = routingInfo->connection;
    if(!connection) {
      if(peisk_printLevel & PEISK_PRINT_PACKAGE_ERR)
	printf("peisk: warning: sending to %d failed, no route to destination (NULL pointer)\n",destination);
      return -1;
    }

    /* Schedule to create a direct connection towards target if possible */
    if(peiskernel.nDirConnReqs < PEISK_MAX_DIR_CONN_REQS) {
      for(i=0;i < peiskernel.nDirConnReqs;i++)
	if(peiskernel.dirConnReqs[i] == destination) break;
      if(peiskernel.nDirConnReqs == 0 || i != peiskernel.nDirConnReqs) {
	/*printf("Scheduling connection towards %d, request: %d\n",destination,peiskernel.nDirConnReqs);*/
	peiskernel.dirConnReqs[peiskernel.nDirConnReqs++]=destination;
      }
    }
  }
  /* See if socket towards target is still open */
  if(connection->id == -1) {
    /** Socket towards destination is broken, this counts as a bug! */
    PEISK_ASSERT(0,("Routing table for target %d points to use a broken connection (%x)\n",
		    destination,(unsigned int)(intA) connection));
    return -1;
  }

  /* Generate a random, previously unused id-number for this package */
  do { pkgid=rand() & 0x7fffffff; } while(peisk_isRepeated(pkgid));

  header.id=htonl(pkgid);
  header.ackID = htonl(pkgid);
  header.type=ePeisDirectPackage;
  header.hops= from == peiskernel.id? 1 : 2;
  header.datalen=htons(len);
  header.port=htons(port);
  header.source=htonl(from);
  header.destination=htonl(destination);
  header.seqlen=0;
  header.seqid=0;
  header.seqnum=0;
  header.flags = htons(flags);

  /* If we are requesting ACK's then make sure to use a unique ackID */
  if(flags & PEISK_PACKAGE_REQUEST_ACK) {
    do { pkgid=rand() & 0x7fffffff; } while(peisk_isRepeated(pkgid));
    header.ackID = htonl(pkgid);
    peiskernel.lastGeneratedAckID = header.ackID;
  }


  /* If message is small then send as a single package */
  if(len <= PEISK_MAX_PACKAGE_SIZE) {

    /* Always *attempt* to send small packages, they might just fit in */

    /* Mark this package as from local origin */
    peisk_markAsRepeated(pkgid);
    return peisk_connection_sendPackage(connection->id,&header,len,(void*)data,0);
  }


  /* Otherwise, message is long so divide message into multiple packages */
  seqlen = (len + PEISK_MAX_PACKAGE_SIZE - 1) / PEISK_MAX_PACKAGE_SIZE;
  header.seqlen = htons(seqlen);

  header.seqid=htons((rand() % 32767)+1);
  /* Force packages to use BULK transfer method when large */
  flags |= PEISK_PACKAGE_BULK;
  header.flags = htons(flags);

  /* See if *all* packages fit into queue, otherwise discard whole package */
  if(connection->nQueuedPackages[PEISK_QUEUE_BULK] + seqlen >= PEISK_MAX_QUEUE_SIZE) {
    if(peisk_printLevel & PEISK_PRINT_PACKAGE_ERR)
      printf("peisk: warning - cannot fit large package into queue, discarding it\n");
    return -1; /* Sorry, too many packages... */
  }
  /* Adjust for the use of RED (Random Early Detection) */
  fillrate=peisk_connection_fillrate(connection,PEISK_QUEUE_BULK) + seqlen * 1.0 / PEISK_MAX_QUEUE_SIZE;
  /*printf("fillrate: %f\n",fillrate);*/
  if(fillrate > 0.5) {
    if(fillrate > 1.0) return -1; /* No chance at all we can fit all of the package */
    /* Compute the chance that none of the packages will be dropped */
    dropchance = 1.0 - pow(1.0 - (2.0 * (fillrate - 0.5)), seqlen);
    /* If it's too small (< 80%) then drop the whole package */
    if(dropchance < 0.80) return -1;
  }

  PeisAcknowledgementHook *oldHookZero=NULL;
  void *oldDataZero=NULL;
  int oldNHooks;
  PeisLargePackageHookData *hookData;

  if(peiskernel.withAckHook) {
    oldHookZero = peiskernel.withAckHook[0];
    oldDataZero = peiskernel.withAckHookData[0];
    oldNHooks = peiskernel.nAckHooks;

    hookData = (PeisLargePackageHookData *) malloc(sizeof(PeisLargePackageHookData));
    for(i=0;i<PEISK_MAX_ACKHOOKS;i++) {
      hookData->hooks[i] = peiskernel.withAckHook[i];
      hookData->users[i] = peiskernel.withAckHookData[i];
    }
    hookData->nHooks = peiskernel.nAckHooks;
    hookData->counter = seqlen;
    hookData->success = 1;
    hookData->data = (void*) malloc(len+sizeof(PeisPackageHeader));
    memcpy(hookData->data,(void*)&header,sizeof(PeisPackageHeader));
    hookData->datalen = len;
    memcpy(hookData->data+sizeof(PeisPackageHeader),data,len);
    
    peiskernel.withAckHook[0] = peisk_largePackageHook;
    peiskernel.withAckHookData[0] = (void*) hookData;
    peiskernel.nAckHooks = 1;
  }

  for(i=0;i<seqlen;i++) {
    header.seqnum=htons(i);
    /* Generate a random, previously unused id-number for this package */
    do { pkgid=rand() & 0x7fffffff; } while(peisk_isRepeated(pkgid));
    header.id=htonl(pkgid);

    /*printf("Sending large message, seqnum: %d (%x) seqid: %d, seqlen=%d (%x), id: %d\n",
      ntohs(header.seqnum),header.seqnum,ntohs(header.seqid),seqlen,header.seqlen,ntohl(header.id));*/

    /* If we are requesting ACK's then make sure to use a unique ackID
       _for each package part_ */
    if(flags & PEISK_PACKAGE_REQUEST_ACK) {
      do { pkgid=rand() & 0x7fffffff; } while(peisk_isRepeated(pkgid));
      header.ackID = htonl(pkgid);
    }

    if(i == seqlen - 1) thislen = len - (seqlen - 1)*PEISK_MAX_PACKAGE_SIZE;
    else thislen = PEISK_MAX_PACKAGE_SIZE;
    header.datalen=htons(thislen);
    /* Mark this package as from local origin */
    peisk_markAsRepeated(pkgid);
    /* \todo Is it an error to use specialFlags=0 for all these sequenced packages?? */
    peisk_connection_sendPackage(connection->id,&header,thislen,data+i*PEISK_MAX_PACKAGE_SIZE,0);
  }
  /* Restore any old hooks */
  if(peiskernel.withAckHook) {
    peiskernel.withAckHook[0] = oldHookZero;
    peiskernel.withAckHookData[0] = oldDataZero;
    peiskernel.nAckHooks = oldNHooks;
  }

  return 0; /* Success */
}

void peisk_largePackageHook(int success,int datalen,PeisPackage *package,void *user) {
  int i;
  PeisLargePackageHookData *hookData = (PeisLargePackageHookData *) user;
  PEISK_ASSERT(hookData->counter > 0,("Incorrect counter in a largePackageHookData\n"));
  hookData->counter--;
  if(!success) hookData->success = 0;
  if(hookData->counter == 0) {
    /* Hook is finished */
    for(i=0;i<hookData->nHooks;i++)
      (hookData->hooks[i])(hookData->success,hookData->datalen,(PeisPackage *)hookData->data,hookData->users[i]);
    free(hookData->data);
    free(hookData);
  }  
}

double peisk_connection_fillrate(PeisConnection *connection,int priority) {
  int i;
  double fillrate = 0.0;
  for(i=0;i<priority;i++)
    fillrate += connection->nQueuedPackages[i] * 1.0 / PEISK_MAX_QUEUE_SIZE;
  return fillrate;
}


int peisk_connection_sendPackage(int id,PeisPackageHeader *package,int datalen,void *data,int specialFlags) {
  int index;
  PeisConnection *connection;
  PeisQueuedPackage *qpackage;
  int priority;
  int i;

  /* Until we start using larger port numbers this is a usefull
     debugging check. */
  if(ntohs(package->port) > 100) {
    fprintf(stderr,"Attempting to send a package on invalid port %d\n",ntohs(package->port));
    return -2;
  }

  if(peisk_debugPackages) /* || ntohs(package->port) == PEISK_PORT_SET_REMOTE_TUPLE)*/
    fprintf(stdout,"peisk: OUT conn=%d id=%x port=%d hops=%d len=%d type=%d src=%d dest=%d\n",
	    id,ntohl(package->id),ntohs(package->port),package->hops,ntohs(package->datalen),package->type,
	    ntohl(package->source),ntohl(package->destination));
  peisk_out_packages[ntohs(package->port)]++;

  if(datalen != ntohs(package->datalen)) {
    fprintf(stderr,"peisk: peisk_connection_sendPackage datalen != package.datalen\n");
    return -2;
  }
  if(id == -1) {
    fprintf(stderr,"peisk: peisk_connection_sendPackage id %d invalid\n",id);
    return -2;
  }
  for(index=0;index<PEISK_MAX_CONNECTIONS;index++)
    if(peiskernel.connections[index].id == id) break;
  if(index == PEISK_MAX_CONNECTIONS)  {
    fprintf(stderr,"peisk: peisk_connection_sendPackage id %d invalid\n",id);
    return -2;
  }
  connection=&peiskernel.connections[index];

  priority=PEISK_QUEUE_NORMALPRI;
  if(package->type == ePeisLinkPackage) priority=PEISK_QUEUE_HIGHPRI; /* ugly hack */
  if(package->flags & PEISK_PACKAGE_BULK) priority=PEISK_QUEUE_BULK;
  if(package->flags & PEISK_PACKAGE_IS_ACK) priority=PEISK_QUEUE_HIGHPRI;
  if(package->port == PEISK_PORT_ACKNOWLEDGEMENTS) priority=PEISK_QUEUE_HIGHPRI;

  /* Find a QueuedPackage to copy this package to */
  qpackage = peiskernel.freeQueuedPackages;
  if(!qpackage) {
    static int totAllocatedPackages=0;
    if(totAllocatedPackages < PEISK_MAX_ALLOCATED_PACKAGES) {
      qpackage = (struct PeisQueuedPackage*) malloc(sizeof(struct PeisQueuedPackage));
      if(qpackage) totAllocatedPackages++;
    }
    if(!qpackage) {
      if(peisk_printLevel & PEISK_PRINT_PACKAGE_ERR)
	fprintf(stderr,"peisk:peisk_connection_sendPackage: failed to allocate new queue package (tot %d)\n  WARNING: dropping packages!\n",totAllocatedPackages);
      return -2;
    }
  }
  else
    peiskernel.freeQueuedPackages = peiskernel.freeQueuedPackages->next;

  /* Copy package to it */
  qpackage->package.header = *package;
  qpackage->package.header.sync = PEISK_SYNC;
  /* Also copy any hooks which have been requested for this package */
  qpackage->nHooks = peiskernel.nAckHooks;
  for(i=0;i<qpackage->nHooks;i++) {
    qpackage->hook[i] = peiskernel.withAckHook[i];
    qpackage->hookData[i] = peiskernel.withAckHookData[i];
  }

  /*if(peiskernel.withAckHook) {
    printf("Creating ack hook'ed package %x\n",ntohl(qpackage->package.header.ackID));
    }*/

  /* qpackage->package.header.datalen = htons(datalen); This is redundant? */
  qpackage->t0 = peisk_gettimef();
  if(data) {
    memcpy((void*)qpackage->package.data,data,datalen);
  }


  /* If we have too many packages then abort */
  if(connection->nQueuedPackages[priority] > PEISK_MAX_QUEUE_SIZE) {
    if(peisk_printLevel & PEISK_PRINT_PACKAGE_ERR)
      fprintf(stderr,"peisk:peisk_connection_sendPackage: too many packages, dropping new packages");
    goto sendPackage_failed;
  }

  /* Use the RED (Random Early Detection) congestion algorithm from TCP/IP here.
     This discards random packets with probability 0.0 for 50% fillrate going up to
     probability 1.0 for 100% fillrate.
     The problem here is the definition of "fillrate" since we have multiple queues.
     For now, I try by summing the fillrate for all higher (lower numbered) priority queues.
     This might give a fillrate > 100% but that's ok.
  */
  double fillrate = peisk_connection_fillrate(connection,priority);
  double dropchance = 2.0 * (fillrate - 0.5);
  if((rand() % 10000) / 10000.0 < dropchance) {
    if(peisk_printLevel & PEISK_PRINT_PACKAGE_ERR)
      /* Note - this is not exactly an error, remove this in the future... */
      fprintf(stderr,"peisk:peisk_connection_sendPackage: RED drop");
    goto sendPackage_failed;
  }

  /* Place package in queue */
  connection->nQueuedPackages[priority]++;
  qpackage->next = NULL;
  *(connection->outgoingQueueLast[priority])=qpackage;
  connection->outgoingQueueLast[priority]=&qpackage->next;

  /* Success */
  return 0; 
 sendPackage_failed:
  /* Failure, alert hook */
  if(qpackage->nHooks) {
    /* Trigger failure hook(s) for this package */
    PEISK_ASSERT(qpackage->nHooks<PEISK_MAX_ACKHOOKS,("Found a queue package with %d hooks\n",qpackage->nHooks));
    for(i=0;i<qpackage->nHooks;i++)
      (qpackage->hook[i])(0,qpackage->package.header.datalen,&qpackage->package,qpackage->hookData[i]);
  }
  return -1; 
}

int peisk_isRepeated(int pkgid) {
  int hash,index;

  PEISK_ASSERT(pkgid>0,("Invalid package ID %d, must be greater than zero\n",pkgid));

  hash=pkgid % PEISK_LOOPINFO_HASH_SIZE;
  index=peiskernel.loopHashTable[hash];
  if(index >= PEISK_LOOPINFO_SIZE  || index < -1) { if(peisk_printLevel & PEISK_PRINT_PACKAGE_ERR) printf("peisk: WARNING 0 isRepeated index=%d hash=%d\n",index,hash); return 0; }
  /*fprintf(stdout,"repeated?: id=%d, hash=%d, index=%d\n",pkgid,hash,index);*/
  while(index != -1 && peiskernel.loopTable[index].id != pkgid) {
	if(index >= PEISK_LOOPINFO_SIZE  || index < -1) { if(peisk_printLevel & PEISK_PRINT_PACKAGE_ERR) printf("peisk: WARNING 1 isRepeated index=%d\n",index); return 0; }
    index = peiskernel.loopTable[index].hashNext;
  }
  if(index == -1) return 0;
  else return 1;
}
void peisk_markAsRepeated(int pkgid) {
  int hash,index;
  PeisLoopInfo *loopInfo;

  if(peisk_isRepeated(pkgid)) return;

  index=peiskernel.nextLoopIndex;
  if(++peiskernel.nextLoopIndex >= PEISK_LOOPINFO_SIZE) peiskernel.nextLoopIndex=0;
  loopInfo=&peiskernel.loopTable[index];
  /* If package already used, then first free it from hashtable */
  if(loopInfo->id != -1) {
	if(loopInfo->hashNext != -1)
	  peiskernel.loopTable[loopInfo->hashNext].hashPrev=loopInfo->hashPrev;
    *(loopInfo->hashPrev) = loopInfo->hashNext;
  }
  loopInfo->id = pkgid;

  /* Insert this package into loop table */
  hash=pkgid % PEISK_LOOPINFO_HASH_SIZE;
  PEISK_ASSERT((hash>=0),("hash of pkgid %d is %d\n",pkgid,hash));
  loopInfo->hashNext = peiskernel.loopHashTable[hash];
  if(peiskernel.loopHashTable[hash] != -1)
    peiskernel.loopTable[peiskernel.loopHashTable[hash]].hashPrev = &loopInfo->hashNext;
  peiskernel.loopHashTable[hash]=index;
  loopInfo->hashPrev = &peiskernel.loopHashTable[hash];
}



void peisk_closeConnection(int id) {
  int i,j,index,queue;
  PeisConnection *connection;
  PeisQueuedPackage *qpackage;
  PeisQueuedPackage *next;
  PeisHashTableIterator iterator;
  PeisRoutingInfo *routingInfo;
  PeisRoutingInfo *routingInfo2;
  intA destination;
  int metric, sequenceNumber, magic;
  PeisConnection *bestConnection;

  PEISK_ASSERT(id != -1,("Attempting to close connection with invalid id\n"));

  /*printf("Closing connection %d\n",id);*/

  /* If we are closing connection to a currently open autohost then mark it as closed */
  for(i=0;i<peiskernel.nAutohosts;i++)
    if(peiskernel.autohosts[i].isConnected == id)
      peiskernel.autohosts[i].isConnected=-1;

  /* Figure out which connection structure it is we are closing */
  for(index=0;index<PEISK_MAX_CONNECTIONS;index++)
    if(peiskernel.connections[index].id == id) break;
  if(index == PEISK_MAX_CONNECTIONS) { fprintf(stderr,"peisk: attempting to close unknown connection\n"); return; }

  connection=&peiskernel.connections[index];

  if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
    printf("peisk: closing connection #%d going to %d\n",id,connection->neighbour.id);

  /* Free the connection structure,update connection info and mark any
     neighbour as possibly no longer connected */
  peisk_freeConnection(connection);




  /** \todo Send a goodbye message */
  /* Close the connection */
  switch(connection->type) {
  case eTCPConnection:
    close(connection->connection.tcp.socket);
    break;
  case eUDPConnection:
    close(connection->connection.udp.socket);
    break;
  case eBluetoothConnection:
    peisk_bluetoothCloseConnection(connection);
  default:  /** \todo Handle other connection types */
    {}
  }

  /* Recompute how many connections we are still using. */
  for(peiskernel.highestConnection=0,i=0;i<PEISK_MAX_CONNECTIONS;i++)
    if(peiskernel.connections[i].id != -1) peiskernel.highestConnection=i;

  /* Remove all old routes using this connection */
  /* Iterate over all routes that was tied to this connection */
  peisk_hashTableIterator_first(connection->routingTable,&iterator);
  while(peisk_hashTableIterator_next(&iterator)) {
    peisk_hashTableIterator_value_generic(&iterator,&destination,&routingInfo);
    /* Now lookup this destination in the main routing table */
    if(peisk_hashTable_getValue(peiskernel.routingTable,(void*)destination,(void**)(void*)&routingInfo) == 0) {
      if(routingInfo->connection == connection) {
	/* The main routing table pointed to use this connection for
	   that destination. Mark route as outdated. */
	metric = 250;
	sequenceNumber = routingInfo->sequenceNumber;
	magic = routingInfo->magic;
	bestConnection = NULL;
	
	/* Search through all other connections and 
	   update routingInfo if there is anyone else */
	for(j=0;j<PEISK_MAX_CONNECTIONS;j++)
	  if(peiskernel.connections[j].id != -1 &&
	     peisk_hashTable_getValue(peiskernel.connections[j].routingTable,
				      (void*)destination,(void**)(void*) &routingInfo2) == 0 &&
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
	/*
	if(metric < 250)
	  printf("Route to %d rescued to %d hops using connection #%d instead\n",destination,metric,bestConnection->id);
	else
	  printf("Route to %d is (temporarily?) lost\n",destination);
	*/

#ifdef OLD
	for(i=0;i<PEISK_MAX_CONNECTIONS;i++)
	  if(peiskernel.connections[i].id != -1 &&
	     peisk_hashTable_getValue(peiskernel.connections[i].routingTable,(void*)destination,(void**)(void*) &routingInfo2) == 0) {
	    /* Method 1: Reroute to this connection if it has exactly
	       not 3 hops. This does not create loops, but it fails to
	       recover a few valid routes under special
	       circumstances. */
	    if(routingInfo2->hops != 3 && routingInfo2->hops < routingInfo->hops) {
	      routingInfo->hops = routingInfo2->hops;
	      routingInfo->connection = &peiskernel.connections[i];
	    }
	  }
#endif

      }
    } else {
      if (peisk_printLevel & PEISK_PRINT_WARNINGS)
        printf("peisk: p2p.c: warning: main routing table is missing destination %d which existed in a connection to %d\n",(int)destination,connection->neighbour.id);
    }
  }

  


  /* DEBUG CHECK */
  peisk_hashTableIterator_first(peiskernel.routingTable,&iterator);
  while(peisk_hashTableIterator_next(&iterator)) {
    peisk_hashTableIterator_value_generic(&iterator,&destination,&routingInfo);
    if(routingInfo->connection == connection) {
      printf("WARNING - main routing table _still_ points to use the connection we are currently closing\n");
    }
  }


  /* Free all queued packages belonging to this queue, we don't need to care about
   any cleanup code (nPackages, lastPointer etc.) since this connection is dead anyway 
   But we DO need to trigger the hooks of any pending packages...
  */
  for(queue=0;queue<PEISK_NQUEUES;queue++) {
    for(qpackage=connection->outgoingQueueFirst[queue];qpackage;) {
      if(peisk_printLevel & PEISK_PRINT_PACKAGE_ERR)
	printf("Giving up on ackID %x, closed connection\n",ntohl(qpackage->package.header.ackID));
      if(qpackage->hook) {
	/* Trigger failure hook for this package */
	PEISK_ASSERT(qpackage->nHooks<PEISK_MAX_ACKHOOKS,("Found a queue package with %d hooks\n",qpackage->nHooks));
	for(i=0;i<qpackage->nHooks;i++)
	  (qpackage->hook[i])(0,qpackage->package.header.datalen,&qpackage->package,qpackage->hookData[i]);
      }
      next=qpackage->next;
      qpackage->next=peiskernel.freeQueuedPackages;
      peiskernel.freeQueuedPackages=qpackage;
      qpackage=next;
    }
  }

  /* Cleanup internal routing table of this connection */
  peisk_hashTableIterator_first(connection->routingTable,&iterator);
  while(peisk_hashTableIterator_next(&iterator)) {
    peisk_hashTableIterator_value_generic(&iterator,&destination,&routingInfo);
    routingInfo->hops = 255;
  }

  /* Trigger routing periodic to send to neighbours immediatly, but do not age hosts or resend queries */
  peisk_do_routing(2,NULL);
}





int peisk_assembleLongMessage(PeisPackageHeader *package,int datalen,void *data) {
  int i,j,seqlen,seqnum;
  int port,destination,source;
  PeisHookList *hooklist;

  /*printf("Assemble Long Message %d\n",ntohs(package->seqid));*/

  /** \todo If we get a new long message from a given sender then delete all old partial long messages from that sender */


  seqlen = ntohs(package->seqlen);
  seqnum = ntohs(package->seqnum);
  /*printf("Assembling package %d, seqnum %d\n",ntohs(package->seqid),seqnum);*/


  if(seqlen > 10000)
    fprintf(stderr,"peisk: warning - received a *very* long message (%d packages)\n",seqlen);

  /* First, see if package belongs to a buffer already allocated */
  for(i=0;i<PEISK_MAX_LONG_MESSAGES;i++)
    if(peiskernel.assemblyBuffers[i].seqid == ntohs(package->seqid)) break;
  if(i == PEISK_MAX_LONG_MESSAGES) {
    /* No it didn't, allocate a new buffer */

    /* First, see if there is one already with the exact right size of allocated buffers */
    for(i=0;i<PEISK_MAX_LONG_MESSAGES;i++)
      if(peiskernel.assemblyBuffers[i].seqid == 0 && peiskernel.assemblyBuffers[i].allocated_seqlen == seqlen)
	goto found_buffer;

    /* Otherwise, see if there is one already with a sufficiently large size of allocated buffers */
    for(i=0;i<PEISK_MAX_LONG_MESSAGES;i++)
      if(peiskernel.assemblyBuffers[i].seqid == 0 && peiskernel.assemblyBuffers[i].allocated_seqlen >= seqlen)
	goto found_buffer;

    /* Otherwise, just grab one that is free */
    for(i=0;i<PEISK_MAX_LONG_MESSAGES;i++)
      if(peiskernel.assemblyBuffers[i].seqid == 0) goto found_buffer;

    /* Ooops, we're out of buffers */
    if(peisk_printLevel & PEISK_PRINT_PACKAGE_ERR)
      fprintf(stderr,"peisk: warning - no free buffers for long messages\n");
    return 0;

  found_buffer:
    if(peiskernel.assemblyBuffers[i].allocated_seqlen < seqlen) {
      peiskernel.assemblyBuffers[i].allocated_seqlen = seqlen;
      peiskernel.assemblyBuffers[i].seqlen = seqlen;
      peiskernel.assemblyBuffers[i].received = realloc(peiskernel.assemblyBuffers[i].received,seqlen);
      peiskernel.assemblyBuffers[i].data = realloc(peiskernel.assemblyBuffers[i].data,seqlen*PEISK_MAX_PACKAGE_SIZE);
    }
    memset(peiskernel.assemblyBuffers[i].received,0,seqlen);
    peiskernel.assemblyBuffers[i].seqid=ntohs(package->seqid);
    /*printf("starting to assemble long message %d (num = %d, id = %d, ack id=%d)\n",peiskernel.assemblyBuffers[i].seqid,package->seqnum,package->id, package->ackID);*/
  }

  /*
  if(package->seqnum == 0)
	printf("Assembling. seqid=%d, seqlen=%d\t seqnum=%d, id = %d, ackID = %d\n",ntohs(package->seqid),seqlen,package->seqnum,package->id,package->ackID);
  */

  /*printf("seqlen: %d, aseqlen %d, seqnum: %d\n",seqlen,peiskernel.assemblyBuffers[i].seqlen,package->seqnum);*/

  if(seqlen > peiskernel.assemblyBuffers[i].seqlen) return 0;  /* Bad seqlen of this package */
  if(seqnum >= seqlen) return 0;                               /* Bad seqnum of this package */
  /** \todo check the case when we get too little data for package inside a long message */
  /* Compute total length of long message */
  if(seqnum == seqlen - 1) peiskernel.assemblyBuffers[i].totalsize = PEISK_MAX_PACKAGE_SIZE * (seqlen - 1) + datalen;

  peiskernel.assemblyBuffers[i].timeout = peisk_gettimef() + PEISK_TIMEOUT_LONG_MESSAGE;
  peiskernel.assemblyBuffers[i].received[seqnum] = 1;
  memcpy(peiskernel.assemblyBuffers[i].data+PEISK_MAX_PACKAGE_SIZE*seqnum,data,datalen);

  /*printf("Assembling seqid %d, seqlen %d\n",ntohs(package->seqid),seqlen);*/

  /* See if all packages has been received and give correct return code */
  for(j=0;j<seqlen;j++)
    if(peiskernel.assemblyBuffers[i].received[j] == 0) return 0;

  /*printf("FINISHED seqid %d, seqlen %d\n",ntohs(package->seqid),package->seqlen);*/

  /* Package has been received in full, see if it should be intercepted by us */
  peisk_lastPackage = package;
  port = ntohs(package->port);
  hooklist=peisk_lookupHook(port);

  /* Trigger all hooks registered to this long message */
  while(hooklist) {
    destination = ntohl(package->destination);
    source = ntohl(package->source);
    (hooklist->hook)(port,destination,source,peiskernel.assemblyBuffers[i].totalsize,peiskernel.assemblyBuffers[i].data);
    hooklist=hooklist->next;
  }
  peiskernel.assemblyBuffers[i].seqid = 0;
  return 1;
}

void peisk_periodic_clearAssemblyBuffers(void *data) {
  int i;
  double timenow=peisk_gettimef();
  for(i=0;i<PEISK_MAX_LONG_MESSAGES;i++)
    if(peiskernel.assemblyBuffers[i].seqid && peiskernel.assemblyBuffers[i].timeout < timenow)
      peiskernel.assemblyBuffers[i].seqid=0;
}

PeisConnection *peisk_lookupConnection(int connid) {
  int i;
  for(i=0;i<PEISK_MAX_CONNECTIONS;i++)
    if(peiskernel.connections[i].id == connid) return &peiskernel.connections[i];
  return NULL;
}

void peisk_deleteHost(int id,int why) {
  PeisDeadHostMessage message;
  PeisRoutingInfo *routingInfo;
  int i;
  PeisHostInfo *hostInfo;
  PeisConnectionMgrInfo *connMgrInfo;

  PEISK_ASSERT(id != peiskernel.id,("Attempting to delete ourselves (ouch!)"));
  if(id == peiskernel.id) return;

  /*printf("Host %d removed from ecology because %d\n",id,why);*/

  /* First remove host from tuplespace */
  peisk_deleteHostFromTuplespace(id);
  peisk_deleteHostFromSentSubscriptions(id);

  /* Find and remove the host from our known hosts */
  hostInfo = peisk_lookupHostInfo(id);
  if(hostInfo) {
    peisk_hashTable_remove(peiskernel.hostInfoHT,(void*)(intA)id);
    free(hostInfo);
  }
  /* And from connection manager information structures */
  connMgrInfo = peisk_lookupConnectionMgrInfo(id);
  if(connMgrInfo) {
    //printf("Removing connection manager structure %p to %d\n",(void*)connMgrInfo,id);
    peisk_hashTable_remove(peiskernel.connectionMgrInfoHT,(void*)(intA)id);
    free(connMgrInfo);
  }

  if(why == PEISK_DEAD_ROUTE || why == PEISK_DEAD_REBORN) {
    /* Send an exit notification to this host specifically, this is to
       ensure that he does not belive that we are still alive in case
       the communication is broken in an asymmetric fashion. In normal
       cases, this message will not arrive. In asymmetric cases it will arrive
       and it will remove us as subscribers etc - until the next time we
       have a working symmetric link. Note that this only works in cases
       where the tuple content is sent using hand-shakes. (Otherwise, he
       might receive this message, followed by a new subscription
       message, send the data to us and belive that we have received it
       although we haven't). */
    message.id = htonl(peiskernel.id);
    message.why = htonl(why);
    //peisk_broadcastSpecial(PEISK_PORT_DEAD_HOST,sizeof(message),&message,PEISK_BPACKAGE_HIPRI);
    if(peisk_debugRoutes)
      printf("Sending a deadhost notification to %d\n",id);
    if(why == PEISK_DEAD_ROUTE)
      peisk_sendMessage(PEISK_PORT_DEAD_HOST,id,sizeof(message),&message,PEISK_PACKAGE_HIPRI);
    else
      peisk_sendMessage(PEISK_PORT_DEAD_HOST,id,sizeof(message),&message,PEISK_PACKAGE_HIPRI | PEISK_PACKAGE_RELIABLE);
  }

  /* Remove host from main routing table */   
  if(peisk_hashTable_getValue(peiskernel.routingTable,(void*)(long)id,(void**)(void*)&routingInfo) == 0) {
    PEISK_ASSERT(routingInfo->id == id,("invalid id %d in routing table for %d\n",routingInfo->id,id));
    peisk_hashTable_remove(peiskernel.routingTable,(void*)(long)id);
    free(routingInfo);
  } else {
    if (peisk_printLevel & PEISK_PRINT_WARNINGS)
      printf("peisk: p2p.c: warning: deleting host %d which has no entry in the main hash table\n",id);
  }
  /* Remove host from all connection routing tables */   
  for(i=0;i<PEISK_MAX_CONNECTIONS;i++)
    if(peiskernel.connections[i].id != -1) {
      if(peisk_hashTable_getValue(peiskernel.connections[i].routingTable,
				  (void*)(long)id,(void**)(void*)&routingInfo) == 0) {
	peisk_hashTable_remove(peiskernel.connections[i].routingTable,(void*)(intA)id);
	free(routingInfo);
      }
    }

  /*
  for(i=0;i<PEISK_ROUTING_SIZE;i++)
    if(peiskernel.routingTable[i].id == id) {
      peiskernel.routingTable[i].id=-1;
      *(peiskernel.routingTable[i].hashPrev) = peiskernel.routingTable[i].hashNext;
      }*/


  /* Close any direct connection to this host (they're probably stale anyway...) */
  if(why != PEISK_DEAD_REBORN) 
    for(i=0;i<PEISK_MAX_CONNECTIONS;i++) {
      if(peiskernel.connections[i].id != -1 &&
	 peiskernel.connections[i].neighbour.id == id)  {
	/*printf("DeleteHost: Closinging connection %d to %d\n",peiskernel.connections[i].id,id);*/
	peisk_closeConnection(peiskernel.connections[i].id);
      }
    }

  if(peiskernel.deadhostHook != NULL) {
    peiskernel.deadhostHook(id,peiskernel.deadhostHook_arg);
  }
}

void peisk_registerDeadhostHook(PeisDeadhostHook *fn,void *arg) {
  if(peiskernel.deadhostHook) {
    fprintf(stderr,"Error, attempting to register a second deadhost hook while we already have one.\n");
  }
  peiskernel.deadhostHook = fn;
  peiskernel.deadhostHook_arg = arg;
}


void peisk_sendAcknowledgement(int destination,int ackID) {
  peiskernel.acknowledgementPackages[peiskernel.nAcknowledgementPackages].destination = destination;
  peiskernel.acknowledgementPackages[peiskernel.nAcknowledgementPackages].ackID = ackID;
  peiskernel.acknowledgementPackages[peiskernel.nAcknowledgementPackages].priority = 1;
  if(++peiskernel.nAcknowledgementPackages >= PEISK_MAX_ACK_PACKAGES)
    peisk_sendAcknowledgementsNow();
}

void peisk_sendBulkAcknowledgement(int destination,int ackID) {
  peiskernel.acknowledgementPackages[peiskernel.nAcknowledgementPackages].destination = destination;
  peiskernel.acknowledgementPackages[peiskernel.nAcknowledgementPackages].ackID = ackID;
  peiskernel.acknowledgementPackages[peiskernel.nAcknowledgementPackages].priority = 0;
  if(++peiskernel.nAcknowledgementPackages >= PEISK_MAX_ACK_PACKAGES)
    peisk_sendAcknowledgementsNow();
}

void peisk_periodic_connectCluster(void *data) {
  int start,i;
  int numHosts;
  intA destination;
  PeisHashTableIterator iterator;
  PeisHostInfo *hostInfo;

  /* TODO - this function can be made more efficient by keeping a separate list of
     clusters around. Possibly, trigger this in the acceptBroadcast function instead... */

  /* Check if there are known nodes on a separate network cluster, if so we should
     (attempt) to connect to them to merge the two networks.
     Potential problems: this may lead to too many components connecting simultaneously to
     the new hosts.
     Possible Solutions: Do it slower (randomly), send new multicast messages
     (updating hostinfo) as soon as we have received a connection.
  */
  if(peiskernel.isLeaf) 
    /* Leaf nodes do not attempt any outgoing connections */
    return;

  numHosts = peisk_hashTable_count(peiskernel.hostInfoHT);

  /* Open iterator and fast forward a random part into it */
  peisk_hashTableIterator_first(peiskernel.hostInfoHT,&iterator);
  peisk_hashTableIterator_next(&iterator);
  for(start=rand()%numHosts;start;start--) peisk_hashTableIterator_next(&iterator);

  /* Iterate until we find a suitable component to connect to, 
     wrap around if neccessary */
  for(;numHosts;numHosts--) {
    peisk_hashTableIterator_value_generic(&iterator,&destination,&hostInfo);
    PeisConnectionMgrInfo *connMgrInfo = peisk_lookupConnectionMgrInfo(destination);
    if(hostInfo->networkCluster != peiskernel.hostInfo.networkCluster &&
       (!connMgrInfo || connMgrInfo->nextRetry <= peisk_timeNow) &&
       peisk_hostIsConnectable(hostInfo)) {
      /* Test if we have any connection already going to this host, if so we probably just haven't
	 updated the clusters yet. */
      for(i=0;i<=peiskernel.highestConnection;i++)
	if(peiskernel.connections[i].id != -1 && 
	   peiskernel.connections[i].neighbour.id == hostInfo->id)
	  break;
      if(i != peiskernel.highestConnection+1) continue;
      if(connMgrInfo && connMgrInfo->directlyConnected) {
	printf("Error, %d marked as connected (conninfo: %p), but we have no connection structure...\n",(int)destination,(void*) connMgrInfo);
	printf("%d connections known\n",peiskernel.highestConnection+1);
	for(i=0;i<=peiskernel.highestConnection;i++)
	  printf("%d: conn %d neighbour %d\n",i,peiskernel.connections[i].id,peiskernel.connections[i].neighbour.id);
	continue;
      }

      if(peisk_isRoutable(hostInfo->id)) {
	/* Host is routable, so he should be on our network anyway... */
	continue;
      } else {
	if(peisk_printLevel & PEISK_PRINT_CONNECTIONS) {
	  printf("Attempting to connect to PEIS #%d with cluster %d. our cluster is %d\n",
		 hostInfo->id,hostInfo->networkCluster,
		 peiskernel.hostInfo.networkCluster);
	}
	peisk_connectToGivenHostInfo(hostInfo,PEISK_CONNECT_FLAG_FORCED_CL);
	/* We do atmost one of these connection attempts per cycle to avoid
	   creating too many new connections */
	return;
      }
    }
    if(!peisk_hashTableIterator_next(&iterator)) {
      /* Wrap around if we have reached the end of the list */
      peisk_hashTableIterator_first(peiskernel.hostInfoHT,&iterator);
      peisk_hashTableIterator_next(&iterator);
    }
  }
}

void peisk_periodic_connectionManager(void *data) {
  int i, j, id;
  int nConnections;
  PeisHostInfo *hostInfo, *bestHostInfo;
  PeisConnectionMgrInfo *connMgrInfo;
  PeisHashTableIterator iterator;
  PeisRoutingInfo *routingInfo, *routingInfo2;
  int bestAlternativeMetric;
  PeisConnection *worstConnection, *connection;
  int worstConnectionValue;
  int bestHostValue;
  int value;
  int doConnect;

  /* Delete connections that seem dead */
  for(i=0;i<PEISK_MAX_CONNECTIONS;i++)
    if(peiskernel.connections[i].id != -1 && 
       peisk_timeNow - peiskernel.connections[i].timestamp > PEISK_CONNECTION_TIMEOUT) {
      printf("Closing connection #%d to %d because of timeout (%.1fs passed)\n",
	     peiskernel.connections[i].id,peiskernel.connections[i].neighbour.id,
	     peisk_timeNow - peiskernel.connections[i].timestamp);
      peisk_closeConnection(peiskernel.connections[i].id);
    }

  /*if(!peisk_useConnectionManager) return;*/

  /* Remove redundant connections */
  for(i=0;i<PEISK_MAX_CONNECTIONS;i++)
    if(peiskernel.connections[i].id != -1)
      for(j=i+1;j<PEISK_MAX_CONNECTIONS;j++)
	if(peiskernel.connections[j].id != -1 &&
	   peiskernel.connections[i].neighbour.id != -1 &&
	   peiskernel.connections[i].neighbour.id == peiskernel.connections[j].neighbour.id) {
	  /* 30% chance only to actually close connection on each
	     period. Helps situations where both hosts
	     connect/disconnect each others incomming connections */
	  if(rand()%100 < 70) continue;

	  PeisConnection *closeCon = NULL;
	  if(peiskernel.connections[i].isPending) closeCon = &peiskernel.connections[i];
	  else if(peiskernel.connections[j].isPending) closeCon = &peiskernel.connections[j];
	  else if(peiskernel.connections[i].totalIncomming < peiskernel.connections[j].totalIncomming) 
	    closeCon = &peiskernel.connections[i];
	  else closeCon = &peiskernel.connections[j];

	  if(1 || peisk_printLevel & PEISK_PRINT_CONNECTIONS)
	    fprintf(stdout,"peisk: connManager - closing redundant connection to %d\n",closeCon->neighbour.id);
	  peisk_closeConnection(closeCon->id);
	}
  
  
  /* Set the value of each connection to zero, unless we are routing
     heavily along this connection. */
  for(i=0;i<PEISK_MAX_CONNECTIONS;i++)
    if(peiskernel.connections[i].id != -1) {
      peiskernel.connections[i].value = 0;  
      if(peisk_timeNow - peiskernel.connections[i].createdTime < 10.0) peiskernel.connections[i].value=10000;
      if(peiskernel.connections[i].neighbour.id == -1) peiskernel.connections[i].value=10000;
      peiskernel.connections[i].usefullTraffic = 
	(int) (peiskernel.connections[i].usefullTraffic * 0.9 +
	       peiskernel.connections[i].lastUsefullTraffic * 0.1);
      peiskernel.connections[i].lastUsefullTraffic = 0;
      if(peiskernel.connections[i].usefullTraffic > PEISK_FORCE_LINK_BPS)
	peiskernel.connections[i].value = 10000;
    }


  /* Count how many connections we currently have */
  nConnections=0;
  for(i=0;i<=peiskernel.highestConnection;i++)
    if(peiskernel.connections[i].id != -1) {
      nConnections++;
    }

  /* Iterate over each known host, establish "forced" connections if neccessary
     and compute it's value. Also, update the value for the connection
     which routes to this host. If any forced connections are
     successfully created then exit from the connectionManager (we do
     not want to establish more than one new connection per periodic call).
  */
  bestHostInfo=NULL;
  bestHostValue=-10000;
  doConnect=1;

  /*printf("Connection manager, %d hosts known\n",peisk_hashTable_count(peiskernel.hostInfoHT));*/

  for(peisk_hashTableIterator_first(peiskernel.hostInfoHT,&iterator);peisk_hashTableIterator_next(&iterator);) {
    peisk_hashTableIterator_value_fast(&iterator,&id,&hostInfo);
    connMgrInfo = peisk_lookupConnectionMgrInfo(hostInfo->id);
    PEISK_ASSERT(connMgrInfo,("No connMgrInfo for host %d\n",id));
    connMgrInfo->usefullTraffic = 
      (int) (connMgrInfo->usefullTraffic * 0.9 + 
	     connMgrInfo->lastUsefullTraffic * 0.1);
    connMgrInfo->lastUsefullTraffic = 0;

    if(id == peiskernel.id) continue;
    if(!peisk_hostIsConnectable(hostInfo))
      continue;

    /*printf("Usefull traffic to %d is %d\n",id,connMgrInfo->usefullTraffic);*/

    if(peisk_hashTable_getValue(peiskernel.routingTable,(void*)(long)id,(void**)(void*) &routingInfo) != 0) {
      /* Unroutable hosts will be handled by the connect clusters function instead */
      continue;
    }

    /* See if we already have a connection towards this host. If so,
       we do not need any more considerations for this host.*/
    for(i=0;i<=peiskernel.highestConnection;i++)
      if(peiskernel.connections[i].id != -1 && 
	 peiskernel.connections[i].neighbour.id == hostInfo->id)
	break;
    if(i != peiskernel.highestConnection) continue;

    connection = routingInfo->connection;
    /* See if the routing towards this host improves the current
       value of the connection going towards this host. */
    if(connection && routingInfo->hops*100+100 > connection->value) {
      for(bestAlternativeMetric=1000,i=0;i<PEISK_MAX_CONNECTIONS;i++)
	if(peiskernel.connections[i].id != -1 && &peiskernel.connections[i] != connection &&
	   peisk_hashTable_getValue(peiskernel.connections[i].routingTable,(void*)(long)id,(void**)(void*) &routingInfo2) == 0 &&
	   routingInfo2->hops < bestAlternativeMetric) {
	  bestAlternativeMetric = routingInfo2->hops;
	  if(routingInfo->hops - bestAlternativeMetric <= connection->value) break;
	  }
      /*printf("Routing to %d have current cost %d and alternative cost %d not conn to %d\n",
	id,routingInfo->hops,bestAlternativeMetric,connection->neighbour.id);*/
      value = (bestAlternativeMetric - routingInfo->hops)*100;
      if(value > connection->value-100) {
	value += (rand()/19)%100;
	if(value > connection->value)
	  connection->value = value;
      }
    }
    
    /* Now, figure out if we should do nothing, force a connection to him or consider a connection to 
       him just to reduce the diameter of the P2P network. */
    if(connMgrInfo->directlyConnected) { value = 0; }
    else if(connMgrInfo->usefullTraffic > PEISK_FORCE_LINK_BPS+PEISK_FORCE_LINK_BPS/4) {
      value = 0; 
      /* TODO - do we need any randomness in this??? */
      if(connMgrInfo->nextRetry <= peisk_timeNow && doConnect) {
	if(peisk_printLevel & PEISK_PRINT_CONNECTIONS) {
	  printf("Forcing connection to %d (%d Bps)\n",hostInfo->id,connMgrInfo->usefullTraffic);
	  printf("%d ntries: %d nextRetry: %f\n",hostInfo->id,
		 connMgrInfo->nTries,connMgrInfo->nextRetry-peisk_timeNow);
	}
	connection = peisk_connectToGivenHostInfo(hostInfo,PEISK_CONNECT_FLAG_FORCED_BW);
	if(connection) {
	  doConnect=0;
	  connection->usefullTraffic = connMgrInfo->usefullTraffic;
	  connection->value = 10000;
	}
      }
    }
    else if(connMgrInfo->nextRetry > peisk_timeNow) value = 0; 
    else {
      PEISK_ASSERT(routingInfo,("this should not happen\n"));
      if(routingInfo) {
	value = (routingInfo->hops - peisk_estimateHostMetric(hostInfo))*100;

	/* Avoid hosts that already have many connections */
	if(connMgrInfo->nConnections > 3  && connMgrInfo->nConnections<255)
	  value -= (connMgrInfo->nConnections-3) * 100;

	if(value > bestHostValue-100) {
	  value += (rand()/17) % 100;
	  if(value > bestHostValue) {
	    bestHostValue = value;
	    bestHostInfo=hostInfo;
	  }
	}
      }
    }
  } 

  /* Find the connection with the worst value, also count number of
     valid connections we have*/
  worstConnection=NULL;
  worstConnectionValue=11000;
  nConnections=0;
  for(i=0;i<PEISK_MAX_CONNECTIONS;i++)
    if(peiskernel.connections[i].id != -1) {
      nConnections++;
      if(peiskernel.connections[i].value < worstConnectionValue) {
	worstConnectionValue = peiskernel.connections[i].value;
	worstConnection = &peiskernel.connections[i];
      }
    }

  /*
  printf("bestHostValue: %d worstConnectionValue: %d\n",bestHostValue,worstConnectionValue);
  if(worstConnection)
    printf("Worst connection going to %d\n",worstConnection->neighbour.id);
  */

  if(nConnections <= PEISK_MIN_AUTO_CONNECTIONS) {
    if(bestHostInfo && doConnect) {
      if(peisk_printLevel & PEISK_PRINT_CONNECTIONS) {
	printf("Connecting to %d since we have to few connectons (value: %d)\n",bestHostInfo->id,bestHostValue);
	connMgrInfo = peisk_lookupConnectionMgrInfo(bestHostInfo->id);
	if(!connMgrInfo) printf("No connMgrInfo for %d\n",bestHostInfo->id);
	else printf("%d ntries: %d nextRetry: %f\n",bestHostInfo->id,
		    connMgrInfo->nTries,connMgrInfo->nextRetry-peisk_timeNow);
      }
      peisk_connectToGivenHostInfo(bestHostInfo,0);
    } else {
    }
    return;
  } else if(nConnections <= PEISK_MAX_AUTO_CONNECTIONS - 2) {
    if(bestHostInfo && bestHostValue > worstConnectionValue && doConnect) {
      if(peisk_printLevel & PEISK_PRINT_CONNECTIONS) {
	printf("Connecting to %d since he's better than conn to %d\n",bestHostInfo->id,worstConnection->neighbour.id);
	connMgrInfo = peisk_lookupConnectionMgrInfo(bestHostInfo->id);
	if(!connMgrInfo) printf("No connMgrInfo for %d\n",bestHostInfo->id);
	else printf("%d ntries: %d nextRetry: %f\n",bestHostInfo->id,
		    connMgrInfo->nTries,connMgrInfo->nextRetry-peisk_timeNow);
      }

      if(nConnections+1 > PEISK_MAX_AUTO_CONNECTIONS && worstConnection) {
	/* Close one connection since we will have too many */
	if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
	  printf("Closing connection to %d, value %d. prepare for too many connections. (%d Bps traffic)\n",worstConnection->neighbour.id,worstConnectionValue,worstConnection->usefullTraffic);	
	peisk_closeConnection(worstConnection->id);
      }
      peisk_connectToGivenHostInfo(bestHostInfo,0);
    }
    return;
  } else if(nConnections > PEISK_MAX_AUTO_CONNECTIONS) {
    /* We have too many connections, close the worst one */
    if(worstConnection) {
      if(worstConnectionValue < 10000) {
	if(peisk_printLevel & PEISK_PRINT_CONNECTIONS) {
	  connMgrInfo = peisk_lookupConnectionMgrInfo(worstConnection->neighbour.id);
	  printf("Closing connection to %d, value %d. too many connections. (conn %d Bps traffic, host %d traffic)\n",
		 worstConnection->neighbour.id,worstConnectionValue,worstConnection->usefullTraffic,connMgrInfo?connMgrInfo->usefullTraffic:0);	
	}
	peisk_closeConnection(worstConnection->id);
      }
      else if(nConnections >= PEISK_MAX_FORCED_AUTO_CONNECTIONS) {
	if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
	  printf("Closing forced connection to %d, value %d. way too many connections. (%d Bps traffic)\n",worstConnection->neighbour.id,worstConnectionValue,worstConnection->usefullTraffic);	
	peisk_closeConnection(worstConnection->id);
      } else {
	if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
	  printf("All connections forced, not closing anything\n");
      }
    }
    return;
  }
}
/** True if given host is the same as our host, currently very naive implementation based on hostname. */
int peisk_hostIsMe(PeisHostInfo *hostInfo) {
  return strcmp(hostInfo->hostname,peiskernel.hostInfo.hostname) == 0 ? 1 : 0;
}

int peisk_hostIsConnectable(PeisHostInfo *hostInfo) {
  int i;
  PeisConnectionMgrInfo *connMgrInfo;

  connMgrInfo = peisk_lookupConnectionMgrInfo(hostInfo->id);
  if(!connMgrInfo) return 0;
  if(connMgrInfo->nextRetry > peisk_timeNow) return 0;

  if(peisk_hostIsMe(hostInfo)) return 1;
  for(i=0;i<hostInfo->nLowlevelAddresses;i++)
    if(!hostInfo->lowAddr[i].isLoopback &&
       peisk_linkIsConnectable(&hostInfo->lowAddr[i])) return 1;

  return 0;

}

PeisConnection *peisk_connectToGivenHostInfo(PeisHostInfo *hostInfo,int flags) {
  int r,j;
  PeisConnectionMgrInfo *connMgrInfo;

  /*printf("Connect to hostinfo: %d (connectable: %d)\n",hostInfo->id,peisk_hostIsConnectable(hostInfo));*/

  connMgrInfo = peisk_lookupConnectionMgrInfo(hostInfo->id);
  if(!connMgrInfo) return NULL;
  if(hostInfo->nLowlevelAddresses == 0) return NULL;
  if(peisk_hostIsMe(hostInfo)) {
    /* Attempting to connect to a PEIS on the same host, make sure to
       use a loopback device if one is available */
    /** \todo Pick a suitable loopback device instead of the first
       one. This would be needed if we have many loopback devices and
       not all beeing accessible by all components on the same CPU. */
    for(j=0;j<hostInfo->nLowlevelAddresses;j++)
      if(hostInfo->lowAddr[j].isLoopback) break;
    if(j != hostInfo->nLowlevelAddresses) {
      return peisk_connectToGivenLowaddr(hostInfo,connMgrInfo,&hostInfo->lowAddr[j],flags);
    }
  }
  /* Otherwise, cannot use a loopback device for connecting */

  /* If we are a leaf node then give up connection here since leafs
     should go through parent node for all host to host
     connections. */
  if(peiskernel.isLeaf) return NULL;

  /* Select which of the network addresses to connect to randomly (if
     it has multiple addresses) */
  r = rand() % hostInfo->nLowlevelAddresses;
  j=r;
  /* If we accidentally picked a loopback interface or an interface
     that is not connectable, then pick another device or return if
     there is no other such device. */
  while(hostInfo->lowAddr[j].isLoopback || !peisk_linkIsConnectable(&hostInfo->lowAddr[j])) {
    j = (j+1) % hostInfo->nLowlevelAddresses;
    if(j == r)
      /* Error, no address to connect to was found */
      return NULL;
  }
  return peisk_connectToGivenLowaddr(hostInfo,connMgrInfo,&hostInfo->lowAddr[j],flags);
}

PeisConnection *peisk_connectToGivenLowaddr(PeisHostInfo *hostinfo,
					    PeisConnectionMgrInfo *connMgrInfo,
					    PeisLowlevelAddress *laddr,
					    int flags) {
  char url[256];
  PeisConnection *connection;

  switch(laddr->type) {
  case ePeisTcpIPv4:
    sprintf(url,"tcp://%d.%d.%d.%d:%d",
	    laddr->addr.tcpIPv4.ip[0],
	    laddr->addr.tcpIPv4.ip[1],
	    laddr->addr.tcpIPv4.ip[2],
	    laddr->addr.tcpIPv4.ip[3],
	    (int) laddr->addr.tcpIPv4.port);
    break;
  case ePeisUdpIPv4:
    sprintf(url,"udp://%d.%d.%d.%d:%d",
	    laddr->addr.udpIPv4.ip[0],
	    laddr->addr.udpIPv4.ip[1],
	    laddr->addr.udpIPv4.ip[2],
	    laddr->addr.udpIPv4.ip[3],
	    (int) laddr->addr.udpIPv4.port);
    break;
  case ePeisBluetooth:
    sprintf(url,"bt://%x:%x:%x:%x:%x:%x;%d",
	    laddr->addr.bluetooth.baddr[0],
	    laddr->addr.bluetooth.baddr[1],
	    laddr->addr.bluetooth.baddr[2],
	    laddr->addr.bluetooth.baddr[3],
	    laddr->addr.bluetooth.baddr[4],
	    laddr->addr.bluetooth.baddr[5],
	    (int) laddr->addr.bluetooth.port);
    break;
  default:
    return NULL;
  }

  /*if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
    fprintf(stdout,"peisk: connManager - attempting to connect to %s\n",url);*/
  connection = peisk_connect(url,flags);
  if(connection) {
    /* The connection was successfully started.
       Initialize it's knownHost field to reasonable values. */

    /*if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
      fprintf(stdout,"peisk: connManager - connection to %s SUCCESS\n",url);*/

    connection->neighbour = *hostinfo;
    PeisConnectionMgrInfo *connMgrInfo=peisk_lookupConnectionMgrInfo(hostinfo->id);
    if(!connMgrInfo) {
      connMgrInfo = (PeisConnectionMgrInfo*) malloc(sizeof(PeisConnectionMgrInfo));
      peisk_insertConnectionMgrInfo(hostinfo->id,connMgrInfo);
      peisk_initConnectionMgrInfo(connMgrInfo);
      connMgrInfo->nTries=0;
      connMgrInfo->nextRetry=peisk_timeNow;
    }

    /* Connection might have been successfull, or might have failed.
       Increment tries and next retry, if connection was successfull then
       hook_hostInfo will reset these to zero/now. */
    connMgrInfo->nTries++;
    connMgrInfo->nextRetry = peisk_timeNow + 1.0 * connMgrInfo->nTries;

    /*connMgrInfo->nTries = 0; moved to hook_hostInfo instead... */
    /*connMgrInfo->nextRetry = peisk_timeNow;*/
    /*printf("Marking %d as directly connected (%x) connid: %d\n",
      hostinfo->id,(unsigned int)(intA) connMgrInfo,connection->id);*/
    PEISK_ASSERT(connection->neighbour.id == hostinfo->id,("Invalid neighbour info in connection?"));

    /* It is a bug to mark it directly connected already now. 
       If we close the connection before receiving the hostInfo along it we may end up with inconsistent ConnectionMgrInfo here */
    /*connMgrInfo->directlyConnected=1;*/
    return connection;
  } else {

    /* Connection failed - increment nTries and nextRetry */
    connMgrInfo->nTries++;
    /* Time to wait between retries are slowly incremented with failures */
    connMgrInfo->nextRetry = peisk_timeNow + 1.0 * connMgrInfo->nTries;

    if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
      fprintf(stdout,"peisk: connManager - connection to %s FAIL\n",url);
    return NULL;
  }
}



void peisk_outgoingConnectFinished(PeisConnection *connection,int flags) {
  int previousRoutes;
  connection->isPending=0;

  if(flags & PEISK_CONNECT_FLAG_FORCE_BCAST) 
    connection->forceBroadcasts = 1;
  else
    connection->forceBroadcasts = 0;

  /* Send our host information along connection */
  peisk_sendLinkHostInfo(connection);

  /* Count how many routes we knew about previously */
  previousRoutes = peisk_hashTable_count(peiskernel.routingTable);
  if(previousRoutes <= 1) {
    /* This looks like the first successfull outgoing link we have
       made, present ourselves to everone else by broadcasting our
       hostInfo */
    peisk_broadcastHostInfo();       
  }

  /* Force an update of routing tables by directly calling the periodic routing function */ 
  /*peisk_checkClusters();??*/
  peisk_do_routing(1,connection);
}

void peisk_incommingConnectFinished(PeisConnection *connection,PeisConnectMessage *connectMessage) {
  PeisConnectionMgrInfo *connMgrInfo;
  int previousRoutes;

  connection->isPending=0;
  connection->neighbour.id = connectMessage->id;
  connMgrInfo=peisk_lookupConnectionMgrInfo(connectMessage->id);
  if(!connMgrInfo) {
    connMgrInfo = (PeisConnectionMgrInfo*) malloc(sizeof(PeisConnectionMgrInfo));
    peisk_insertConnectionMgrInfo(connectMessage->id,connMgrInfo);
    peisk_initConnectionMgrInfo(connMgrInfo);
    connMgrInfo->nTries = 0;
    connMgrInfo->nextRetry = peisk_timeNow;
  }
  /*printf("Marking %d as directly connected (%x)\n",
    connectMessage->id,(unsigned int)(intA) connMgrInfo);*/
  connMgrInfo->directlyConnected=1;
  connection->usefullTraffic = connMgrInfo->usefullTraffic;
  if(connectMessage->flags & PEISK_CONNECT_FLAG_FORCE_BCAST)
    connection->forceBroadcasts=1;
  else
    connection->forceBroadcasts=0;

  /* Send our host information along connection */
  peisk_sendLinkHostInfo(connection);
  
  /* Count how many routes we knew about previously */
  previousRoutes = peisk_hashTable_count(peiskernel.routingTable);
  if(previousRoutes <= 1) {
    /* This looks like the first successfull outgoing link we have
       made, present ourselves to everone else by broadcasting our
       hostInfo */
    peisk_broadcastHostInfo();       
  }
    
  /* Force an update of routing tables by directly calling the periodic routing function */
  peisk_do_routing(1,connection);
}


void peisk_printNetPackageInfo(PeisPackage *package) {
  PeisPackageHeader *header;
  header = &package->header;
  printf("Src: %d Dst: %d Port: %d Len: %d\n",ntohl(header->source),ntohl(header->destination),ntohs(header->port),ntohs(header->datalen));
}
