/** \file udp.c
   Implements a UDP/IP linklayer interface
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
#include <getopt.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include <netinet/in.h>
#include <net/if.h>

#include <arpa/inet.h>


#define PEISK_PRIVATE
#include "peiskernel.h"
#include "p2p.h"
#include "linklayer.h"
#include "peiskernel_tcpip.h"


int peisk_udpConnect(char *name,int port,int flags) {
  PeisConnection *connection;
  struct hostent *hostent;
  struct sockaddr_in addr;
  socklen_t len;
  int sock,index;
  char buf[256],str[256];
  int ret;

  printf("Creating UDP connection to %s at port %d\n",name,port);

  /* Find a free connection to use */
  for(index=0;index<PEISK_MAX_CONNECTIONS;index++)
    if(peiskernel.connections[index].id == -1) break;
  if(index == PEISK_MAX_CONNECTIONS) {
    fprintf(stdout,"peisk: too many connections - refusing manual connection\n");
    return -1;
  }
  connection=&peiskernel.connections[index];
  if(index > peiskernel.highestConnection) peiskernel.highestConnection=index;

  /* Lookup the given hostname */
  h_errno=0;
  hostent=gethostbyname(name);
  if(!hostent) {
    fprintf(stdout,"peisk: gethostbyname(%s)  failed. h_errno=%d hostent=%x\n",
	    name,h_errno,(unsigned int)(unsigned long) hostent);
    return -1;
  }

  /* Create the sockaddr structures */
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;
  bzero(&(addr.sin_zero),8);
  bcopy((char *)hostent->h_addr,(char *)&addr.sin_addr,hostent->h_length);
  len=sizeof(struct sockaddr_in); /*hostent->h_length;*/

  /* Create a socket to use and bind it to a fresh port */
  sock=socket(AF_INET,SOCK_DGRAM,0);

  /* Set socket non blocking */
  if(fcntl(sock,F_SETFL,O_NONBLOCK) == -1) {
    fprintf(stderr,"peisk: error setting socket nonblocking, not connecting\n");
    close(sock);
    return -1;
  }

  /* Store socket, destination etc. in connection */
  connection->type=eUDPConnection;
  connection->connection.udp.socket = sock;
  memcpy(&connection->connection.udp.addr,&addr,sizeof(struct sockaddr_in));
  connection->connection.udp.len = len;

  connection->connection.udp.status=eUDPPending;
  peisk_initConnection(connection);

  if(flags & PEISK_CONNECT_FLAG_FORCE_BCAST)
    peiskernel.connections[index].forceBroadcasts=1;
  else
    peiskernel.connections[index].forceBroadcasts=0;

  /* Send connection strings */
  long version = htonl(peisk_protocollVersion);
  memcpy((void*)buf,(void*)&version,sizeof(version));
  /* Setup connection string message.
     Make sure connection string is null terminated, put connection
     flags as last byte. */
  strcpy(str,peisk_networkString);
  str[62]=0;
  str[63]=flags & PEISK_CONNECT_FLAG_FORCE_BCAST;
  memcpy((void*)buf+sizeof(version),(void*)str,64);

  /*sendto(sock,(void*)buf,sizeof(int)+strlen(peisk_networkString)+1,MSG_NOSIGNAL,&connection->connection.udp.addr,connection->connection.udp.len);*/
  ret=sendto(sock,(void*)buf,sizeof(int)+64,MSG_NOSIGNAL,&connection->connection.udp.addr,connection->connection.udp.len);
  if(ret == -1) perror("peisk::udpConnect::sendto");

  return connection->id;
}

void peisk_acceptUDPConnections() {
  struct sockaddr addr;
  socklen_t len;
  char buf[1024];
  int size;
  int sock;
  int index;
  int flags;
  struct sockaddr_in *in_addr;
  PeisConnection *connection;

  if(peiskernel.udp_serverPort == 0) return;

  len=sizeof(addr);
  size=recvfrom(peiskernel.udp_serverSocket,buf,sizeof(buf),0,&addr,&len);
  if(size <= 0) return;
  printf("Incomming udp connection...\n");
  if(size < sizeof(int)) { printf("wrong size %d\n",size); return; }
  long version=ntohl(*((long*)buf));
  if(peisk_protocollVersion != version) {
    printf("wrong version, got %d. expected %d\n",(int)version,(int)peisk_protocollVersion);
    if(version > peisk_protocollVersion && version < 1000)
      printf("ERROR - A newer version of the  PEIS kernel is available!\n");
    return;
  }
  buf[size-1]=0;
  if(strcmp(buf+sizeof(int),peisk_networkString) !=  0) { printf("bad network string '%s'\n",buf+sizeof(int)); }

  /* Extract flags from last byte of message */
  flags=buf[sizeof(int)+63];

  /* Ok, this looks like a valid connection attempt */

  /* find a connection structure to use */
  for(index=0;index<PEISK_MAX_CONNECTIONS;index++)
    if(peiskernel.connections[index].id == -1) break;
  if(index == PEISK_MAX_CONNECTIONS) {
    if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
      fprintf(stdout,"peisk: too many connections - refusing peer\n");
    /* \todo is this a bug? */
    /*close(sock);*/
    return;
  }
  connection = &peiskernel.connections[index];
  if(index > peiskernel.highestConnection) peiskernel.highestConnection=index;

  /* store addr, len in this connection structure */
  peisk_initConnection(connection);
  connection->type=eUDPConnection;
  connection->connection.udp.addr = addr;
  connection->connection.udp.len = len;
  connection->connection.udp.status=eUDPConnected;

  if(flags & PEISK_CONNECT_FLAG_FORCE_BCAST)
    peiskernel.connections[index].forceBroadcasts=1;
  else
    peiskernel.connections[index].forceBroadcasts=0;

  /* debug */
  in_addr=(struct sockaddr_in *)&connection->connection.udp.addr;
  printf("incoming port: %d\n",ntohs(in_addr->sin_port));

  /* create a new socket */
  sock=socket(AF_INET,SOCK_DGRAM,0);
  /* Set it non blocking */
  if(fcntl(sock,F_SETFL,O_NONBLOCK) == -1) {
    fprintf(stderr,"peisk: error setting socket nonblocking, not connecting\n");
    close(sock);
    connection->id = -1;
    return;
  }
  /* store this socket in the connection */
  connection->connection.udp.socket = sock;

  /* Send a response acknowledging that connection is ok */
  sendto(sock,(void*)"ack",4,MSG_NOSIGNAL,&addr,len);

  /* that's it, done */
  printf("new incomming UDP connection %d finished\n",connection->id);
}

int peisk_udpSendAtomic(PeisConnection *connection,PeisPackage *package,int len) {
  int status, timeout, value;
  void *data = (void*) package;
  struct sockaddr_in *in_addr;
  PeisPackageHeader *header;

  if(connection->outgoing >= connection->maxOutgoing * PEISK_CONNECTION_CONTROL_PERIOD) { connection->isThrottled=1; return -1; }
  connection->outgoing += PEISK_ASSUMED_UDP_OVERHEAD + len;
  
  /* Give package the next link count used for congestion control. */
  header=&package->header;
  header->linkCnt = htonl(connection->outgoingIdCnt++);

  errno=0;
  /*status=sendto(connection->connection.udp.socket,(void*) data,len,MSG_DONTWAIT|MSG_NOSIGNAL,&connection->connection.udp.addr,connection->connection.udp.len);*/
  status=sendto(connection->connection.udp.socket,(void*) data,len,MSG_NOSIGNAL,&connection->connection.udp.addr,connection->connection.udp.len);
  if(status == -1) {
    perror("failed udp send");
    printf("len: %d (%d)\n",(int)connection->connection.udp.len,(int)sizeof(struct sockaddr_in));
    printf("socket: %d\n",connection->connection.udp.socket);
    in_addr=(struct sockaddr_in*) &connection->connection.udp.addr;
    printf("port: %d\n",ntohs(in_addr->sin_port));
    /*exit(0);*/
  }

  if(status == -1) return -1;
  if(status < len) {
    printf("Error, sending %d bytes on udp socket but only %d got sent\n",len,status);
    return -1;
  }
  connection->outgoing++;
}

int peisk_udpReceiveIncomming(struct PeisConnection *connection,struct PeisPackage *package) {
  errno=0;
  len=sizeof(addr);
  size=recvfrom(connection->connection.udp.socket,(void*)package,sizeof(PeisPackage),0,&addr,&len);
  if(size <= 0) return 0;  /* no message received */
  if(memcmp(&addr,&connection->connection.udp.addr,sizeof(struct sockaddr_in)) != 0) {
    printf("peisk::warning %s@%d: Got UDP message from unexpected source\n",__FILE__,__LINE__);
    return 0;
  }
  if(size!=sizeof(PeisPackageHeader)+ntohs(package->header.datalen)) {
    printf("peisk::warning %s@%d: Bad size of incoming UDP package. Got %d bytes, expected %d+%d=%d bytes",
	   __FILE__,__LINE__,
	   (int)size,(int)sizeof(PeisPackageHeader),ntohs(package->header.datalen),
	   (int)sizeof(PeisPackageHeader)+ntohs(package->header.datalen));
    return 0;
  }
  /* Update congestion control information */
  if(ntohl(package->header.linkCnt)+1 > connection->incomingIdHi)
    connection->incomingIdHi = ntohl(package->header.linkCnt)+1;
  connection->incomingIdSuccess++;

  return 1;
}
