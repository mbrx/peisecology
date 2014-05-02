/** \file linklayer.h
    Defines the basic functions shared by all linklayer(s)
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

#ifndef LINKLAYER_H
#define LINKLAYER_H

#ifndef P2P_H
struct PeisConnection;
#endif

/** \ingroup peisk */
/*\{@*/
/** \defgroup LinkLayer The link layer
    \brief This is the lowermost layer in the *overlay*
    network dealing with any connections. Not to be confused with the
    OSI linklayer, in fact, the API layer of the OSI stack for
    eg. TCP/UDP can be considered the link layer for the peiskernel. 

    Provides connection capabilities for the P2P layer. Offers
    
    - Notion of lowlevel addresses that can be used in establishing connections
    - Discovery of possible connections to establish
    - Connections that can send 1Kbyte large packages to neighbours. 

    The link layer is built with modules for different link
    mechanisms. Currently we have these mechanism, as more estoteric
    hardware becomes available we may add more mechanisms. 

    - TCP/IPv4: Currently the dominant link mechanism. Provides
    bidirectional links to any other PEIS on the same ethernet
    network. Sometimes also to other networks. 
    - UDP/IPv4: Currenly not used. Promises a few advantages with
    regards to speed, overhead and latencies. 
    - Bluetooth: Creates connections between any bluetooth connected
    peis as soon as they enter the same physical space. Allows for a
    much simpler (even non existant) infrastructure, ie. no network
    configurations are required for the PEIS to find each other.     

    @{
*/
/** Hardcoded multicast address from reserved IANA block. See
   http://www.iana.org/assignments/multicast-addresses */
#define PEISK_MULTICAST_IP        "227.1.3.5"

/** UDP port used for broadcasting peis presence on multicast networks */
#define PEISK_MULTICAST_PORT      10001

/** Synchronisation magic byte used when flushing stream based (eg. TCP) connections. */
#define PEISK_SYNC                htonl(0xff00aa00)

/** Timeout for reading data packages */
#define PEISK_READ_PACKAGE_TIMEOUT  5.0

/** The different lowlevel addresses available. */
typedef enum { ePeisTcpIPv4=0, ePeisUdpIPv4, ePeisTcpIPv6, ePeisBluetooth } PeisLowlevelAddressType;
/** Representation of different lowlevel addresses. This structure is
    always stored in HOST byte order. */
typedef struct PeisLowlevelAddress {
  char deviceName[8];
  char isLoopback;
  unsigned char padding[3];
  PeisLowlevelAddressType type;
  union {
    struct { unsigned char ip[4]; int port; } tcpIPv4;
    struct { unsigned char ip[4]; int port; } udpIPv4;
    struct { unsigned char baddr[6]; short port; } bluetooth;
  } addr;
} PeisLowlevelAddress;

typedef enum { eTCPConnection, eUDPConnection, eSerialConnection, eBluetoothConnection } PeisConnectionType;
typedef enum { eUDPConnected, eUDPPending } PeisUDPStatus;


/** \brief Assumed overhead for each UDP package sent out */
#define PEISK_ASSUMED_UDP_OVERHEAD       20

/* Make advance declarations of structures needed as
   parameters. Actual declarations are done later in the
   peiskernel_private.h file.  */
struct PeisPackage;
struct PeisConnection;


/** Returns true if it is belivable that the given address can be
    connected to. Uses the underlying heuristics of the specific link
    type given inside the address structure. */
int peisk_linkIsConnectable(PeisLowlevelAddress *address);

/** Send data on connection atomically without delaying. 
    This is one of the major interface between the P2P layer and
    the link layer. 

    Package points to a PeisPackage structure including data 
    for a total length of len bytes. Returns zero on
    success. */
int peisk_connection_sendAtomic(struct PeisConnection *connection,struct PeisPackage *package,int len);

/** Attempts to read a package from the connection. This is one of the
    major interface between the P2P layer and the link layer.    
    Returns non-zero if there was a package.  */
int peisk_connection_receiveIncomming(struct PeisConnection *connection,struct PeisPackage *package);

/** Manages the speed of all connections requiring congestion control */
void peisk_periodic_connectionControl1(void *data);       
/** Manages the speed of all connections requiring congestion control */
void peisk_periodic_connectionControl2(void *data);       

/** Announces our precence on local (ethernet) network */
void peisk_periodic_inet_broadcast(void *data);           

/** Responds to UDP speed and congestion control packages transmitted on the P2P layer */
int peisk_hook_udp_speed(int port,int destination,int sender,int datalen,void *data);

/** Reads bytes from a STREAM socket, blocks until all bytes received
    or timeout reached */
int peisk_recvBlocking(int fd,void *buf,size_t len,int flags,double timeout); 
/** Reads all or nothing of a package of given length from a STREAM socket. 
     Blocks if there is atleast one byte but less than the number of requested bytes */
int peisk_recvAtomic(int fd,void *buf,size_t len,int flags);

/** Intializes and returns the next usable connection structure. Returns NULL on error. */
struct PeisConnection *peisk_newConnection();

/** Attempts to setup a connection to the given target. Sets up a
    "pending" connection. Returns NULL if failure can be detected
    immediatly, otherwise connection structure for pending
    connection. Accepts the different PEISK_CONNECT_FLAG_* flags. 

    Targets are specified as protocoll://address where protocoll and
    address are linklayer specific. Eg. tcp://sand65:8000 attempts to
    establish a connection using TCP/IP to the given name and port,
    bt://AA:BB:CC:DD:EE:EF;8000 attempts to connect to bluetooth
    device AA:BB:CC:DD:EE:EF using L2CAP port 8000. 
*/
struct PeisConnection *peisk_connect(char *url,int flags);                                


/*\}@ Linklayer */
/*\}@ Ingroup peisk */

/** \ingroup LinkLayer */
/** \defgroup ConnectMessage Connection Request Message
    \brief Message when establishing new linklayer connections

    Connection request messages are a message sent through underlying
    linklayer interfaces when an interface of one peis is establishing
    a PeisConnection with another peis. See also \ref PeisConnection. 

    These connection request messages contains the network string,
    peis kernel version and peis id. Thereby making sure
    that peis of different networks or version do not intermix. 
 */
/** @{ */

typedef struct PeisConnectMessage {
  int version, id, flags;
  char networkString[64];  
} PeisConnectMessage;

/** Initializes a connection request message and translates to network
    byteorder */
void peisk_initConnectMessage(PeisConnectMessage *,int flags);

/** Verifies a connection request message and translates to host byte order. Returns zero if message was
    successfully parsed and we should keep the connection. Non zero
    on error or too many connections.  */
int peisk_verifyConnectMessage(struct PeisConnection *,PeisConnectMessage *);

/** @} ConnectMessage */




#endif
