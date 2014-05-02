/** \file peiskernel_tcpip.h
    Provides functionalities for using TCP/IP interfaces with the peiskernel
*/
/*
    Copyright (C) 2005 - 2006  Mathias Broxvall

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

#ifndef PEISKERNEL_TCPIP_H
#define PEISKERNEL_TCPIP_H

/** \ingroup LinkLayer */
/** \defgroup TcpIP  TCP/IPv4

  In the begining only tcp/ip and udp/ip connections between peis
  kernels are handled. However, as time progresses more alternative
  interfaces can be added such as serial radio modems etc. 

  When a peis kernel boots up the computer will be queries for all
  currently active network interfaces. These interfaces will be used
  for listening for other active peiskernels on and for multicasting
  messages announcing the presence of this peiskernel. These multicasts
  are usually limited to the local subnet, depending on the
  limitations of any routers on the TCP/IP level, and is a way for ensuring that
  peiskernels automatically connect to each other. 
*/
/** @{ */

struct peiskernel_inetinfo {
  char name[8];
  char isLoopback;
  char padding[3];
  struct in_addr ip;
  struct in_addr netmask;
  struct in_addr broadcast;
  unsigned int mtu;
  unsigned int metric;
};

/** Message structure for UDP multicast messages.
    These structures appear in host byte order until it is sent over the network. */
typedef struct PeisUDPBroadcastAnnounceMessage {
  /** Protocoll version used by PEIS-kernel */
  int protocollVersion;
  unsigned char padding1[4];
  /** Network ID used to separate different ecologies */
  char networkString[64];
  /** Hostinfo for this host containing connection information etc. */
  PeisHostInfo hostinfo;
} PeisUDPBroadcastAnnounceMessage;

extern struct peiskernel_inetinfo peisk_inetInterface[32];
extern int peisk_nInetInterfaces;

/** Initializes all TCP/IP interfaces */
void peiskernel_initNetInterfaces();
/** Checks for new incomming tcp connections */
void peisk_acceptTCPConnections();                  

/** Attempts to create a TCP/IP connection towards target. Returns a PENDING connection structure, or NULL on failure.  */
struct PeisConnection *peisk_tcpConnect(char *name,int port,int flags);
#ifdef OLD
/** Returns -1 if failed, otherwise connection id  number */
int peisk_udpConnect(char *name,int port,int flags);
#endif


/** Restarts the TCP/UDP/IPv4 server listening for connecting peers */
void peisk_restartServer();                         

/** True if it is beliveable that we can connect to this
    address. Checks all our IPv4 interface to see if we have one that
    can reach this address. This check is based on  identifying which
    network it is on, we assume 
    that we do not have routing between separate IP networks and/or
    firewalls separating them. This is somewhat too pessistic and a
    better implementation should be made in the future. */
int peisk_ipIsConnectable(unsigned char ip[4],int port);

/** Performs the sending of data on a TCP/IP connection. Package
    points to a PeisPackage structure including data for a total 
    length of len bytes. Returns zero on success. */
int peisk_tcpSendAtomic(PeisConnection *connection,PeisPackage *package,int len);

/** Attempts to read a package from the connection. 
    Returns non-zero if there was a package.  */
int peisk_tcpReceiveIncomming(struct PeisConnection *connection,struct PeisPackage *package);

/* @} TCP/IPv4 */


#endif
