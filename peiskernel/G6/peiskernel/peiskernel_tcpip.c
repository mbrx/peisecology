/** \file peiskernel_tcpip.h
    \brief TCP/IPv4 linklayer
    
    Provides functionalities for using TCP/IP interfaces with the peiskernel  and implements the basic TCP/IPv4 link layer functions
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

/* This is needed for SIOCOUTQ tests below. Can be disabled for platforms which does not have it. */
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
#include "linklayer.h"
#include "peiskernel_tcpip.h"

struct peiskernel_inetinfo peisk_inetInterface[32];
int peisk_nInetInterfaces;

void peiskernel_initNetInterfaces() {
  int i,val;
  int sockfd;
  struct ifconf ifc;
  struct ifreq ifr_buf[32];   /* Note, no more than 32 interfaces allowed */
  struct ifreq *ifrp, *ifend, *ifrnext;
  int flags;

  /* First, attempt to list all active network interfaces */
  //if (0 > (sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP))) {
  if (0 > (sockfd = socket(AF_INET, SOCK_DGRAM, 0))) {
    fprintf(stderr, "Cannot open socket.\n");
    exit(EXIT_FAILURE);
  }

  ifc.ifc_len = sizeof(ifr_buf);
  ifc.ifc_req = ifr_buf;
  val = ioctl(sockfd, SIOCGIFCONF, &ifc);
  if(val) {
    printf("SIOCFIFCONF failed with return value %d\n",val);
    perror("peiskernel_initNetInterfaces::SIOCGIFCONF");
    exit(EXIT_FAILURE);
  }

  ifrp = ifr_buf;
  ifend = (struct ifreq*) ((char*)ifrp + ifc.ifc_len);

  /* Iterate over all detected interfaces, extract info and count number of successfully extracted interfaces */
  peisk_nInetInterfaces=0;
  for(;ifrp<ifend;ifrp=ifrnext) {
    struct sockaddr_in sa; /* This variable is unused but neccessary for the sizeof() call below to work */

    // TODO: Make this a test for Darwin, or does it work under Linux and Windows/Cygwin also?
#ifdef HAVE_SOCKADDR_SA_LEN
    int n = ifrp->ifr_addr.sa_len + sizeof(ifrp->ifr_name);
    if(n<sizeof(struct ifreq))
      ifrnext = ifrp + 1;
    else
      ifrnext = (struct ifreq*)((char*)ifrp + n);
#else
    ifrnext = ifrp + 1;
#endif

    /* Test if this is a dupe, if so then skip past it */
    for(i=0;i<peisk_nInetInterfaces;i++)
      if(strncmp(ifrp->ifr_name,peisk_inetInterface[i].name,sizeof(peisk_inetInterface[i].name)) == 0) break;
    if(i != peisk_nInetInterfaces) continue;

    if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
      printf("Processing interface: %s...",ifrp->ifr_name);
    
#ifdef NOT_WORKING
    if(ifrp->ifr_addr.sa_family != AF_INET) {
      /* This is not an INET interface, dont know how to handle it */
      if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
	printf("not an INET interface, skipping\n");
      continue;
    }
#endif

    if (ioctl(sockfd, SIOCGIFFLAGS, ifrp)) {
      if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
	printf("failed to get flags\n");
      continue;  /* failed to get flags */
    }
    flags = ifrp->ifr_flags;
    if(!flags&IFF_UP) {
      if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
	printf("is not up\n");
      continue; /* Device is not up */
    }

    /* Copy interface name */
    if(ioctl(sockfd, SIOCGIFADDR, ifrp)) {
      if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
	printf("Failed to query for addr\n");
      continue;
    }

    strncpy(peisk_inetInterface[peisk_nInetInterfaces].name,ifrp->ifr_name,sizeof(peisk_inetInterface[peisk_nInetInterfaces].name));
    peisk_inetInterface[peisk_nInetInterfaces].ip = *((struct in_addr*) &ifrp->ifr_addr.sa_data[sizeof sa.sin_port]);

    /* Attempt to get the netmask address */
    if(ioctl(sockfd, SIOCGIFNETMASK, ifrp)) {
      if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
	printf("failed to get netmask\n");
      perror("peiskernel_initNetInterfaces::SIOCGIFNETMASK"); continue;
    }
    peisk_inetInterface[peisk_nInetInterfaces].netmask = *((struct in_addr*) &ifrp->ifr_addr.sa_data[0]); //sizeof sa.sin_port];


    /* Attempt to get the broadcast address */

    /* Many people don't have a broadcast address on the loopback device - which is a bad idea, so we have to compensate for it. This is an ugly hardcoded fix for this. */
    if(flags & IFF_LOOPBACK ) {
      /*|| strncmp(peisk_inetInterface[peisk_nInetInterfaces].name,"lo",(size_t) 2) == 0) {*/
      unsigned char *c = (unsigned char *) &peisk_inetInterface[peisk_nInetInterfaces].broadcast;
      c[0]=127; 
      c[1]=0; 
      c[2]=0; 
      c[3]=1; 
    } else if(flags & IFF_BROADCAST) {
      if(ioctl(sockfd, SIOCGIFBRDADDR, ifrp)) {
	if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
	  printf("failed to query for broadcast address\n");
	perror("peiskernel_initNetInterfaces::SIOCGIFNETMASK"); continue;
      }
      peisk_inetInterface[peisk_nInetInterfaces].broadcast = *(struct in_addr*) &ifrp->ifr_addr.sa_data[sizeof sa.sin_port];
    } else { 
      if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
	printf("has no broadcast address! FLAGS=%x\n",flags);
      else
	printf("ERROR: Interface %s has no broadcast address\n",ifrp->ifr_name);
      //continue;
    }

    if(flags & IFF_LOOPBACK) { 
      /*|| strncmp(peisk_inetInterface[peisk_nInetInterfaces].name,"lo",(size_t) 2) == 0) {*/
      peisk_inetInterface[peisk_nInetInterfaces].isLoopback=1;
    } else {
      peisk_inetInterface[peisk_nInetInterfaces].isLoopback=0;
    }

    /* Metric */
    if (ioctl(sockfd, SIOCGIFMETRIC, ifrp)) {
      if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
	printf("(ailed to read metric)...\n");
      continue; /* failed to read metric */
      peisk_inetInterface[peisk_nInetInterfaces].metric = 0; /* Fallback to default metric */
    } else
      peisk_inetInterface[peisk_nInetInterfaces].metric = ifrp->ifr_metric;

    /* MTU */
    if (ioctl(sockfd, SIOCGIFMTU, ifrp)) {
      if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
	printf("(failed to read metric)...\n");
      peisk_inetInterface[peisk_nInetInterfaces].mtu = 1500; /* Use a default MTU of 1500 */
      continue; /* failed to read MTU */
    } else
      peisk_inetInterface[peisk_nInetInterfaces].mtu = ifrp->ifr_mtu;

    /* Interface information succesfully extracted */
    peisk_nInetInterfaces++;
    if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
      printf("ok\n");
  }

  
  if(peisk_printLevel & PEISK_PRINT_STATUS) {
    printf("Found %d network interfaces\n",peisk_nInetInterfaces);
    for(i=0;i<peisk_nInetInterfaces;i++) {
      printf("Interface: %s\n",peisk_inetInterface[i].name);
      printf("  IP:        %s\n",inet_ntoa(peisk_inetInterface[i].ip));
      /*unsigned char *foo=(unsigned char *) &peisk_inetInterface[i].ip;*/
      /*printf("  IP:        %x %x %x %x\n",foo[0],foo[1],foo[2],foo[3]);*/
      printf("  Netmask:   %s\n",inet_ntoa(peisk_inetInterface[i].netmask));
      printf("  Broadcast: %s\n",inet_ntoa(peisk_inetInterface[i].broadcast));
      printf("  Metric:    %d\n",peisk_inetInterface[i].metric);
      printf("  MTU:       %d\n",peisk_inetInterface[i].mtu);
    }
  }
  
  /** \todo Should we close the socket in some other way??? */
  close(sockfd);
}

void peisk_restartServer() {
  struct sockaddr_in serverAddr;
  int on;
  /*int sin_size=sizeof(struct sockaddr_in);*/
  struct ip_mreq imreq;
  struct sockaddr_in addr;

  /** \todo If server already listening then first shutdown smoothly. */

  peiskernel.tcp_serverSocket = socket(AF_INET,SOCK_STREAM,0);
  serverAddr.sin_family=AF_INET;
  serverAddr.sin_addr.s_addr=INADDR_ANY;
  bzero(&(serverAddr.sin_zero),8);
  /* Attempt to bind a port until we have succeeded */
  while(1) {
    serverAddr.sin_port=htons(peiskernel.tcp_serverPort);
    if(bind(peiskernel.tcp_serverSocket,(struct sockaddr*)&serverAddr,sizeof(struct sockaddr))==-1) {
      /*fprintf(stderr,"peisk: Warning, failed to bind port %d\n",peiskernel.serverPort);*/
      peiskernel.tcp_serverPort=peiskernel.tcp_serverPort+1;
    } else break;
  }
  /* Note, we might want to increase the backlog used here. Think about it first. */
  if(listen(peiskernel.tcp_serverSocket,2) == -1) {
    fprintf(stderr,"peisk: could not listen to serverSocket, continuing without server\n");
    close(peiskernel.tcp_serverSocket);
    return;
  }
  /* Make tcp server socket nonblocking */
  if(fcntl(peiskernel.tcp_serverSocket,F_SETFL,O_NONBLOCK) == -1) {
    fprintf(stderr,"peisk: error setting serverSocket nonblocking, continuing without server\n");
    close(peiskernel.tcp_serverSocket);
    return;
  }

  peiskernel.udp_serverSocket = socket(AF_INET,SOCK_DGRAM,0);
  serverAddr.sin_family=AF_INET;
  serverAddr.sin_addr.s_addr=INADDR_ANY;
  bzero(&(serverAddr.sin_zero),8);
  /* Attempt to bind a port until we have succeeded */
  while(1) {
    serverAddr.sin_port=htons(peiskernel.udp_serverPort);
    if(bind(peiskernel.udp_serverSocket,(struct sockaddr*)&serverAddr,sizeof(struct sockaddr))==-1) {
      /*fprintf(stderr,"peisk: Warning, failed to bind port %d\n",peiskernel.serverPort);*/
      peiskernel.udp_serverPort=peiskernel.udp_serverPort+1;
    } else break;
  }
  /* Make udp server socket nonblocking */
  if(fcntl(peiskernel.udp_serverSocket,F_SETFL,O_NONBLOCK) == -1) {
    fprintf(stderr,"peisk: error setting UDP serverSocket nonblocking, continuing without it\n");
    close(peiskernel.udp_serverSocket);
    peiskernel.udp_serverPort=0;
  }


  /* server successfully started */
  peiskernel.tcp_isListening=1;

  /* receiving broadcasted packages */
  peiskernel.tcp_broadcast_receiver = socket(PF_INET,SOCK_DGRAM,IPPROTO_IP);

  memset(&addr, 0, sizeof(struct sockaddr_in));
  memset(&imreq, 0, sizeof(struct ip_mreq));

  addr.sin_family=PF_INET; /*WAS: AF_INET;*/
  addr.sin_port=htons(PEISK_MULTICAST_PORT);
  addr.sin_addr.s_addr=htonl(INADDR_ANY);
  on=1;

  /* join multicast group */
  imreq.imr_multiaddr.s_addr = inet_addr(PEISK_MULTICAST_IP);
  imreq.imr_interface.s_addr = INADDR_ANY; /* use default interface */

  /* Changed the order of the calls below: reuseaddr must be set before
   * binding the socket, but add_membership must be done after it is
   * bound.  --AS 060818 */

  /* try to make the broadcast receiver socket reusable */
  if (setsockopt(peiskernel.tcp_broadcast_receiver,
		 SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) != 0) {
    perror("Failed to make multicast socket reusable");
  }
  /* try to bind it */
  else if (bind(peiskernel.tcp_broadcast_receiver,
		(struct sockaddr*) &addr,sizeof(struct sockaddr_in)) != 0) {
    perror("Failed to bind multicast socket");
    peiskernel.tcp_broadcast_receiver=-1;
  }
  /* try to make it nonblocking */
  else if (fcntl(peiskernel.tcp_broadcast_receiver,F_SETFL,O_NONBLOCK) == -1) {
    perror("Failed to make broadcast socket non blocking");
    peiskernel.tcp_broadcast_receiver=-1;
  }
  /* join multicast group */
  else if(setsockopt(peiskernel.tcp_broadcast_receiver, IPPROTO_IP, IP_ADD_MEMBERSHIP,
		     (const void *)&imreq, sizeof(struct ip_mreq)) != 0) {
    perror("Error joining multicast group");
  }
  /* succeded */
  else if(peisk_printLevel & PEISK_PRINT_STATUS)
    printf("peisk: listening for multicasts on %s:%d\n",
	   PEISK_MULTICAST_IP,PEISK_MULTICAST_PORT);
}

/*********************************/
/*        TCP CONNECTIONS        */
/*********************************/

PeisConnection *peisk_tcpConnect(char *name,int port,int flags) {
  struct hostent *hostent;
  struct sockaddr_in peerAddr;
  int sock;

  int retval;
  socklen_t retlen;
  fd_set writeSet, readSet, exepSet;
  struct timeval timeout;
  PeisConnection *connection;

  /** \todo peisk_tcpConnect - refuse to connect to hosts we already
      are connected to. */

  /* Refuse connections to ourselves */
  if(port == peiskernel.tcp_serverPort &&
     (strcmp(name,"localhost") == 0 ||
      strcmp(name,"127.0.0.1") == 0 ||
      strcmp(name,peiskernel.hostInfo.hostname) == 0)) return NULL;

  /* Lookup hostname */
  h_errno=0;
  hostent=gethostbyname(name);
  if(!hostent) {
    fprintf(stdout,
	    "peisk: gethostbyname(%s)  failed. h_errno=%d hostent=%x\n",
	    name,h_errno,(unsigned int)(unsigned long) hostent);
    return NULL;
  }
 
  /* Create connection structure to use. This will allocate and
     place the connection in PENDING mode. */
  connection = peisk_newConnection();

  /* Prepare OS socket */
  sock=socket(AF_INET,SOCK_STREAM,0);
  if(sock == -1) {
    perror("Failed to create a new socket\n");
    return NULL;
  }
  peerAddr.sin_family = AF_INET;
  peerAddr.sin_port = htons(port);
  peerAddr.sin_addr.s_addr = INADDR_ANY;
  bzero(&(peerAddr.sin_zero),8);
  bcopy((char *)hostent->h_addr,(char *)&peerAddr.sin_addr,hostent->h_length);
  /* Set socket non blocking before connecting */
  if(fcntl(sock,F_SETFL,O_NONBLOCK) == -1) {
    fprintf(stderr,"peisk: error setting socket nonblocking, not connecting\n");
    close(sock);
    return NULL;
  }
  /* Start connecting */
  connect(sock,(struct sockaddr*)&peerAddr,sizeof(struct sockaddr));
  /** For now, wait and see if connection has succeeded within 100ms and continue afterwards.
      \todo Handle queued connection attempts better, eg. by placing them on a queue of pending connections */
  timeout.tv_sec=0;
  timeout.tv_usec=100000;  /* Use a timeout of 100ms */
  FD_ZERO(&readSet);
  FD_ZERO(&writeSet);
  FD_ZERO(&exepSet);
  FD_SET(sock,&writeSet);
  retval=select(sock+1, &readSet, &writeSet, &exepSet, &timeout);
  if(retval <= 0) {
    if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
      fprintf(stdout,"peisk: failed to connect to %s:%d.\n   Select returned: %d\n",name,port,retval);
    return NULL;
  }
  retlen=sizeof(int);
  if(getsockopt(sock,SOL_SOCKET,SO_ERROR,&retval,&retlen) == -1) {
    if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
      fprintf(stdout,"peisk: failed to connect to %s:%d.\n   Getsockopt failed, errno: %d\n",name,port,errno);
    return NULL;
  }
  if(retval) {
    if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
      fprintf(stdout,
	      "peisk: failed to connect to %s:%d.\n SO_ERROR is %d\n",
	      name,port,retval);
    return NULL;
  }

  connection->type=eTCPConnection;
  connection->connection.tcp.socket=sock;

  PeisConnectMessage message;
  peisk_initConnectMessage(&message,flags);
  send(sock,&message,sizeof(message),MSG_NOSIGNAL);
  peisk_outgoingConnectFinished(connection,flags);

  if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
    fprintf(stdout,"peisk: new outbound tcp/ip connection #%d to %s:%d established, flags=%d\n",
	    connection->id,name,port,flags);
  return connection;
}

void peisk_acceptTCPConnections() {
  int sin_size=sizeof(struct sockaddr_in);
  int sock;
  struct sockaddr_in peerAddr;
  PeisConnection *connection;
  PeisConnectMessage message;

  if(!peiskernel.tcp_isListening)  return;

  sock=accept(peiskernel.tcp_serverSocket,(struct sockaddr *)&peerAddr,(socklen_t*) &sin_size);
  if(sock <= 0) return;

  if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
    printf("peisk: incomming TCP connection ...\n");
  
  if(fcntl(sock,F_SETFL,O_NONBLOCK) == -1) {
    fprintf(stderr,"peisk: error setting socket nonblocking, closing connection\n");
    close(sock);
    return;
  }
  
  /* Create connection structure to use */
  connection = peisk_newConnection();
  if(!connection) return;
  
  /* Wait up to 200ms for data - otherwise drop connection */    
  if(!peisk_recvBlocking(sock,&message,sizeof(message),0,0.2) && (peisk_printLevel & PEISK_PRINT_CONNECTIONS)) {
    printf("Timeout on incomming connection (1)\n");
    peisk_freeConnection(connection);
    close(sock); return;
  }
  /* Check that it's ok to accept this connection */
  if(peisk_verifyConnectMessage(connection,&message) != 0) {
    peisk_freeConnection(connection);
    close(sock); return;
  }
    
  /* Store TCP information in connection */
  connection->type = eTCPConnection;
  connection->connection.tcp.socket=sock;
  
  /* Let P2P layer handle this connection */    
  peisk_incommingConnectFinished(connection,&message);
    
  if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
    printf("peisk: accepted new connection to %d with index: %d, flags=%d\n",
	   message.id,connection->id,message.flags);
  
}


/*********************************/
/*    MULTICASTS CONNECTIONS     */
/*********************************/


/** Listen to multicast/broadcasted UDP packages announcing peiskernels on the local network */
void peisk_acceptUDPMulticasts() {
  int size;
  PeisUDPBroadcastAnnounceMessage message;

  if(peiskernel.tcp_broadcast_receiver <= 0) return;
  while(1) {
    size=recvfrom(peiskernel.tcp_broadcast_receiver,(void*)&message,sizeof(message),MSG_DONTWAIT,NULL,NULL);
    if(size == -1) return;
    if(size == sizeof(message)) {
      /* convert broadcasted message so that all subfields within it is now in HOST order */
      message.protocollVersion = ntohl(message.protocollVersion);
      peisk_ntoh_hostInfo(&message.hostinfo);
      
      if(message.hostinfo.id > 65535) {
	/* Just for debugging - until we start using longer PEIS-id's */
	printf("Warning - getting hostinfo with id = %d (%x) on the multicast\n",message.hostinfo.id,message.hostinfo.id);
      }
      
      /* Check if it matches the network connection strings */
      message.networkString[63]=0;
      if(strcmp(message.networkString,peisk_networkString) != 0) return;
      if(message.protocollVersion > peisk_protocollVersion && message.protocollVersion < 1000) {
	printf("ERROR - A newer version of the PEIS kernel exists - you should upgrade!\n");
	peisk_shutdown();
      }
      if(message.protocollVersion != peisk_protocollVersion) return;
      
      message.hostinfo.fullname[PEISK_MAX_HOSTINFO_NAME_LEN-1]=0;   /* force name part of hostinfo to be a null terminated string */
      
      
      /*printf("peisk: received host announcement for '%s'\n",hostinfo.fullname);*/
      /* We received a full PeisHostInfo field describing another available host */
      if(message.hostinfo.id == -1 || message.hostinfo.id == peisk_id) return;
      
      peisk_handleBroadcastedHostInfo(&message.hostinfo);

      /* Reset counter for deciding if we are allowed to broadcast if this one comes from our
	 host and have a lower ID than us. */
      if(message.hostinfo.id < peiskernel.id &&
	 strcmp(message.hostinfo.hostname,peiskernel.hostInfo.hostname) == 0)
	peiskernel.broadcastingCounter = 0;
    }
  }
}

int peisk_ipIsConnectable(unsigned char ip[4],int port) {
  int i,j;
  unsigned char *netmask;
  unsigned char *addr;

  int isPrivateNetwork=0;
  /* This is the definition of private IPv4 networks according to IETF and IANA, see RFC 1918 */
  if(ip[0] == 10) isPrivateNetwork=1;
  if(ip[0] == 172 && ip[1] >= 16 && ip[1] <= 31) isPrivateNetwork=1;
  if(ip[0] == 192 && ip[1] == 168) isPrivateNetwork=1;

  //printf("IsPrivate: %d\n",isPrivateNetwork);

  /* Check all our interface to see if we have one that can reach this address */
  for(i=0;i<peisk_nInetInterfaces;i++) {
    /* Assume that we can reach any public IP's if we have any IPv4 network device that is not loopback */
    if(!isPrivateNetwork && peisk_inetInterface[i].isLoopback == 0) 
      /* It's a public IP, and we seem to have access to the internet */
      return 1;

    netmask = (unsigned char *) &peisk_inetInterface[i].netmask;
    addr = (unsigned char *) &peisk_inetInterface[i].ip;
    //printf("ip is: %d %d %d %d\n",ip[0],ip[1],ip[2],ip[3]);
    //printf("netmask is: %d %d %d %d\n",~netmask[0],~netmask[1],~netmask[2],~netmask[3]);
    //printf("addr: %d %d %d %d\n",addr[0],addr[1],addr[2],addr[3]);
    for(j=0;j<4;j++)
      if((ip[j] & (~netmask[j])) != (addr[j] & (~netmask[j]))) break;
    if(j == 4) { 
      //printf("Matches, we can connect to it\n"); 
      return 1; 
    }
    //else printf("Cannot use this interface\n");
  }
  //printf("Does not match... not connectable\n");
  return 0;
}

int peisk_tcpSendAtomic(PeisConnection *connection,PeisPackage *package,int len) {
  int status, timeout, value;
  void *data = (void*) package;
  PeisPackageHeader *header;

  /* First check if this data would fit into buffer. Might have
     portability issues to other platforms but a reasonable fallback
     is to not do these tests. The other code below should handle the
     remaining cases somewhat worse. */

  value=0;
  /* CYGWIN does not have these...  --AS 060816 */
#ifndef __CYGWIN__
  /* MACOS X does not have these...  --MB 061120 */
#ifndef __MACH__
  status=ioctl(connection->connection.tcp.socket, SIOCOUTQ, &value);
  /* todo - use SIOCINQ on input from tcp sockets... */
  if(status) perror("peisk_connection_sendAtomic::ioctl(SIOCOUTQ)");
  /* todo - use getsockopt(SO_RCVBUFFER) to know the buffer size */
  if(value > 20000) return -1; /* Too much data in buffer, come back later */
#endif
#endif

  header=&package->header;
  header->linkCnt = htonl(connection->outgoingIdCnt++);

  status=send(connection->connection.tcp.socket,(void*) data,len,MSG_DONTWAIT|MSG_NOSIGNAL);
  if(status != -1 && status < len) {
    if(peisk_printLevel & PEISK_PRINT_PACKAGE_ERR)
      printf("Warning! Package sent only partially errno=%d, Status=%d\n",errno,status);
    timeout=0;
    while(1) {
      if(status != -1) { data += status; len -= status; if(!len) break; }
      if(++timeout > 100*2) { printf("Warning - forced to break package! (2s timeout)\n"); errno=EPIPE; break; }
      peisk_waitForWrite(connection->connection.tcp.socket,0.01);
      errno=0;
      status=send(connection->connection.tcp.socket,(void*) data,len,MSG_DONTWAIT|MSG_NOSIGNAL);
      if(errno == EPIPE) break;
      if(status == -1 && errno != EAGAIN) {
	/* todo - make this printout dependent on debug level? */
	printf("error sending package - errno=%d '%s'\n",errno,strerror(errno));
	break;
      }
    }
  }
  if(errno == EPIPE || errno == EBADF || errno == EINVAL) {
    if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
      fprintf(stdout,"peisk: tcp connection %d broken pipe (header)\n",connection->id);
    peisk_closeConnection(connection->id);
    return -1;
  } else if(errno == EAGAIN) {
    if(status > 0) { if(peisk_printLevel & PEISK_PRINT_PACKAGE_ERR) printf("send - EAGAIN, status=%d\n",status); }
    /* Package was not sent - don't remove it from queue and don't attempt any more packages for now */
    /* Also, decrease outgoingIdCnt so we do not count this package
       multiple times */
    connection->outgoingIdCnt--;
    return -1;
  }
  return 0;
}

int peisk_tcpReceiveIncomming(struct PeisConnection *connection,struct PeisPackage *package) {
  int status;
  
  errno=0;
  status=recv(connection->connection.tcp.socket,(void*)package,sizeof(PeisPackage),MSG_PEEK|MSG_NOSIGNAL);
  if(errno == EAGAIN) return 0;                 /* Nothing received, come back later */
  if(errno == EPIPE || status == -1) {          /* Error - close socket */
    if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
      printf("peisk: warning - error (1) in receive, closing socket\n");
    peisk_closeConnection(connection->id);
    return 0;
  }
  if(status < sizeof(package->header)) return 0; /* We have not yet received the full header, come back later */

  if(ntohs(package->header.datalen) > PEISK_MAX_PACKAGE_SIZE) { /* Check for bad package length */
    if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
      fprintf(stdout,"peisk: error, too large (%d) package received on connection %d\n",package->header.datalen,connection->id);
    /*peisk_syncflush(connection);*/
    peisk_closeConnection(connection->id);
    return 0;
  }

  if(status < sizeof(package->header) + ntohs(package->header.datalen))
    /* We have not yet received the full data, come back later */
    return 0;

  /* Ok, we have all the data we wanted, read it once again without MSG_PEEK to get rid of it */
  errno=0;
  status=recv(connection->connection.tcp.socket,
	      (void*)package,sizeof(package->header) + ntohs(package->header.datalen),MSG_NOSIGNAL);
  if(errno == EAGAIN || errno == EPIPE || status == -1 ||
     status < sizeof(package->header) + ntohs(package->header.datalen)) {
    if(peisk_printLevel & PEISK_PRINT_CONNECTIONS)
      printf("peisk: warning - error (2) in receive, closing socket. status=%d\n",status);
    peisk_closeConnection(connection->id);
    return 0;
  }
  return 1;
}
