/** \file linklayer.c
    Implements all the generic routines in the linklayer. See specific
    linklayer modules (peiskernel_tcpip.c or bluetooth.c) for the link
    mechanism specific functions.  
*/
/**
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

/* This is needed for SIOCOUTQ tests below. Can be disabled for platforms which does not have it... */
/* CYGWIN does not have these...  --AS 060816 */
#ifndef __CYGWIN__
#ifndef __MACH__
#include <linux/sockios.h>
#include <asm/ioctls.h>
#include <sys/ioctl.h>
#endif
#endif

#define PEISK_PRIVATE
#include "peiskernel.h"
#include "p2p.h"
#include "peiskernel_tcpip.h"
#include "bluetooth.h"

void peisk_autoConnect(char *url) {
  PeisAutoHost *autohost;

  if(peiskernel.nAutohosts >= PEISK_MAX_AUTOHOSTS) {
    fprintf(stderr,"peisk: ERROR - Too many autohosts\n");
    exit(0);
  }
  autohost=&peiskernel.autohosts[peiskernel.nAutohosts++];
  autohost->url = strdup(url);
  autohost->isConnected = -1;
  autohost->lastAttempt = 0;
}

PeisConnection *peisk_connect(char *url,int flags) {
  char name[256];
  int port=8000;
  int i;

  if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
    printf("peisk: connecting to %s\n",url);

  if(strncmp("udp://",url,6) == 0) {
#ifdef UNUSED
    url += 6;

    /* Extract hostname and port */
    for(i=0;i<256;i++) {
      if(url[i] == 0 || url[i] == ':') { name[i]=0; break; }
      else name[i]=url[i];
    }
    if(url[i] && url[i+1]) {
      if(sscanf(&url[i+1],"%d",&port) != 1) {
	fprintf(stderr,"peisk::connect; malformed url: 'udp://%s'\n",url);
	return -1;
      }
    }
    return peisk_udpConnect(name,port,flags);
#endif    
  } else if(strncmp("tcp://",url,6) == 0) {
    url += 6;

    /* Extract hostname and port */
    for(i=0;i<256;i++) {
      if(url[i] == 0 || url[i] == ':') { name[i]=0; break; }
      else name[i]=url[i];
    }
    /** \todo Allow a * as "port" for TCP/BT connections. This should
	make the connection attempt successive ports until it find one
	that it can connect to. */
    if(url[i] && url[i+1]) {
      if(sscanf(&url[i+1],"%d",&port) != 1) {
	fprintf(stderr,"peisk::connect; malformed url: 'tcp://%s'\n",url);
	return NULL;
      }
    }
    return peisk_tcpConnect(name,port,flags);
  } else if(strncmp("bt://",url,5) == 0) {
    url += 5;
    unsigned int btAddrI[6];
    unsigned char btAddr[6];
    int btPort;

    /* Extract BT address and port */
    if(sscanf(url,"%x:%x:%x:%x:%x:%x;%d",
	      &btAddrI[0],&btAddrI[1],&btAddrI[2],&btAddrI[3],&btAddrI[4],&btAddrI[5],&btPort)
       != 7) {
      fprintf(stderr,"peisk::connect; malformed url: 'bt://%s'\n",url);
      return NULL;
    }
    for(i=0;i<6;i++) btAddr[i] = btAddrI[i];
    return peisk_bluetoothConnect(btAddr,port,flags);
  } else {
    fprintf(stderr,"peisk::connect; unknown protocoll in url: '%s'\n",url);
  }
  return NULL;
}

void peisk_recomputeConnectionMetric(PeisConnection *connection) {
  connection->metricCost = peisk_netMetricCost;
  if(connection->neighbour.id != -1) {
    /* Figure out if connection is to localhost. */
    /* For now - assume that everyone have a unique hostname... */
    /* \todo Compute connection cost based on network device which host routes through */
    if(strcmp(connection->neighbour.hostname,peiskernel.hostInfo.hostname) == 0) 
      connection->metricCost = 1;
  }
}
int peisk_estimateHostMetric(PeisHostInfo *hostInfo) {
  if(strcmp(hostInfo->hostname,peiskernel.hostInfo.hostname) == 0) return 1;
  else return peisk_netMetricCost;
}

int peisk_connection_sendAtomic(PeisConnection *connection,PeisPackage *package,int len) {
  if(connection->isPending) return -1;

  if(peiskernel.simulatePackageLoss > 0.0 && (rand()%1000) < (peiskernel.simulatePackageLoss*1000.0)) {
    /* Introduces a fake packet loss on outgoing packages, used for
       debugging robustness */
    connection->outgoingIdCnt++;
    return 0;
  }

  switch(connection->type) {
  case eSerialConnection:
    return -1;
  case eTCPConnection:
    return peisk_tcpSendAtomic(connection,package,len);
  case eUDPConnection:
    /*return peisk_udpSendAtomic(connection,package,len);*/
    return -1;
  case eBluetoothConnection:
    return peisk_bluetoothSendAtomic(connection,package,len);
  }
  return -1;
}
/** Call when a connection seem to be out of sync, flushes packages until we are back on track again. */
void peisk_syncflush(PeisConnection *connection) {
  int status;
  long int mysync;

  if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
    fprintf(stdout,"peisk: connection %d lost sync\n",connection->id);
  switch(connection->type) {
  case eTCPConnection:
	  while(1) {
	    /* For TCP connections we just attempt to read and throw away chars until we have found a sync long int
	       until we have found a new sync character. */
	    errno=0;
	    status=recv(connection->connection.tcp.socket,(void*)&mysync,sizeof(mysync),MSG_PEEK|MSG_NOSIGNAL);
	    if(errno == EAGAIN) return;
	    if(errno == EPIPE || status == -1) {          /* Error - close socket */
	      if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
		printf("peisk: warning - error (3) in receive, closing socket\n");
	      peisk_closeConnection(connection->id);
	      return;
	    }
	    if(status == sizeof(mysync) && mysync == PEISK_SYNC)
	      /* We have found the sync, continue as before */
	      { peisk_connection_processIncomming(connection); return; }
	    /* read again but without peek so that the data is realy consumed */
	    errno=0;
	    //status=recv(connection->connection.tcp.socket,(void*)&mysync,sizeof(mysync),MSG_NOSIGNAL);
	    status=recv(connection->connection.tcp.socket,(void*)&mysync,1,MSG_NOSIGNAL);
	  }
	  break;
  case eUDPConnection:
    /* UDP connections cannot become out of sync since udp preserves record boundaries */
    return;
  case eBluetoothConnection:
    /* Bluetooth connections cannot become out of sync since L2CAP preserves record boundaries */
    return;
  default: peisk_closeConnection(connection->id);
  }
}


/** \todo remove the pending connection stuff from
    peisk_connection_receiveIncomming and put into periodic functions
    instead. */
int peisk_connection_receiveIncomming(PeisConnection *connection,PeisPackage *package) {
  if(connection->isPending) return 0;

#ifdef OLD
  /* Handle pending UDP connection specially, just check for the ACK message so it can be promoted
     to a eUDPConnected status */
  if(connection->type == eUDPConnection && connection->connection.udp.status == eUDPPending) {
    /* Check for a response... */
    /* Update our sin_port according to response */
    errno=0;
    len=sizeof(addr);
    size=recvfrom(connection->connection.udp.socket,buf,sizeof(buf),MSG_DONTWAIT,&addr,&len);
    if(size <= 0) return 0;

    /* TODO: Test that we realy got the package "ack" back */

    in_addr=(struct sockaddr_in*) &connection->connection.udp.addr; 
    memcpy(&connection->connection.udp.addr,&addr,len);
    connection->connection.udp.len = len;
    printf("UDP connection acknowledged. Socket: %d Len: %d Port: %d\n",connection->connection.udp.socket,len,ntohs(in_addr->sin_port));
    printf("New sin_addr: %d %d %d %d\n",((unsigned char*)&in_addr->sin_addr)[0],((unsigned char*)&in_addr->sin_addr)[1],((unsigned char*)&in_addr->sin_addr)[2],((unsigned char*)&in_addr->sin_addr)[3]);

    /* Force an update of routing tables by directly calling the periodic routing function */
    /* peisk_do_routing(1); */
    if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
      fprintf(stdout,"peisk: new outbound udp/ip connection #%d established\n",connection->id);

    connection->connection.udp.status=eUDPConnected;
    return 0;
  }
#endif

  int success=0;
  switch(connection->type) {
  case eTCPConnection:
    success = peisk_tcpReceiveIncomming(connection,package);
    break;
  case eUDPConnection:
    /*success = peisk_udpReceiveIncomming(connection,package);*/
    break;
  case eBluetoothConnection:
    success = peisk_bluetoothReceiveIncomming(connection,package);
    break;
  default: return 0; /* TODO - handle other connection types here */
  }

  if(!success) return 0;

  /* Update congestion control information */
  if(ntohl(package->header.linkCnt)+1 > connection->incomingIdHi)
    connection->incomingIdHi = ntohl(package->header.linkCnt)+1;
  connection->incomingIdSuccess++;

  connection->totalIncomming += sizeof(package->header) + ntohs(package->header.datalen);
  peiskernel.incomingTraffic+= sizeof(package->header) + ntohs(package->header.datalen);

  /* Update timestamp on connection so it is kept alive */
  connection->timestamp = peisk_timeNow;

  /* Print debug info if requested */
  if(peisk_debugPackages) /* || ntohs(package->header.port) == PEISK_PORT_SET_REMOTE_TUPLE)*/
    fprintf(stdout,"peisk: IN conn=%d id=%x port=%d hops=%d len=%d type=%d src=%d dest=%d\n",
	    connection->id,ntohl(package->header.id),ntohs(package->header.port),package->header.hops,
	    ntohs(package->header.datalen),package->header.type,
	    ntohl(package->header.source),
	    ntohl(package->header.destination));
  connection->incommingTraffic += sizeof(package->header) + ntohs(package->header.datalen);
  peisk_in_packages[ntohs(package->header.port)]++;

  /* Verify that the sync character where correct, otherwise close connection. */
  if(package->header.sync != PEISK_SYNC) {
    printf("peisk: warning - bad sync. Closing connection %d\n",connection->id);
    peisk_closeConnection(connection->id);
    return 0;
  }

  /* Make sure ID is a valid network byte representation of a natural integer (>= 0) */
  if(ntohl(package->header.id) < 0  || ntohl(package->header.ackID) < 0) {
    fprintf(stdout,"peisk: bad package on connection %d, id=%d\n",connection->id,ntohl(package->header.id));
    peisk_closeConnection(connection->id);
    return 0;
  }

  return 1;
}


int peisk_recvBlocking(int fd,void *buf,size_t len,int flags,double timeout) {
  int i, new_bytes;
  char *data=(char*)buf;
  double t0 = peisk_gettimef();

  while(len>0) {
    errno=0;
    new_bytes=recv(fd,data,len,flags|MSG_NOSIGNAL);
    if(errno == EPIPE || new_bytes == 0) {
      /* Broken pipe - close socket and return */
      for(i=0;i<PEISK_MAX_CONNECTIONS;i++) {
	if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
	  printf("Connection %d socket=%d\n",i,peiskernel.connections[i].connection.tcp.socket);
	if(peiskernel.connections[i].id != -1 && peiskernel.connections[i].connection.tcp.socket == fd)
	  peisk_closeConnection(peiskernel.connections[i].id);
      }
      /*printf("<\n"); fflush(stdout); */
      return 0;
    }
    if(new_bytes == -1) peisk_waitForRead(fd,0.01); /* was: usleep 0 */
    else {
      if(new_bytes == 0) if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)  printf("peisk: error - not getting any bytes, broken connection?\n");
      len -= new_bytes;
      data+=new_bytes;
    }
    if(peisk_gettimef() > t0 + timeout) { /*printf("< (TIME)\n"); fflush(stdout);*/ return 0; } /* Timeout reached */
  }
  return 1;
}

int peisk_recvAtomic(int fd,void *buf,size_t len,int flags) {
  int i, new_bytes;
  char *data=(char*)buf;
  errno=0;
  new_bytes=recv(fd,data,len,flags|MSG_NOSIGNAL);

  /*printf("ATOMIC>"); fflush(stdout);*/
  if(new_bytes <= 0) { /*printf("<\n"); fflush(stdout);*/ return 0; }/* Nothing received */
  /*printf("new_bytes=%d\n",new_bytes);*/
  len -= new_bytes;
  data+=new_bytes;

  while(len>0) {
    errno=0;
    new_bytes=recv(fd,data,len,flags|MSG_NOSIGNAL);
	printf("new_bytes=%d\n",new_bytes);
    if(errno == EPIPE || new_bytes == 0) {
      /* Broken pipe - close socket and return */
      for(i=0;i<PEISK_MAX_CONNECTIONS;i++) {
	printf("Connection %d socket=%d\n",i,peiskernel.connections[i].connection.tcp.socket);
	if(peiskernel.connections[i].id != -1 && peiskernel.connections[i].connection.tcp.socket == fd)
	  peisk_closeConnection(peiskernel.connections[i].id);
      }
      /*printf("<\n"); fflush(stdout);*/
      return 0;
    }
    if(new_bytes == -1) peisk_waitForRead(fd,0.01); /* was: usleep(100) */
    else {
      len -= new_bytes;
      data+=new_bytes;
    }
  }
  /*printf("<\n"); fflush(stdout);*/
  return 1;
}

void peisk_setSelectReadSignals(int *n,fd_set *readSet,fd_set *writeSet,fd_set *excpSet) {
  int i,j;

  *n=1;

  if(peiskernel.tcp_isListening) {
    *n=MAX(*n,peiskernel.tcp_serverSocket);
    FD_SET(peiskernel.tcp_serverSocket,readSet);
  }
  if(peiskernel.udp_serverPort != 0) {
    *n=MAX(*n,peiskernel.udp_serverSocket);
    FD_SET(peiskernel.udp_serverSocket,readSet);
  }
  *n=MAX(*n,peiskernel.tcp_broadcast_receiver);
  if(peiskernel.tcp_broadcast_receiver >= 0)
    FD_SET(peiskernel.tcp_broadcast_receiver,readSet);

  for(i=0;i<=peiskernel.highestConnection;i++)
    if(peiskernel.connections[i].id != -1) {
      PeisConnection *connection=&peiskernel.connections[i];
      if(connection->type == eTCPConnection) {
	*n=MAX(*n,connection->connection.tcp.socket);
	FD_SET(connection->connection.tcp.socket,readSet);
	/*FD_SET(peiskernel.connections[i].connection.tcp.socket,excpSet);*/ /*Is this neccessary????? */
	for(j=0;j<PEISK_NQUEUES;j++) if(connection->nQueuedPackages[j] > 0) break;
	if(j != PEISK_NQUEUES) FD_SET(connection->connection.tcp.socket,writeSet);
      }
    }
}

int peisk_linkIsConnectable(PeisLowlevelAddress *address) {
  switch(address->type) {
  case ePeisTcpIPv4:    
    /* If you want to activate debugging use this... to test bluetooth connections instead. Will only affect non loopback devices... */
    /*return 0;*/
    return peisk_ipIsConnectable(address->addr.tcpIPv4.ip,address->addr.tcpIPv4.port);
  case ePeisUdpIPv4:
    /* We have currently disabled UDP */
    return 0;
    /*return peis_ipIsConnectable(address->addr.tcpIPv4.ip,address->addr.tcpIPv4.port);*/
  case ePeisTcpIPv6:
    /* Not fully implemented */
    return 0;
  case ePeisBluetooth:
    return peisk_bluetoothIsConnectable(address->addr.bluetooth.baddr,address->addr.bluetooth.port);
  }
  return 0;
}

void peisk_initConnectMessage(PeisConnectMessage *message,int flags) {
  message->version = htonl(peisk_protocollVersion);  
  strncpy(message->networkString,peisk_networkString,sizeof(message->networkString));
  message->flags = htonl(flags);
  message->id = htonl(peiskernel.id);
}

int peisk_verifyConnectMessage(PeisConnection *connection,PeisConnectMessage *message) {
  static int hasWarned=0;
  int index, nConnections;

  message->version = ntohl(message->version);
  message->flags = ntohl(message->flags);
  message->id = ntohl(message->id);

  if(message->id == peiskernel.id) 
    /* We do not trust anyone that claims to be us... */
    return -1;

  if(message->version > peisk_protocollVersion && !hasWarned) {
    hasWarned=1;
    fprintf(stderr,"peisk::WARNING - A newer version of the PEIS kernel might be available\n");
    fprintf(stderr,"Consider updating your peiskernel or running on a private network!\n");
  }
  if(message->version != peisk_protocollVersion) {
    fprintf(stderr,"incomming connection with incorrect protocoll version %d\n",message->version);
    return -1;
  }
  
  if(strncmp(message->networkString,peisk_networkString,sizeof(message->networkString)) != 0){
    printf("network '%s' is wrong\n",message->networkString);
    return -1;
  }

  /* Count how many connections we currently have open */
  nConnections=0;
  for(index=0;index<PEISK_MAX_CONNECTIONS;index++)
    if(peiskernel.connections[index].id != -1) nConnections++;

  if(nConnections+1 > PEISK_MAX_AUTO_CONNECTIONS && 
     !(message->flags & (PEISK_CONNECT_FLAG_FORCED_BW|PEISK_CONNECT_FLAG_FORCED_CL))) {
    /* We have too many connections... and he's not desperate. Just close this one */
    if(1 || peisk_printLevel & PEISK_PRINT_CONNECTIONS)
      printf("Refusing incomming connection from %d (flags: %d)\n",message->id,message->flags);
    return -1;
  }

  if(nConnections+1 > PEISK_MAX_FORCED_AUTO_CONNECTIONS &&
     !(message->flags & PEISK_CONNECT_FLAG_FORCED_CL)) {
    /* He want to connect us to optimize bandwidth. See if the 
       worst connection we have is better than this guy. */
    /* Find the connection with the worst value, also count number of
       valid connections we have*/
    int i,value,worst;
    worst=-1;
    for(i=0,value=10000;i<=peiskernel.highestConnection;i++) {
      if(peiskernel.connections[i].id != -1 && 
	 peiskernel.connections[i].value < value) { value=peiskernel.connections[i].value; worst=i; }
    }
    
    if(worst == -1) {
      /* No non-forced connection found, reject this incoming one */
      if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
	printf("Refusing incomming connection from %d, no freeable connection found\n",message->id);
      return -1;
    } else {
      peisk_closeConnection(peiskernel.connections[worst].id);
      if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
	printf("Closing connection #%d to %d in favour for %d\n",
	       peiskernel.connections[worst].id,
	       peiskernel.connections[worst].neighbour.id,
	       message->id);
    }
  }

  return 0;
}
