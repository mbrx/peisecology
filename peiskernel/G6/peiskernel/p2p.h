/** \file p2p.h
   Defines the functions belonging to the P2P layer
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

#ifndef P2P_H
#define P2P_H

/** \ingroup peisk */
/** \defgroup P2PLayer The P2P layer
    \brief The middle layer of the peiskernel consists of a basic P2P
    overlay network which uses any forms of network links provided
    by the lowermost overlay link layer to create an ad-hoc P2P layer
    discovering and connecting all PEIS. 

    It provides these basic API services to other parts of the peis kernel.

    - routing of messages towards destintions multiple hops away

    - port/pin based communication. Any peis can send a message on a
      given port of any other named peis or broadcast on a given port to
      reach all available peis in the environment. The ports typically
      denote different forms of services which the peis provide to the
      ecology. Communication can be initiated with unknown peis by a
      broadcasted request and continued by sending messages back to the replyer.

    - long messages: splits up messages longer than linklayer limit
      (1Kb) into multiple packages sent individually towards
      destination

    - reliable messages: see \ref Acknowledgements for details how
      this is implemented. This allows for the automatic resending of
      messages until they have been acknowledged by the receiver, or
      until a timeout have been reached. In either case, a user
      defined hook can be triggered to deal with the success/failure.

    - port based hooks: calls user defined functions when a package
      belonging to a given port passes through this host. Allows the
      functions to intercept or ignore the packages. 

    - periodic functions: calls user defined functions with a given
      periodicity. Will call the function with _up to_ that
      frequency. In general, we do not call a function multiple times
      in order to "catch up" with it if our step function have been
      called too slowly. This allows for (1) gradual degradation when
      CPU usage is 100% and (2) to register periodics with a period
      time of 0.0 in order to be called on every step. 
*/
/** @{ */

/** Initialiazes datastructures and registers hooks and services
    needed by the P2P layer. */
void peisk_initP2PLayer();


/** Port number for service updating routing information */
#define PEISK_PORT_ROUTING           0
/** Port number for service requesting traceroute information */
#define PEISK_PORT_TRACE             1
/** Port number for service giving responses to traceroute requests */
#define PEISK_PORT_TRACE_REPLY       2
/** Port number for sending requests of a list of all neighbours from node */
#define PEISK_PORT_NEIGHBOURS        3
#define PEISK_PORT_NEIGHBOURS_REPLY  4                                    /**< Reply containing list of all neighbours from node */
#define PEISK_PORT_SUBSCRIBE         5                                    /**< Subscribtion to tuples */
#define PEISK_PORT_UNSUBSCRIBE       6                                    /**< Unsubscribtion from tuples */
#define PEISK_PORT_PUSH_TUPLE        7                                    /**< Propagate a produced value */
#define PEISK_PORT_UDP_SPEED         8                                    /**< Info needed for congestion control */
#define PEISK_PORT_DEAD_HOST         9                                    /**< Signals that a host has disappeared from the network */
#define PEISK_PORT_SET_REMOTE_TUPLE 10                                    /**< Sets a remote tuple */
/** Broadcasted timing information on whole ecology to sync global time */
#define PEISK_PORT_TIMESYNC         11
/** Reqeuest host information */
#define PEISK_PORT_QUERY_HOST       12
/** Response to host information */
#define PEISK_PORT_HOSTINFO         13
/** Handle acknowledgements of important packages */
#define PEISK_PORT_ACKNOWLEDGEMENTS 14

/** Pushes a request to append a tuple */
#define PEISK_PORT_SET_APPEND_TUPLE 15

/** Receives notification of appended tuples */
#define PEISK_PORT_PUSH_APPENDED_TUPLE 16

/** If we receive packages with a higher port number we know they are wrong */
#define PEISK_HIGHEST_PORT_NUMBER   20

/** The number of possible ports that can be used */
#define PEISK_NPORTS                256

/** Maximum number of connections that can be maintained by the peiskernel */
#define PEISK_MAX_CONNECTIONS        64

/** Highest priority queue, used for control flow packages */
#define PEISK_QUEUE_HIGHPRI           0                                   
/** Special queue for pending messages */
#define PEISK_QUEUE_PENDING           1                                   
/** Default priority for all small messages */
#define PEISK_QUEUE_NORMALPRI         2                                   
/** Default priority for large messages */
#define PEISK_QUEUE_BULK              3                                   
/** Total number of outgoing queues per connection */
#define PEISK_NQUEUES                 4                                   

/** \brief Size of local store of previously seen packages. 
    Help eliminate loops in broadcasted or incorrectly routed packages. */
#define PEISK_LOOPINFO_SIZE         4096

#define PEISK_LOOPINFO_HASH_SIZE    256

/** \brief Maxmimum number of periodic functions that can be registered */
#define PEISK_MAX_PERIODICS         64

/** \brief Size of ringbuffer for storing meta data for incomming long messages.  */
#define PEISK_MAX_LONG_MESSAGES     512 

/** \brief Timeout after the latest package received for a long message. After the timeout the message is discarded. */
#define PEISK_TIMEOUT_LONG_MESSAGE  10.0

/**  \brief How long to wait between tries when sending a message with guaranteed delivery. 
    Accumulates as 1+2+3+4... times constant. */
#define PEISK_PENDING_RETRY_TIME    0.4 /* MB - was 0.5 */

/**  \brief How many tries to send a package we can maximum do before giving up */
#define PEISK_PENDING_MAX_RETRIES   6

/**  \brief Maxmimum length of out queue in connections counted as packages. */
#define PEISK_MAX_QUEUE_SIZE      1024

/**  \brief Maximum number of packages we can allocate (places upper limit on worst case scenario for memory use) */
#define PEISK_MAX_ALLOCATED_PACKAGES 1024

/**  \brief Special flag for sending packages that are routed, will not add any ack requests */
#define PEISK_SEND_ROUTED          1

/**  \brief Upper limit of how many pages of routing data that can be sent. This
    limits the total number of hosts to be max 32*70 = 2240 hosts */
#define PEISK_MAX_ROUTING_PAGES      32
/**  \brief Number of entries (hosts) per routing page */
#define PEISK_ROUTING_PER_PAGE       70
/**  \brief How many bytes of routing data there is per entry */
#define PEISK_ROUTING_BYTES_PER_ENTRY  14
/**  \brief How large a routing package can maximum be */
#define PEISK_ROUTING_PAGE_SIZE     (PEISK_ROUTING_PER_PAGE*PEISK_ROUTING_BYTES_PER_ENTRY+sizeof(PeisRoutingPackage))

/**  \brief  After what "metric cost" to giveup finding a route to a host. 
    Enforces an upper limit to metric costs allowed on the network 
    and acts as a hint what costs can be given links. */
#define PEISK_METRIC_GIVEUP         10

#define PEISK_ROUTING_SIZE         1024
#define PEISK_ROUTING_HASH_SIZE     256

/**  \brief Flag for PeisHostInfo structure showing that it is contested, we have seen him with different magic IDs before */
#define PEISK_HOSTINFO_CONTESTED  1

/**  \brief  Basic host information providing fullname, PEIS address, reachable
    lowlevel addresses etc. This structure is always stored in HOST
    byte order. */
typedef struct PeisHostInfo {
  /** Address of this kernel on the network. -1 if not a valid/unused
      hostinfo */
  int32 id;  

  /** Magic value used to auto-detect some errors with duplicated PEIS id's */
  int32 magic;

  /** Name of host computer */  
  char hostname[PEISK_MAX_HOSTINFO_NAME_LEN];                      
  /** A human readable fully qualified name describing this PEIS. Must
      be a null terminated string */
  char fullname[PEISK_MAX_HOSTINFO_NAME_LEN];                       

  /** Number of lowlevel addresses of all network interfaces */
  unsigned char nLowlevelAddresses;                                 
  unsigned char padding[7];

  /** The lowlevel addresses of all network interfaces */
  PeisLowlevelAddress lowAddr[PEISK_MAX_LOWLEVEL_ADDRESSES];        
  /** Timepoint host was last seen on any linklayer interface or on P2P network */
  double lastSeen;         

  /** Id number of network cluster this host belong to. 
      This is the lowest peis-id which can be routed to */
  int32 networkCluster;  
  /** Special flags used internally by peiskernel, currently unused! */
  int32 flags; 
  /*unsigned char padding2[4];*/
} PeisHostInfo;

typedef struct PeisPackageRoutingInfo {
  /** ID number of PEIS or -1 if unused */
  int id;                           
  /** Magic number of PEIS */
  int magic;                        
  /** Estimated number of hops to destination */
  int hops;                         
  /** Sequence number of last updated package */
  int sequenceNo;                   
  /** Connection id (or equivalent) package was received from. */
  int source;                       
  /** Next pointer in hash table for routing info or -1 if unused. */
  int hashNext;
  /** For easy deletion of route from hashtable. */
  int *hashPrev;
} PeisPackageRoutingInfo;

/** Information contained in each entry of the routing tables */
typedef struct PeisRoutingInfo {
  /** ID number of PEIS */
  int id;
  /** Magic number of PEIS */
  int magic;
  /** Counts up when host was last seen */
  int sequenceNumber;
  /** Number of hops to destination. Values about 250 has a special
      meaning;
      250: route is newly lost.
      251: route lost one time period ago.
      252: route lost two time periods ago.
      253: route lost and reported as dead */
  unsigned char hops;  
  /** Time before we will resend a hostInfo query if this host is not
      known */
  unsigned char timeToQuery;
  unsigned char padding[2];
  /** Outgoing connection to use */
  struct PeisConnection *connection;
} PeisRoutingInfo;

/** Structure used for remembering previously seen packages, this allows for a simple form of loop detection. */
typedef struct PeisLoopInfo {
  /** Id of package */
  int id;
  /** Next pointer in hash table or -1 if unused. */
  int hashNext;                     
  /** For easy deletion from hashtable */
  int *hashPrev;                    
} PeisLoopInfo;

/** Stores information about all registered periodic functions */
typedef struct PeisPeriodicInfo {
  double periodicity;                     /**< Delay in seconds between invokations or -1.0 if unused */
  double last;                            /**< Last invokation of function */
  void *data;
  PeisPeriodic *hook;
  char *name;
} PeisPeriodicInfo;

/** The different types of routing of packages available in the P2P network */
typedef enum { ePeisLinkPackage=0, ePeisBroadcastPackage, ePeisDirectPackage } PeisPackageType;

/** The different types of ackHookFailure types (see peiskernel_private.h) */
enum { eAckHookFailureNone=0, eAckHookFailureRED, eAckHookFailureDeadConnection, eAckHookFailureTooManyPackages, eAckHookFailureTooManyRetries };

/** Describes the header used in all linklevel connections.
    IntegerCertain fields of this structure should always occur
    in network byte order and there care must be taken whenever
    reading/writing to this structure.
    Note that you cannot change the size of any of these integer
    fields without updating _all_ occurances of ntohs/l for that
    specific field. */
typedef struct PeisPackageHeader {
  /** A special sync character transmitted first in all packages. Special byte-order. */
  int32 sync;

  /** Pseudo-unique id-number of package. Network byte-order. */
  int32 id;

  /** Counter used to monitor link quality, increments for each
      sent package (only implemented for UDP now). NETWORK BYTE ORDER.  */
  int32 linkCnt;

  /** Used when sending ACK's and when resending data. For normal
      packages should equal id fields, otherwise must be distinct
      from id field. Network byte-order. */
  int32 ackID;

  /** Logical OR of PEIS_PACKAGE_* flags. Network BYTE ORDER. */
  short flags;
  char type;                      /**< Routing type of package. Single character only. */
  char hops;                      /**< Number of hops package has travelled. */
  int32 source;                     /**< id of source, automatically updates routing tables when used with broadcast. NETWORK BYTE ORDER. */
  int32 destination;                /**< Target for packages. NETWORK BYTE ORDER */
  short port;                     /**< Destination port of package, note: not related to IP ports. NETWORK BYTE ORDER. */
  short datalen;                  /**< Length in bytes of this package. NETWORK BYTE ORDER. */

  short seqlen;                   /**< Zero if normal package, otherwise number of packages that are part of a long message. NETWORK BYTE ORDER */
  short seqid;                    /**< Pseudo-unique id-number of long messages. Network byte order */
  short seqnum;                   /**< Package number in sequence of a long message. Network byte order */
  unsigned char padding[2];
} PeisPackageHeader;

/** A full package as transmitted in the P2P network filled with maximum data */
typedef struct PeisPackage {
  PeisPackageHeader header;
  char data[PEISK_MAX_PACKAGE_SIZE];
} PeisPackage;

/** Private callback hook triggered when "reliable" packages have
    either been acknowledged, or failed permanently. Whole package
    (headers+data) is given to allow more parameters to be
    seen. Datalen gives the size of the datapart of the package, in
    case of large packages package->data contains the whole data. */
typedef void PeisAcknowledgementHook(int success,int dataLen,PeisPackage *package,void *user);

/** Maximum number of acknowledgementHooks that can be attached to
    packages at the same time. */
#define PEISK_MAX_ACKHOOKS       3

/** Maximum number of acknowledgement packages to queue in each
    connection */
#define PEISK_MAX_ACK_PACKAGES 100

/** Macro for running code with one more hooks pushed onto the hook
    stack. Sending one or more packages with given acknowledgement hook applied to all packages. 

    \param hook is a function of type void PeisAcknowledgementHook(int success,int datalen,PeisPackage *pkg,void *user)
    \param data is the userdata fed to callback
    \param code is the codeblock around which this hook applies
 */
#define with_ack_hook(hook,data,code) { \
    PEISK_ASSERT(peiskernel.nAckHooks < PEISK_MAX_ACKHOOKS, ("Too many stacked acknowledgement hooks\n")); \
    peiskernel.withAckHook[peiskernel.nAckHooks] = hook; \
    peiskernel.withAckHookData[peiskernel.nAckHooks] = data; \
    peiskernel.nAckHooks++; \
    {code}; \
    peiskernel.nAckHooks--;			\
}

/** Used for remembering acknowledgements to be sent back to the
    sender of incomming messages. */
typedef struct PeisPendingAck {
  int destination, ackID, priority;
} PeisPendingAck;

/** Datastructure for remembering all pending acknowledgements for a large package */
typedef struct PeisLargePackageHookData {
  int counter, success, datalen, nHooks;
  PeisAcknowledgementHook *hooks[PEISK_MAX_ACKHOOKS];
  void *data, *users[PEISK_MAX_ACKHOOKS];
} PeisLargePackageHookData;

/** \ingroup P2PLayer @{ */
/** \brief Queued package in the P2P layer.

    Used to keep track of packages waiting to be sent on a connection,
    waiting for an ACK message or in the heap of free packages to be
    used for this. */
typedef struct PeisQueuedPackage {
  double t0;                       /**< Timepoint package was added to outgoing queue or when an ACK request was last sent */
  unsigned char retries;           /**< How many times package has been sent (only used if it's a request-ack package) */
  unsigned char padding[3];
  PeisPackage package;             /**< The actual data for each package */
  struct PeisQueuedPackage *next;  /**< Point to next package in queue
				      */

  /** Hooks to call when package is acknowledged, or after final retry
      if failed. */
  PeisAcknowledgementHook *hook[PEISK_MAX_ACKHOOKS];
  /** Data to be given to the acknowledgement hook upon successfull or
      failed delivery. */
  void *hookData[PEISK_MAX_ACKHOOKS];
  /** Number of hooks in the stack of hooks to be called */
  int nHooks;
} PeisQueuedPackage;



/** Information about _active_ connections to other nodes */
typedef struct PeisConnection {
  /** Unique id number of connection or -1 if unused */
  int id;
  /** Gives the value of the connection as computed by the connection manager */
  int value;
  /** Timestamp for when packages where last received on this connection */
  double timestamp;
  /** Timepoint when connection was first established */
  double createdTime;
  /** Total number of incomming bytes */
  int totalIncomming;
  /** Total number of outgoing bytes */
  int totalOutgoing;
  /** Total incomming traffic in bytes since last kernelInfo periodic */
  int incommingTraffic;
  /** Total outgoing traffic in bytes since last kernelInfo periodic */
  int outgoingTraffic;
  /** Incoming+outgoing traffic in bytes that was of a non-meta nature along this 
      connection since the last connectionManger period. */
  int lastUsefullTraffic;
  /** Sliding window average of incoming+outgoing traffic per second along this connection. */
  int usefullTraffic;

  /** Estimated packet loss */
  double estimatedPacketLoss;
  /** Counter for ID numbers on outgoing packages, used for congestion
      control and estimating package losses in link layer. */
  int outgoingIdCnt;
  /** Highest seen ID+1 so far */
  int incomingIdHi;
  /** Highest ID from last congestion count */
  int incomingIdLo;
  /** Total number of incoming packages since last congestion check */
  int incomingIdSuccess;
  /** Maximum outgoing speed counted in bytes per second. Note: each
      package is counted as having an additional protocoll overhead of
      X bytes. */
  double maxOutgoing;
  /** How many bytes have been sent out this time interval. Modified
      periodically according to speedlimit. */
  double outgoing;
  /** Is this connection currently speed limited. */
  char isThrottled;

  /** Force broadcasts to always be sent on this connection */
  char forceBroadcasts;

  /** Current metric cost for using this link */
  char metricCost;

  unsigned char padding2[5];

  /** Routing table last received from neighbour */
  PeisHashTable *routingTable;
  unsigned char padding3[4];


  /** Stores incommming routingTable packages until they have been
      assembled correctly. */
  unsigned char *routingPages[PEISK_MAX_ROUTING_PAGES];

  PeisHostInfo neighbour;                /**< Info about neighbour */
  PeisConnectionType type;               /**< What type of connection this is */
  /** Contains linklayer connection type specific data inside a PeisConnection structure */
  union PeisConnectionInternal {
    /** Internal information for connections of type TCP */
    struct {
      /** Unix TCP/IP socket to remote node */
      int socket;                        
    } tcp;                               
    /** For connections of type UDP */
    struct {
      /** Marks connection status, pending if not yet fully connected. */
      PeisUDPStatus status;              
      int socket;
      struct sockaddr addr;
      socklen_t len;
    } udp;                               
    /** Internal information for connections of type Serial */
    struct {
      /** Unix file descriptor for opened serial port */
      int device;                        
    } serial;                            
    struct {
      /** The bluetooth L2CAP socket for this connection */
      int socket;
      /** Index to which adaptor is tied to this socket */
      struct PeisBluetoothAdaptor *adaptor;
    } bluetooth;
  } connection;                          

  /** If true, connection is not yet ready for reading/sending data */
  int isPending;

  /** Queues for outgoing packages, sorted after priority */
  PeisQueuedPackage *outgoingQueueFirst[PEISK_NQUEUES];
  /** Pointers to end of outgoing queues (for fast insert) */
  PeisQueuedPackage **outgoingQueueLast[PEISK_NQUEUES];
  /** Number of packages in different queues */
  int nQueuedPackages[PEISK_NQUEUES];
}  PeisConnection;


/** A linked list for containing a set of hooks to be triggered when a
    package arrives on a specific port. */
typedef struct PeisHookList {
  PeisHook *hook;
  char *name;
  struct PeisHookList *next;
} PeisHookList;

/** Assembled packages belong to a long message and calls intercept routines when the full message has been received. */
int peisk_assembleLongMessage(PeisPackageHeader *package,int datalen,void *data);
void peisk_periodic_clearAssemblyBuffers(void *data);  /**< Peridoric function to delete old (failed) long messages */

/** Compute the current fillrate for a given connection and priority setting. */
double peisk_connection_fillrate(PeisConnection *connection,int priority);

/** Stores incomming packages when receiving longer messages (over 1kb). */
typedef struct PeisAssemblyBuffer {
  short seqid;                    /**< Null or the pseudo-unique id-number of the messages. Host byte order */
  short seqlen;                   /**< Number of packages that are part of this message. */
  short allocated_seqlen;         /**< Size of memory allocated for buffers below. */
  unsigned char padding[2];
  char *received;                 /**< Pointer to boolean buffer for storing which packages have been received */
  void *data;                     /**< Pointer to buffer for the received and assembled data */
  double timeout;                 /**< Timeout for when this message can be discarded */
  int totalsize;                  /**< The total size for this message in bytes */
  unsigned char padding2[4];
} PeisAssemblyBuffer;

/** Information for storing connection attempts for different PEIS ID. Stored in the peiskernel.connectionMgrHT. */
typedef struct PeisConnectionMgrInfo {
  /** Total number of tries since last successfull connection */
  int nTries;               
  /** Timepoint when we can next try to connect to this host */
  double nextRetry;                 
  /** Do we currently have a direct connection to this host? */
  char directlyConnected;
  /** Incoming+outgoing traffic in bytes that was of a non-meta nature to/from this 
      host since the last connectionManger period. */
  int lastUsefullTraffic;
  /** Sliding window average of incoming+outgoing traffic per second along this connection. */
  int usefullTraffic;
  /** How many connections do we belive this host to have? */
  unsigned char nConnections;
} PeisConnectionMgrInfo;

/** A periodic function responsible for monitoring knownHosts for separate
    clusters of nodes not fully connected and merge them with our own by creating
    a single link to a host on that network. */
void peisk_periodic_connectCluster(void *data);

/** Opens and closes connections to maintain a constant number of open connections */
void peisk_periodic_connectionManager(void *data);

/** Attempts to estimate if given host is connectable using any of the underlying linklayer interfaces */
int peisk_hostIsConnectable(PeisHostInfo *hostInfo);

/** Initializes a connection structure to sane and default values */
void peisk_initConnection(PeisConnection *connection);
/** Intializes and returns the next usable connection structure. Returns NULL on error. */
PeisConnection *peisk_newConnection();
/** Free's this connection structure. */
void peisk_freeConnection(PeisConnection*);

/** Helper function for connection manager */
PeisConnection *peisk_connectToGivenHostInfo(PeisHostInfo *,int flags);
/** Helper function for connection manager */
PeisConnection *peisk_connectToGivenLowaddr(PeisHostInfo *,struct PeisConnectionMgrInfo *,PeisLowlevelAddress *laddr,int flags);
/** Provides sane initial values for a PeisConnectionMgrInfo structure */
void peisk_initConnectionMgrInfo(PeisConnectionMgrInfo *connMgrInfo);


/** Finializes an outgoing connection. Called by linklayer connection establishement functions when the connections are released from pendin state. */
void peisk_outgoingConnectFinished(PeisConnection *connection,int flags);
/** Finializes an incomming connection. Called by linklayer connection establishement functions when the connections are released from pendin state. */
void peisk_incommingConnectFinished(PeisConnection *connection,struct PeisConnectMessage *connectMessage);

/** Prints some basic debug information for a package. */
void peisk_printNetPackageInfo(struct PeisPackage *package);

/** Abort and free a pending connection, to be called by the lowermost
    link type specific functionalities when giving up on the connection. 
    This will clear up any other neccessary information for the P2P layers etc */
void peisk_abortConnect(PeisConnection *connection);


/** \ingroup P2PLayer */
/** \defgroup Routing Routing Tables
    The P2P layer keeps track of a routing table and propagates these
    along the neighbours links whenever the routing information
    changes. This information is stored in a local hash table.   

    For every other known component a hash table entry is created with
    routing information for this host. The routing information consists of
    the peis-id of the host, the magic number of the host, the sequence
    number of the last known routing information to this host, the
    estimated hops (metric cost) to this host as well as a timer used by
    the known hosts updating service. 
    
    Additionally, a simplified version of this hash table is stored for
    each connection, listing the estimated routing information to this
    host as if it had to go through this connection. 

    A periodic function is used to update the global routing table by
    inserting the local node into it's own routing table and monotonically
    incrementing the corresponding sequence number by one on each
    update. Furthermore, on each update a packed representation of the
    global routing table is computed containing the id,
    last-seen-sequence-number, magic-number, hops and
    estimated-connections for each host. The estimated-connections are
    computed from the connection manager structure for this host if
    available. These packed representations are sent along each connection
    (split into pages that fit within each package size requirement) to
    the direct link layer neighbours without any further propagation.  

    The bandwidth consumed by this module increase linearly by number
    of connections and in thresholds of number of known hosts divided
    by number of hosts that fits in the routing pages.  

    Bandwidth = O( nConnections * floor(hosts / 70) )


    An update to the global routing table is computed on each node
    whenever they receive routing information from the direct
    neighbours. The received routing information is decoded and compared
    to the current routing hash table for that connection and missing
    hosts are marked as outdated by setting their hops to 250. Hosts that
    are not outdated are updated in the connections routing table and a
    check is made to see if the global routing table is updated. This is
    done by comparing the values of seqno – hops for the routing table,
    trying to maximize this value in the global table. This can lead to
    the three cases: (1) no-change required, (2) use this connection
    instead of the current connection or (3) select another connection.  

    This function always maximize seqno – metric, which is
    monotonically increasing in all routing tables. 
    Implications:
      - When a short route to someone is lost, longer routes won't be considered until the seqno has increased.
      - Whenever a node is lost, metrics will increase and seqno stay
        same leading to the connections routing tables to not be updated
        (marking them as outdated) and leading eventual to deletion from
        the global routing table and triggering the internal hooks for
       dead hosts.  

    The magic numbers for hosts are used for detecting the situation
    when a given peis-id is used simultaneously by two different
    nodes, or to detect when a node have stopped and restarted. It is
    also used to prevent propagation of incorrect routing information
    to hosts that have been deleted and restarted.  
*/

/** \ingroup P2PLayer */
/** \defgroup Connections
    All connections between PEIS hosts are currently one-to-one links that uses the linklayer of the PEIS-kernel to send messages. 
    Connections can be established by the different connection management services of the linklayer and are either established using the peisk_connect call or by accepting an incoming connection using one of the peisk_acceptXYZConnection calls. (Eg. acceptTCPConnection or acceptBluetoothConnection).

    \msc
       p1s [label="PEIS-1 P2P Services"], 
       p1l [label="PEIS-1 LinkLayer"], 
       net [label="Network"],
       p2l [label="PEIS-2 LinkLayer"],
       p2s [label="PEIS-2 P2P Services"];

       p2l->net [label="bind/accept"];
       p1s => p1l [label="connect()"];
       p1l=>p1l [label="newConnection()"];
       p1l note p1l [label="is pending",textbgcolor="aqua"];
       p1l->net [label="TCP connect"];
       ... ;
       net->p1l [label="timeout 100ms"];
       p1l=>p1s [label="failed"];
       --- [label="or"];
       net->p1l [label="connected"];
       p1l->net [label="ConnectMessage"];
       p1l=>p1l [label="outgoingConnectFinished()"];
       p1l note p1s [label="NOT pending",textbgcolor="aqua"];
       p1s->net [label="LinkHostInfo"];
       ...;
       p2s=>p2l [label="acceptTCPConnection()"];
       p2l=>p2l [label="newConnection()"];
       p2l note p2l [label="is pending",textbgcolor="aqua"];
       net->p2l [label="ConnectMessage"];
       p2l=>p2l [label="verifyConnectMessage"];
       p2l=>p2s [label="incommingConnectFinished()"];
       p2l note p2s [label="mark host as directly connected",textbgcolor="silver"];
       p2l note p2s [label="NOT pending",textbgcolor="aqua"];
       p2s->net [label="LinkHostInfo"];
       ...;
       net->p2s [label="LinkHostInfo"];
       net->p2s [label="since PEIS-1::outgoingConnectFinished()"];
       p2s=>p2s [label="hook_hostInfo()"];
       p2s note p2s [label="update known hosts",textbgcolor="silver"];
       p2s note p2s [label="update connection hostinfo",textbgcolor="silver"];
       ...;
       net->p1s [label="LinkHostInfo"];
       net->p1s [label="since PEIS-2::outgoingConnectFinished()"];
       p1s note p1s [label="update known hosts",textbgcolor="silver"];
       p1s note p1s [label="update connection hostinfo",textbgcolor="silver"];
       p1s note p1s [label="mark host as directly connected",textbgcolor="silver"];

    \endmsc
    
 */

/** \ingroup P2PLayer */
/** \defgroup Acknowledgements Acknowledgement handling

    Packages sent through the P2P layer of the PeisKernel can be, and
    usually are, sent in "reliable mode". Meaning  
    that they after sending get put on a stack of packages to be
    acknowledged within a given timeframe or they are
    retransmitted. In order to not flood this stack, packages will
    only be retranmissted maximum of PEISK_PENDING_MAX_RETRIES
    times. The retransmission are with an increasing period of
    PEISK_PENDING_RETRY_TIME seconds for the first retry, twice that
    for the second retry, three times that for the third etc. The
    total time packages are kept on the stack are thus
    PEISK_PENDING_RETRY_TIME*(1+2+3+...+PEISK_PENDING_MAX_RETRIES) 

    After receiving an acknowledgement or giving up on a package, a
    peiskernel/expert user defined function may be called with a
    success status of 0 or 1. This is done by registering an
    acknowledgementHook when the package is sent, this hook is kept on
    the stack of packages and called at the relevant future time. 

    See also \ref LargeAcknowledgement
*/

/** \ingroup Acknowledgements */
/** \defgroup LargeAcknowledgement Acknowledgment of large packages

    We need to treat acknowledgement hooks special for large packages.
    Create a new hook which have a reference to the old hook, a
    counter and a copy of the whole data. When all parts have arrived
    or been rejected, then this hook calls the orignal hook, deletes
    it's copied data and itself. The original hook(s) is/are calĺed with
    success if all parts succeeded, and failure otherwise. 
*/

/** \ingroup P2PLayer */
/** \defgroup LongMessage Long messages

  This refers to messages which are longer than PEISK_MAX_PACKAGE_SIZE (ie. 1kb) of data. These messages
  are sent as multiple packages in the network and are assembled together by the receiver. Assembling and ordering
  the packages are done automatically by the peiskernel.

  Long messages cannot be broadcasted.

  Long messages cannot be intercepted.

  See also \ref LargeAcknowledgement "acknowledgement of long messages". 
*/

/** \ingroup P2PLayer */
/** \defgroup ConnectionManagement Connection mangement
    Goals: To keep the diameter of the P2P network small (increases
    chance of packages beeing delivered with fewer retries), to keep
    number of connection establishments as small as possible (avoid
    routing table changes, saves OS time) and to have direct
    connections to any hosts with whom we exchange much data (overall
    efficiency of network). Also attempt to keep the number of
    connections in a suitable range (not too few, not too many). 

    Method: minimise diameter, plus establish connections to hosts to
    whom we transmitt much information.

    Keep track of how much traffic (bytes/second) are _routed_ to
    different hosts. This includes all directed messages (not link
    level packages) and routed packages. For hosts that we route more
    than X bytes per second over a sliding window (for a constant X),
    force a connection if possible. 

    To minimise girth: Find the value V(c) of each connection c. The value of
    a connection is the maximum *increase* in metric distance to any
    host which are currently routed through that connection if that
    connection would be closed. This can be computed from the
    connections routing tables. The value of connections to whom we
    have a forced link (data per second > X) have an infinite value. 

    Next, for each known host which we haven't connected to recently: 
    find the value V(h) of establishing a connection to this host h, this
    value is defined as a random number (0-99) plus a constant (100)
    times the current metric to this host minus the guestimated metric
    cost along a direct connection to that host. Hosts that we have
    recently connect to have a value of  zero. This allows for a
    certain randomness in chosing which hosts to connect to, aliasing
    interference patterns from multiple components attempting to
    connect to the same hosts. 

    If we have fewer than the minimum wanted connections (eg. 3) then
    establish a new connection to the host h with the highest value
    V(h). 

    If we have a suitable number of connections (eg. < 7) then
    connect to the host h with the highest value V(h) if this value is
    higher than the lowest V(c) for any connection c. 

    If we have more than a suitable number connections (eg. 7) then
    close the connection with the lowest value (if it not infinite). 

    Idea for load balancing: Keep track of the two or three
    connections which have the lowest metric to any given hosts. Send
    some of the packages through the other metric connections
    (depending on relative metric costs), this might reduce the load
    of the routing hosts and may establish new direct links... 
    Hmm, could be dangerous if bandwidth along each connection becomes too
    low, so the connections get a low value...
 */

/** @} P2PLayer */

#endif
