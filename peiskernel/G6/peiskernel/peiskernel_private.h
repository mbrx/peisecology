/** @file peiskernel_private.h
    \brief Defines private interfaces to the peiskernel library.

    This header file is included by all internal code of the
    peiskernel, and can be included by some very specific middleware
    components such as the tupleview or peisinit - but should never be
    included by other user components. If you realy need to access some
    internal functions/variables of the peiskernel this might be a
    fault of the API and I would appreciate to be contacted with the
    reasoning behind - perhaps major change to the library will be needed
    then.

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

#ifndef PEISKERNEL_PRIVATE_H
#define PEISKERNEL_PRIVATE_H

#include <sys/types.h>
#include <sys/socket.h>

#ifndef MAX
#define MAX(x,y) (x>y?x:y)
#endif

#ifndef MIN
#define MIN(x,y) (x<y?x:y)
#endif

typedef long intA;   /* Architecture dependent int, of the same size as pointers */
typedef int int32;   /* Guaranteed 32 bit integer, 4 bytes in size */
typedef long long int int64; /* Guaranteed 64 bit integer, 8 bytes in size */
typedef unsigned int uint32;
typedef unsigned long long int uint64;


/* Fix some compile problems with MacOS */
#ifdef DARWIN
#define MSG_NOSIGNAL 0
#endif

/** \defgroup peisk The PEIS kernel
    The PEIS kernel is a software library written in pure C and with
    as few library dependencies as possible. The purpose of this
    library is to enable software programs to participate as PEIS
    components in the PEIS Ecology network.  

    Internally, the PEIS kernel consists of a number of modules, which
    can loosely be organized as a number of layers providing
    services to higher layes, and in the end the software
    application. 
    These layers are:

    - The \ref LinkLayer "link layer" which provides basic
      communication channels between different software instances on
      the same computer, or distributed on any computers on a ethernet
      network or connectable by bluetooth connections. 
    - The \ref P2PLayer "P2P layer" which creates an ad-hoc P2P
      network ontop of all available network links in order to let any
      PEIS component in the ecology communicate with any other
      components. 
    - The \ref tuples "Tuple layer" which implements the basic
      tuplespace on top of the P2P layer. Although this layer provides the core
      of the services used by applications, some of the other library
      function are of use in some applications. 

      For a complete list of all modules in the peiskernel, see the
      Doxygen page for software modules. 
*/
/** \{@ */

/** This is the maximum number of hops between nodes in the network */
#define PEISK_MAX_HOPS           15         /* Noone needs more than 640kbytes of memory anyway... */

/** The maximum size of packages that can be send through the network atomically */
#define PEISK_MAX_PACKAGE_SIZE 1024

/** Maximum length of the human readable fullname of all known hosts */
#define PEISK_MAX_HOSTINFO_NAME_LEN  64

/** Maximum number of direct connection requests that can be queued due 
    to sending packages towards a host. */
#define PEISK_MAX_DIR_CONN_REQS       8
/** Maximum number of (reported) lowlevel addresses */
#define PEISK_MAX_LOWLEVEL_ADDRESSES  16
/* This was 8, which is < than peisk_nInetInterfaces on some computers */
/* I raised it to 16, but there must be a better way... --AS */



#ifndef HASHTABLE_H
#include "hashtable.h"
#endif

#ifndef LINKLAYER_H
#include "linklayer.h"
#endif

#ifndef SERVICES_H
#include "services.h"
#endif

#ifndef P2P_H
#include "p2p.h"
#endif

/*********************************************************************************/
/*                                                                               */
/*        Private variables, types and functions used by peis kernel core        */
/*                                                                               */
/*********************************************************************************/

/** Defines special assert statements so that they all can easily be removed in the future.
    Usage: PEISK_ASSERT(x>0,("ERROR, X has the value %d\n",x))
    Note the use of the paranthesis around the second argument!
*/
#define PEISK_ASSERT(condition,message) {if(!(condition)) {printf("ASSERTION FAILED %s@%d:\n  ",__FILE__,__LINE__); printf message;}}


/** Contains most of the data structures needed for the PEIS kernel
    state and it's generic functions. */
typedef struct PeisKernel {
  /** Address of this kernel on the network. */
  int id;

  /** Pseudo random number initialized on bootup to detect
      colliding PEIS id's during the routing phase. */
  int magicId;

  /** Timepoint of previous step, use for speed calculations and timers. Also for caching timenow calculations during the peisk_step and all subsequent functions. */
  double lastStep;
 /** Average time between steps over a sliding window */
  double avgStepTime;

  PeisHostInfo hostInfo;                   /**< The host info propagated by this host */
  int doShutdown;                          /**< True if a shutdown has been requested. */
  unsigned char padding[4];

  /** Simulates package losses, used for debuging robustness */
  double simulatePackageLoss;

  /** This kernel is a leaf node preventing all incomming/outgoing
      connections to remote (not on same CPU) hosts except those
      explicitly setup with command line arguments. */
  char isLeaf;
  /** Filled when an AckHook is called in the P2P layer with the specific type of error (or success) */
  char ackHookFailureType;
  unsigned char padding2[2];

  /** Hashtable giving routing information for all destinations. */
  PeisHashTable *routingTable;

  /* Loop detection */
  int nextLoopIndex;                                          /**< Index of next free loopinfo struct */
  unsigned char padding3[4];
  int loopHashTable[PEISK_LOOPINFO_HASH_SIZE];                /**< Hashtble for loop detection */
  PeisLoopInfo loopTable[PEISK_LOOPINFO_SIZE];                /**< Preallocated memory for loopdetection hashtable entries */

  /* Long messages */
  PeisAssemblyBuffer assemblyBuffers[PEISK_MAX_LONG_MESSAGES]; /** Stores incomming packages when receiving longer messages (over 1kb). */
  /* Autohost */
  PeisAutoHost autohosts[PEISK_MAX_AUTOHOSTS];                 /**< List of hosts will that regularly be attempted to connect to. */
  int nAutohosts;                                              /**< Current size of autohost list */

  /* TCP/IP specifics */
  int tcp_serverPort;                                         /**< TCP/IP port number listening for incomming connections */
  int tcp_serverSocket, tcp_isListening;
  int tcp_broadcast_receiver;

  /* Allow UDP/IP so we can increase throughput */
  int udp_serverPort;                                         /**< UDP/IP port number listening for incomming connections */
  int udp_serverSocket;

  /* Connections */
  int nextConnectionId;
  /** Contains data for all _active_ connections */
  PeisConnection connections[PEISK_MAX_CONNECTIONS];
  /** Highest connection index that are in use */
  int highestConnection;

  /** Request for direct connection towards these targets */
  int dirConnReqs[PEISK_MAX_DIR_CONN_REQS];
  int nDirConnReqs;

  /** Preallocated packages to be used when queueing outgoing data */
  PeisQueuedPackage *freeQueuedPackages;

  /** Callback hooks for all incomming packages at given port */
  PeisHookList *hooks[PEISK_NPORTS];

  unsigned char padding4[4];
  /** Used to store functions that will be called periodically */
  PeisPeriodicInfo periodics[PEISK_MAX_PERIODICS];

  /** Highest numbered periodics function */
  int highestPeriodic;

  /** How much to add to our local time to get the global
      (synchornized) time */
  int timeOffset[2];
  char isTimeMaster;
  unsigned char padding5[3];

  /** Pending acknowledgement packages to send. These are buffered for
      efficiency. */
  PeisPendingAck acknowledgementPackages[PEISK_MAX_ACK_PACKAGES];
  /** Number of (currently) stored acknowledgement packages. */
  int nAcknowledgementPackages;

  /** The id of the last generated acknowledgement ID, useful for
      functions that spoof the sender of messages. Ie, the TinyGateway */
  long lastGeneratedAckID;

  /*****************************************/
  /**************** STATISTICS *************/
  /*****************************************/

  /** Keeps track of number of incoming data since last statistics
      update. */
  int incomingTraffic;

  /** Keeps track of number of outgoing data since last statistics
      update. */
  int outgoingTraffic;

  unsigned char padding6[4];
  /** User definable hook to call when a host is removed from our routing tables. */
  PeisDeadhostHook *deadhostHook;
  /** Argument to be passed when calling the deadhost hook */
  void *deadhostHook_arg;

  /** \ingroup Acknowledgements 
      A stack of hooks to apply to all future packages */
  PeisAcknowledgementHook *withAckHook[PEISK_MAX_ACKHOOKS];
  /** \ingroup Acknowledgements
      A stack of data to be given to the respective acknowledment hooks */
  void *withAckHookData[PEISK_MAX_ACKHOOKS];
  /** \ingroup Acknowledgements
      Number of items in the current acknowledgment hook stack. */
  int nAckHooks;


  /** How many peisk_step's have elapsed since startup */
  int tick;

  /** Flag for computing if we are the current localhost broadcaster. 
      0: Do not broadcast (we have seen someone 
      else with a lower ID from our host do it).
      1 - 2: Do not broadcast, but we have not recently seen
      anyone else with a lower ID do it.
      3: Do broadcast, we haven't seen anyone else broadcast from our host. 
  */
  char broadcastingCounter;

  /** Main storage for information about all known hosts, contains PeisHostInfo structures indexed by the PEIS ID. */
  struct PeisHashTable *hostInfoHT;

  /** Main storage for information about connection attempts to hosts, contains PeisConnectionMgrInfo
      structures indexed by the PEIS ID. */
  struct PeisHashTable *connectionMgrInfoHT;
} PeisKernel;

/** Internalized access to the ID variable. \todo  Should (in the future) be moved replaced everywhere with peiskernel.id instead */
extern int peisk_id;

/****************************************/
/********** GLOBAL VARIABLES ************/
/****************************************/

/** The singleton object used for (almost) all peiskernel operations */
extern PeisKernel peiskernel;                               
/** Provides the raw package for use for hooks. Should only be used by
    peiskernel private functions and not applications  */
extern PeisPackageHeader *peisk_lastPackage;                
/** Provides the raw connections that triggered a hook. Should only be used by
    peiskernel hook functions and not applications  */
extern int peisk_lastConnection;
/** If true generates a lot of debug information for received packages */
extern int peisk_debugPackages;                             
/** Print debug information for routes */
extern int peisk_debugRoutes;                               
/** Connection string used to separate disparate P2P networks */
extern char *peisk_networkString;                           
/** Safeguard against peiskernels with incompatible protocolls. */
extern int peisk_protocollVersion;                          
/** Internal variable showing if we are running or not */
extern int peisk_int_isRunning;
/** Internal variable for base metric cost to communicate over 
    network (non-loopback) connections. */
extern int peisk_netMetricCost;

/** Global variable storing latest traceroute response */
extern PeisTracePackage peisk_traceResponse;

/** Used internally in the peisk_step and child functions to cache the current PEIS clock. */
extern double peisk_timeNow;

/** Global variable controlling if connections should be managed
    automatically (default true). If true connections will
    automatically be created if too few connections are currently
    active or closed if too many are active.  */
extern int peisk_useConnectionManager;

/***********************************************************************/
/*           Private  functions used by the peis kernel core           */
/***********************************************************************/

/** Returns a list of hooks for given port (if any). */
PeisHookList *peisk_lookupHook(int port);

void peisk_trapCtrlC(int);                          /**< Catches Ctrl-C signals to shutdown cleanly */
void peisk_trapPipe(int);                           /**< Catches Pipe signals to close sockets properly */
void peisk_closeConnection(int);                    /**< Closes the given connection */
int peisk_lookupRoutingInfo(int id);                /**< Looks for routing information for given destination, returns index or -1 if not found */
int peisk_allocateRoutingInfo(int id);              /**< Allocates and inserts a routing info into hash table */

int peisk_connection_sendPackage(int id,PeisPackageHeader *package,int datalen,void *data,int specialFlags); /**< Send a package+data on given connection. Returns zero on success. */
int peisk_sendBroadcastPackage(PeisPackageHeader *header,int len,void *data,int flags); /** Sends a package on (all/a subset of all) connections */

/** Waits until it is possible to read something from given
    filedescriptor or timeout is reached. Returns zero on timeout,
    nonzero otherwise */
int peisk_waitForRead(int fd,double timeout);

/** Waits until it is possible to read something from given
    filedescriptor or timeout is reached. Returns zero on timeout,
    nonzero otherwise */
int peisk_waitForWrite(int fd,double timeout);

PeisConnection *peisk_lookupConnection(int id);       /**< Looks up a connection id and gives the PeisConnection structure. */
void peisk_connection_processOutgoing(PeisConnection *connection);    /**< Processes a queue and attempts to send any pending data. Returns nonzero if messages successfully sent. */
/** Receives, processes and routes incomming messages from a queue. Returns nonzero if new messages where received */
int peisk_connection_processIncomming(PeisConnection *connection);

/** \brief Register a new periodic function.

    This function will be called with the given period length and is
    passed the usersupplied data on each invokation.
*/
void peisk_registerPeriodicWithName(double period,void *data,PeisPeriodic *hook,char *name);
/** Macro wrapper automatically passing the function name to the periodic registration */
#define peisk_registerPeriodic(period,data,hook) peisk_registerPeriodicWithName(period,data,hook,#hook)


/** Loop detection to see if package is repeated. Return non zero if it is repeated */
int peisk_isRepeated(int pkgId);

/** Adds package id to memory for loop detection. */
void peisk_markAsRepeated(int pkgId);


/** Determines the amount of printouts done by the peiskernel. Logical or of
    the different printout flags PEISK_PRINT_* */
extern int peisk_printLevel;

#define PEISK_PRINT_STATUS        1          /**< Print out basic status information */
#define PEISK_PRINT_CONNECTIONS   2          /**< Print out basic connections managment */
#define PEISK_PRINT_PACKAGE_ERR   4          /**< Print out errors and other event related to packages. Can be noisy. */
#define PEISK_PRINT_TUPLE_ERR     8          /**< Print out errors and other events related to tuples. Can be noisy. */
#define PEISK_PRINT_DEBUG_STEPS   16         /**< Print out lots of debugging steps */
#define PEISK_PRINT_WARNINGS      32         /**< Print out some misc warnings that no one but Mathias cares about. */

/*********************************************************************************/
/*                                                                               */
/*        Private variables, types and functions used by peis services           */
/*                                                                               */
/*********************************************************************************/

#define PEISK_PACKAGE_REQUEST_ACK   (1<<0)    /**< Requests receiver to send back an ACK package upon receit copying the ackID */
#define PEISK_PACKAGE_RELIABLE      (1<<0)    /**< Requests a reliable delivery method. Currently a synonym for REQUEST_ACK above. */
#define PEISK_PACKAGE_IS_ACK        (1<<1)    /**< Signals that this package is an ACK package. The remaing data is discarded */
#define PEISK_PACKAGE_BULK          (1<<2)    /**< Package has lower priority */
#define PEISK_PACKAGE_HIPRI         (1<<3)    /**< Package has the highest priority */

/** Broadcasted package has lower priority */
#define PEISK_BPACKAGE_BULK         (1<<2)    
/** Broadcasted package has the highest priority */
#define PEISK_BPACKAGE_HIPRI        (1<<3)    
/** Spoof that a broadcasted package is one hop older than it is */
#define PEISK_BPACKAGE_SPOOF_HOPS     (1<<4)    


/* Asks a connection to force letting through all broadcasts in both
   direction instead of relying on stochastic algorithm. */
#define PEISK_CONNECT_FLAG_FORCE_BCAST  (1<<0)

/* Asks a connection attempt to be forced due to bandwidth even if the receiving PEIS have too many connections */
#define PEISK_CONNECT_FLAG_FORCED_BW (1<<1)

/* Asks a connection attempt to be forced due to clusters even if the receiving PEIS have too many connections */
#define PEISK_CONNECT_FLAG_FORCED_CL (1<<2)


/** Time in seconds of inactivity before a known host is removed / node is deleted from the topology */
#define PEISK_ROUTE_TIMEOUT             15.0
/** Timeout for old routes measured in number of sequence numbers since last use */
#define PEISK_ROUTE_TIMEOUT_SEQ          3 /* was 2 */
/** How often routing information should be broadcasted. Must be less than \ref PEISK_ROUTE_TIMEOUT */
#define PEISK_ROUTE_BROADCAST_PERIOD    10.0
/** Interval for broadcasting our precence on local (ethernet) network. Must be less than \ref PEISK_ROUTE_TIMEOUT */
#define PEISK_INET_BROADCAST_PERIOD      1.0
/** Minumum number of connections to maintain */
#define PEISK_MIN_AUTO_CONNECTIONS        3
/** Prefered maximum number of connections to maintain. Total bandwidth used
    scales (approximatly) linearly with this number. We can have more
    connections (up to PEISK_MAX_FORCED_AUTO_CONNECTIONS) if they all
    are forced due to high bandwidth. 
*/
#define PEISK_MAX_AUTO_CONNECTIONS        11
/** This is the absolute number of maximum connections, even when they are all forced connections. */
#define PEISK_MAX_FORCED_AUTO_CONNECTIONS 20

/** Limit for when hosts/connections should always keep a forced connection (if possible) */
#define PEISK_FORCE_LINK_BPS 2000

/** Number of connections that can be broadcasted on */
#define PEISK_BROADCAST_CONNECTIONS       4
/** Period in which number and QoS of connections are checked */
#define PEISK_CONNECTION_MANAGER_PERIOD  3.0
/** Period in how often we attempt to connect separate clusters of this network together if
    there is missing links making them disconnected. */
#define PEISK_CONNECT_CLUSTER_PERIOD     2.0
/** Period how often we broadcast timing information */
#define PEISK_TIMESYNC_PERIOD            10.0

/** Period how often we publish kernel information */
#define PEISK_KERNELINFO_PERIOD          5.0


/** Timeout for recieving new packages after which connections are treated as dead and is closed */
#define PEISK_CONNECTION_TIMEOUT         30.0 /* MB - changed from 5.0 to 30.0 when changing broadcast behaviour. */

/** Interval for managing speed control on connections */
#define PEISK_CONNECTION_CONTROL_PERIOD     0.1
/** How often allowable speed is recomputed */
#define PEISK_CONNECTION_CONTROL_PERIOD_2   2.0
/** Smallest/initial UDP speed setting possible, counted in number of bytes per second. */
#define PEISK_MIN_CONTROL_SPEED          1000




/** Periodically broadcasted to generate routing information. 
    This package is sent only to the nearest neighbours where it is
    collected and merged with their existing routing tables. This
    package is followed by a set of {int destination,int sequenceNumber,char hops}
    structs (packed to 9 bytes) which contain the actual routing
    information. Maximum number of entires is PEISK_ROUTING_PER_PAGE per page. */
typedef struct PeisRoutingPackage {
  /** Which page of routing information this is */
  short page;
  /** How many pages there are */
  short npages;
  /** How many entries on this page */
  short entries;
  unsigned char padding[2];
  /** Sequence number so we know we have received all parts of package */
  int sequenceNumber;
} PeisRoutingPackage;

/** Package used when answering a query for host information. */
typedef struct PeisHostInfoPackage {
  PeisHostInfo hostInfo;
  /** If true, then the reply comes from a cache, thus cannot be used
      to determine the immediate neighbour of a connection. */      
  char isCached;
  unsigned char padding[7];
} PeisHostInfoPackage;


/** Hashes the given string with a simple hash function. */
int peisk_hashString(char *string);
/** Basic package sending function, broadcast over whole network at
    given port number. Returns zero on success. */
int peisk_broadcast(int port,int len,void *data); 
/** Special version of broadcasting method, specify special state
    using the PEISK_BPACKAGE_ flags. Returns zero on success. */
int peisk_broadcastSpecial(int port,int len,void *data,int flags); 
/** Basic package sending function, broadcast over whole network at
    given port number. Send from a specific sender id. Returns zero on
    success. */
int peisk_broadcastFrom(int from,int port,int len,void *data); 
/** Special version of broadcasting method, specify special state
    using the PEISK_BPACKAGE_ flags. Send from a specific sender
    id. Returns zero on success.  */ 
int peisk_broadcastSpecialFrom(int form,int port,int len,void *data,int flags); 
/** peis-to-peis communication by sending replies to earlier
    messages. Returns zero on success. */
int peisk_sendMessage(int port,int destination,int len,void *data,int flags);
/** Special form of sendMessage allowing spoofed source
    address. Returns zero on success. */
int peisk_sendMessageFrom(int from,int port,int destination,int len,void *data,int flags);

/** Gives the local port used to receive incomming
    connections. Returns -1 if no server port used. */
int peisk_localTCPServerPort();

/** The current PEIS network we are running on as requested by bootup
    parameters. DO NOT change it. Only to be used by peisinit */
extern char *peisk_networkString;


void peisk_registerDefaultServices();
void peisk_registerDefaultServices2();
int peisk_hook_routing(int port,int destination,int sender,int datalen,void *data);

/** Triggered by incomming hostinfo query packages. Generates a
    response hostinfo package back to sender. */
int peisk_hook_queryHostInfo(int port,int destination,int sender,int datalen,void *data);
/** Triggred by incomming  hostinfo packages. Adds to known hosts */
int peisk_hook_hostInfo(int port,int destination,int sender,int datalen,void *data);

/** Periodically broadcast routing packages to update all routing
     tables. We're using the "arg" field just to see if we where called
     as a normal periodic function or called manually. In the later
     case to not update route hops prematurely (would screw up
     timings) */
void peisk_periodic_routing(void *arg);

/** Message which are sent to a specific connection, not for public use */
int peisk_sendLinkPackage(int port,PeisConnection *connection,int len,void *data);

/** Used to convert a hostInfo structure from host to network byte
    order. */
void peisk_hton_hostInfo(PeisHostInfo *hostInfo);

/** Used to convert a hostInfo structure from network to host byte
    order. */
void peisk_ntoh_hostInfo(PeisHostInfo *hostInfo);

/** Recomputes current cluster ID and triggers a retransmission if neccessary */
void peisk_checkClusters();

/** Checks for occurrance of kernel.do-quit and performs a shutdown if
    given any value other than "","no" or "nil". */
void peisk_callback_kernel_quit(PeisTuple *tuple,void *arg);

/** Removes all information about a host from the ecology. flag must
    be one of PEISK_DEAD_MESSAGE, PEISK_DEAD_ROUTE or PEISK_REBORN */
void peisk_deleteHost(int id,int flag);

/** Creates an acknowledgement package and sends it back to the
    sender at a future time, marks this acknowledgment as prioerity */
void peisk_sendAcknowledgement(int destination,int ackID);
/** Creates a bulk acknowledgement package and sends it back to the sender. */
void peisk_sendBulkAcknowledgement(int destination,int ackID);

/** Hook intercepting acknowledgement packages and removing the
    corresponding packages from the pending queues. */
int peisk_hook_acknowledgments(int port,int destination,int sender,int datalen,void *data);

/** Periodic for sending acknowledgement packages or when enough have
    been collected. */
void peisk_sendAcknowledgementsNow();

/** Creates a tuple containing our routing table, used for debugging
    and visualization purposes. */
void peisk_updateRoutingTuple();

/** Recomputes the cost metric for a connection based on network type. */
void peisk_recomputeConnectionMetric(PeisConnection *connection);

/** Tries to estimate the cost metric for a connection to this host, returns the guestimated metric cost */
int peisk_estimateHostMetric(PeisHostInfo *hostInfo);


/** Performs one round of routing, depending on value of kind.
   0: Sends routing data to all connections, ages hosts, deletes old
   hosts, sends hosts queries etc.
   1: Send routing data only to given connection
   2: Send routing data to all connections, but do not age or re-query
   hosts. 
*/   
extern void peisk_do_routing(int kind,PeisConnection *connection);

/** Sends a hostInfo structure to the given destination */
void peisk_sendHostInfo(int destination,PeisHostInfo *);

/** Sends our hostInfo along the given link. */
void peisk_sendLinkHostInfo(PeisConnection *connection);

/** Broadcasts our hostInfo to most (all?) hosts on network */
void peisk_broadcastHostInfo();

/** Return the hostInfo structure for the given PEIS ID, if currently known. */
PeisHostInfo *peisk_lookupHostInfo(int id);

/** Return the connectionMgrInfo structure for the given PEIS ID, if any currenly exists. */
PeisConnectionMgrInfo *peisk_lookupConnectionMgrInfo(int id);

/** Inserts the pointer to the given hostInfo into the hostInfoHT. 
    Note, does _not_ create a unique copy (called must do that explicitly). */
void peisk_insertHostInfo(int id,PeisHostInfo *hostInfo);

/** Inserts the pointer to the given connMgrInfo into the connMgrInfoHT. 
    Note, does _not_ create a unique copy (called must do that explicitly). */
void peisk_insertConnectionMgrInfo(int id,PeisConnectionMgrInfo *connMgrInfo);


/** \ingroup LargeAcknowledgement
    Wrapper hook for handling acknowledgents/rejections of large
    packages. Works by creating a copy of the whole message (which can
    be large!) and a counter. Calls the user defined hook when the
    counter has reached zero and passed the original whole message and
    an emulated package header (with the whole length) to the user. */
void peisk_largePackageHook(int success,int datalen,PeisPackage *package,void *user);

/** Process hostInfo packages coming from multicasts or other
    non-P2P layer sources. This function is essential for
    letting components find each other bootstrapping the P2P network. */
void peisk_handleBroadcastedHostInfo(PeisHostInfo *newHostInfo);

/** \brief Print name and addresses of given hostinfo structure */
void peisk_printHostInfo(PeisHostInfo *hostInfo);

/** \brief Debug information for tracking number of incoming packages per port */
extern int peisk_in_packages[50];
/** \brief Debug information for tracking number of outgoing packages per port */
extern int peisk_out_packages[50];

/** Last connection on which a broadcasted package came.
    Prevents that
    connection from beeing used in sendBroadcastPackage function
    (presumably, to not send the same package back there again). */
extern PeisConnection *peisk_incommingBroadcastConnection;

/** Prints a timestamp for logging outputs to given stream. Used as a prefix by printouts */
void peisk_logTimeStamp(FILE*);

/** \}@ PeisKernel */
#endif
