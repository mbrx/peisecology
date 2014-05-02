/** \file tuples_private.h
    The private interface to tuples.h
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

/** \ingroup tuples \{@ */
/** \defgroup tuples_internal Tuplespace internals
    
    \brief This page describes the internal functions that implement the
    tuplespace. None of these functions, structures and API's should
    be of interest for pure users of the peiskernel.  

    The tuplespace is implemented on top of the \ref P2PLayer 
    "P2P layer" and the public interface can be found in the \ref
    tuples "Tuplespace layer" page. 
*/ 
/*\{@*/

#ifndef TUPLES_PRIVATE_H
#define TUPLES_PRIVATE_H

/******************************************************************************/
/*                                                                            */
/*     Private variables, types and functions used by peis services           */
/*                                                                            */
/******************************************************************************/

/** Interval for maintainence operations on local tuplespaces. See also peisk_periodic_tuples. */
#define PEISK_MANAGE_TUPLES_PERIOD       1.0

/** Interval for sending subscription messages. See also peisk_periodic_resendSubscriptions. */
#define PEISK_RESEND_SUBSCRIPTIONS_PERIOD 4.0

/** Maximum expiry time of (remote) subscriptions */
#define PEISK_MAX_SUBSCRIPTION_TIME     60.0

/** Interval for printing debugging information about tuples. See also
    peisk_periodic_debugInfoTuples. */
#define PEISK_DEBUG_TUPLES_PERIOD       2.0

/** Interval for checking for outdated tuples. Note we do not need to
    do this often if we let all getTuple functions etc. perform this
    filtering also on tuples that are returned. */
#define PEISK_EXPIRE_TUPLES_PERIOD       0.5

/** Interval for attempting to resend tuples that have failed. */
#define PEISK_SEND_PENDING_TUPLES_PERIOD 1.0

/** After how long time to give attempting to send a tuple. */
#define PEISK_PUSH_TUPLE_TIMEOUT  30.0

/** Default array size for storing target tuples in an iterator */
#define PEISK_RESULTSET_DEFAULT_MAX_TUPLES 32

/** Shows that a callback is for normal tuple changes */
#define PEISK_CALLBACK_CHANGED 1

/** Shows that a callback is for tuple deletion */
#define PEISK_CALLBACK_DELETED 2


/** The representation of tuples used when transmitting them over the network. */
typedef struct PeisNetworkTuple {
  
  /** The owner (controlling peis) of the data represented by this
      tuple, or -1 as wildcard */
  int owner;
  /** ID of Peis last writing to this tuple, or -1 as wildcard */
  int creator;

  /** Timestamp assigned when first written into owner's tuplespace or
      -1 as wildcard. */
  int ts_write[2];
  /** Timestamp as assigned by user or -1 as wildcard */
  int ts_user[2];
  /** Timestamp when tuple expires. Zero expires never, -1 as wildcard. */
  int ts_expire[2];

  /** Type of encoding of tuple data. Should be one of PEISK_ENCODING_{WILDCARD,PLAIN,BINARY} */
  int encoding;
  
  /** Length of the mimetype, data of it stored before the data of the tuple */
  unsigned char mimetypeLength;

  /** NULL if no data, otherwise any non null integer value */
  int data;

  /** Used length of data for this tuple if data is non null,
      otherwise -1 */
  int datalen;

  /*                            */
  /*   For internal use only    */
  /* should be "private" if C++ */
  /*                            */

  /** Unused "key" pointers when transmitted over the network. Can be deleted in the future when making a non backwards compatible upgrade. */
  int unused[7];
  /** Depth of key or -1 as wildcard. */
  int keyDepth;

  /** Size of allocated data buffer. Usually only used by the tuple
      space privatly. Should be zero (uses datalen size instead) or a
      value >= datalen */
  int alloclen;

  /** Memory for storing the (sub)keys belonging to this tuple. Only
      for internal use. */
  char keybuffer[PEISK_KEYLENGTH];
  /** True if tuple has not been read (locally) since last received
      write. If used in a prototype, then any nonzero value means that
      only new tuples will be accepted. (findTuple returns zero if not
      new). */
  uint32_t isNew;                     

  /** Sequence number incremented by owner when setting the tuple, internal use only. */
  uint32_t seqno;

  /** Sub-sequence number used to arrange the order of append messages */
  uint32_t appendSeqNo;
} PeisNetworkTuple;


/** Message sent when subscribing to a key. We do not need to include 
    owner here since it is implicit in the recipient of the
    subscription message. */
typedef struct PeisSubscribeMessage {
  /** If nonzero always resend any existing data for this tuple when
      subscription is received. */
  int forceResend;                  

  /** Prototype tuple to subscribe to, notes uses a slightly modified
      content of the tuple fields, the data for the prototype (if
      relevant) is appeneded to the end of the PeisSubscribeMessage
      structure. */
  PeisNetworkTuple prototype;
}  PeisSubscribeMessage;

/** Message sent when unsubscribing to a key. We do not need to include owner here since it is implicit in the recipient of the
    subscription message. */
typedef struct PeisUnsubscribeMessage {
  /** Prototype tuple to subscribe to, notes uses a slightly modified
      content of the tuple fields, the data for the prototype (if
      relevant) is appeneded to the end of the PeisSubscribeMessage
      structure. */  
  PeisNetworkTuple prototype;
}  PeisUnsubscribeMessage;

/** Message sent when pushing a tuple to other peiskernels.
    The actual data comes after this message header. 
    This message type is used both for setRemoteTuple and for propagating
    actual assigned tuple values. */
typedef struct PeisPushTupleMessage {
  PeisNetworkTuple tuple;
} PeisPushTupleMessage;

/** Message sent when pushing a tuple have been appended with new
    data. The tuple will contain the old data size and sequence
    number. The client must check that these corresponds to his cached
    values and perform the whole append in his local space with the
    given new data.  

    When used as a wildcard the actual tuple data comes first after this part, the difference data
    second. The length of the tuple data is implicit in the tuple, the
    length of the difference data is given in the message. 

    When used as a notification, the tuple data is not
    included. Instead the only the difference data is included
    directly after the message. 
*/
typedef struct PeisAppendTupleMessage {
  PeisNetworkTuple tuple;
  uint32_t difflen;
} PeisAppendTupleMessage;

/** Keeps record of a subscriber of data. 
    Used both for recording which tuples peis self is subscribed
    (local subscriptions) to and which tuples others are subscribed to
    (remote subscriptions). */
typedef struct PeisSubscriber {
  /** Used as primary key in the database, enumerating the subscribers
      */
  PeisSubscriberHandle handle;     
  /** Who is the subscriber, equal to peiskernel.id if it is
      ourselves. -1 if unused */
  int subscriber;                  
  /** Time that subscription will expire or -1.0 if forever */
  double expire;                   
  /** The tuple prototype subscribed to */
  PeisTuple *prototype;            


  /** True of this is a meta subscription */
  char isMeta; 
  unsigned char padding[3];
  /** The callback responsible for updating this meta subscription */
  PeisCallbackHandle metaCallback;
  /** Current value of meta subscription */
  PeisSubscriberHandle metaSubscription;
} PeisSubscriber;

/** Keeps record of callbacks registered to be invoked when tuples is
    written */
typedef struct PeisCallback {
  /** Used as primary key in the database, enumerating all callbacks */
  PeisCallbackHandle handle;
  /** The function to be called when tuple is changed or NULL if this
      callback is unused */
  PeisTupleCallback *fn;
  /** A user supplied pointer given to callback when invoked */
  void *userdata;      
  /** The tuple prototype interested in */    
  PeisTuple *prototype;
  /** Callback type. Must be one of PEISK_CALLBACK_CHANGED or
      PEISK_CALLBACK_DELETED */
  int type;
} PeisCallback;

/** Used to store tuples which are waiting a chance to be push'ed to
    some subscriber. This only happens when the outgoing connection
    buffers get filled up. */ 
typedef struct PeiskPendingTuple {
  int subscriber;     /**< Whom to send the tuple to */
  PeisTuple *tuple;   /**< The tuple to send */
  double timestamp;   /**< When the request was _first_ made */
} PeiskPendingTuple;


/** Datatype of the list to keep track of tuples with an explicit
    expiry date that should be (eventually) deleted */
typedef struct PeisExpireList {
  PeisTuple *tuple;
  struct PeisExpireList *next;
} PeisExpireList;

/*                                               */
/* State variables - affects next created tuple. */
/*                                               */

/** The ts_user to use for all comming tuples that are created with
    the wrapper functions. */
extern int ts_user[2];  

/*                                                                 */
/* Global variables - does not affect a direct user visible state. */
/*                                                                 */

extern int peisk_override_setTuple_restrictions;
extern int peisk_debugTuples;
extern int peisk_tuples_errno;
extern struct PeisHashTable *peisk_tuples_primaryHT;
extern struct PeisHashTable *peisk_callbacks_primaryHT;
extern PeisCallbackHandle peisk_nextCallbackHandle;
extern struct PeisHashTable *peisk_subscribers_primaryHT;
extern int peisk_nextSubscriberHandle;
extern struct PeisExpireList *peisk_expireList;
extern struct PeisExpireList *peisk_expireListFree;
extern struct PeisHashTable *peisk_hostGivenSubscriptionMessages;


/*                                    */
/* Prototypes - for internal use only */
/*                                    */


/** Generate the fully qualitifed name corresponding to given tuple.
    The fully qualified name is the owner of the tuple suffixed by the 
    ordinary name. Eg: "10.foo.boo" or "10.*" or even "10." depending
    on wildcards */ 
int peisk_getTupleFullyQualifiedName(PeisTuple *tuple,char *buffer,int buflen);

/** Clones and adds a concrete tuple to the local tuple space. Returns
    zero if successfull. */
int peisk_addToLocalSpace(PeisTuple *tuple);

/** Generates the special tuple "kernel.all-keys" listing all keys in
    the local tuplespace */ 
void peisk_generateAllKeysTuple();

/** Sends a message to all subscribers listening to abstract tuples
    matching this tuple. */
void peisk_alertSubscribers(PeisTuple *tuple);

/** Sends a message to all callbacks listening to abstract tuples
    matching this tuple. */ 
void peisk_alertCallbacks(PeisTuple *tuple);

/** For each tuple matching the given subscriber, sends a message with
    the latest value */
void peisk_alertSubscriber(PeisSubscriber *subscriber);


/** Sends a push-tuple message with given tuple to the given
    destination. Return non-zero on immediate failure, zero on maybe success */
int peisk_pushTuple(PeisTuple *tuple,int destination);

/** Sends a push-tuple message with given tuple to the given
    destination acting like it was sent from a specific sender */
void peisk_pushTupleFrom(int from,PeisTuple *tuple,int destination);

/** Initializes datastructures and registers all hooks needed for
    managing tuplespaces */
void peisk_tuples_initialize();      

/** Frees all data and closes databases */
void peisk_tuples_shutdown();        

int peisk_hook_subscribe(int port,int destination,int sender,int datalen,void *data);
int peisk_hook_unsubscribe(int port,int destination,int sender,int datalen,void *data);
int peisk_hook_pushTuple(int port,int destination,int sender,int datalen,void *data);
int peisk_hook_setTuple(int port,int destination,int sender,int datalen,void *data);

/** Updates the local copies of tuples and performs any neccessary notifications */
void peisk_appendLocalTuple(PeisTuple *proto,int difflen,const void *diff);
/** Sends a request to append data to all tuples matching the given prototype to all hosts matching the prototype */
void peisk_sendAppendTupleRequest(PeisTuple *proto,int difflen,const void *diff);

/** Is notified about appended tuples */
int peisk_hook_pushAppendedTuple(int port,int destination,int sender,int datalen,void *data);

/** Listens for requests to append data to a tuple */
int peisk_hook_setAppendTuple(int port,int destination,int sender,int datalen,void *data);

/** Generic routine for registering any kind of callbacks. */
PeisCallbackHandle peisk_registerAbstractTupleGenericCallback(PeisTuple *tuple,void *userdata,PeisTupleCallback *fn,int type);

/** Generic routine for registering any kind of callbacks. */
PeisCallbackHandle peisk_registerTupleGenericCallback(int owner,const char *key,void *userdata,PeisTupleCallback *fn,int type);

/** Performs maintenence operations on local tuplespace and
    subscription records. */
void peisk_periodic_tuples(void *data);                  

/** Resend subscription messages at a regular interval. */
void peisk_periodic_resendSubscriptions(void *data);

/** Generates debug info tuples every second. Usable to debug network
    topology. */
void peisk_periodic_debugInfoTuples(void *data);         

//PeisTuple *peisk_findTuple(char *key,int owner);       /**< Find a tuple from the local tuple space */

/** Frees a subscriber record */
void peisk_freeSubscriber(PeisSubscriber *);            

void peisk_generateAllNames();                            /**< Used to maintain the special all-names tuple */

/** Adds a tuple to the local store of tuples. Note that this mechanism/store is also
    used for storing local copies of tuples belonging to other peis components.   
    If the tuple belongs to us then this also takes care of propagating the tuple to any eventual subscribers  */
/*void peisk_addToLocalSpace(char *key,int owner,int creator,long int ts_write[2],long int ts_user[2],int datalen,void *data);*/

/** This adds a (tuple,subscriber) pair to the ring buffer of push messages that
    need to be resent at a later timepoint. */
void peisk_pendingTuples_add(int subscriber, PeisTuple *tuple);
void peisk_pendingTuples_remove(int subscriber, PeisTuple *tuple);

/** Returns a pointer to a temporary buffer, if size is larger than
    last size it will be reallocated to fit. */
char *peisk_getTupleBuffer(int size);

/** Adds a tuple to the given resultSet object, returns zero on success */
int peisk_resultSetAdd(PeisTupleResultSet *,PeisTuple *);

/** Sends a subscribe message for a subscription record, possibly
    forcing a resend of the data from the owner. */
void peisk_sendSubscribeMessage(PeisSubscriber*,int forceResend);

/** Sends a subscribe message for a subscription record, possibly
    forcing a resend of the data from the owner. */
void peisk_sendSubscribeMessageTo(PeisSubscriber*,int forceResend,int destination);

/** Sends a subscribe message for a subscription record, possibly
    forcing a resend of the data from the owner acting like it was
    sent from a specific sender */
void peisk_sendSubscribeMessageFrom(int from,PeisSubscriber*,int forceResend);


/** Raw unsubscription function which accepts argument for if the
    unsubscription should be propagated (forwarded to publishers) or not. */
int peisk_rawUnsubscribe(PeisSubscriberHandle handle,int propagate);

/** Adds a new subscriber to the subscribers hashtables. If already
    present then the expiry time is only updated to the new
    value. Returns the subscribers structure */
PeisSubscriber *peisk_insertSubscriber(PeisSubscriber*);

/** Converts from network to host byte order. */
void peisk_tuple_ntoh(PeisNetworkTuple *from,PeisTuple *to);

/** Converts from host to network byte order. */
void peisk_tuple_hton(PeisTuple *from,PeisNetworkTuple *to);

/** Looks up a callback handle to a callback structure */
PeisCallback *peisk_findCallbackHandle(PeisCallbackHandle);

/** Looks up  a subscriber handle to a subscriber structure */
PeisSubscriber *peisk_findSubscriberHandle(PeisSubscriberHandle);

/** Removes a tuple from the expire list */
void peisk_expireListRemove(PeisTuple *tuple);

/** Adds a tuple from the expire list at the proper place in the list */
void peisk_expireListAdd(PeisTuple *tuple);

/** Periodic function for deleting outdated nodes from the expiry list */
void peisk_periodic_expireTuples(void *data);


/** Setup reasonable default values for a fresh subscriber structure */
void peisk_initSubscriber(PeisSubscriber *subscriber);

/** Removes all tuples and subscriptions associated with the given host */
void peisk_deleteHostFromTuplespace(int id);

/** Clears the state of all our subscriptions going to this host. */
void peisk_deleteHostFromSentSubscriptions(int id);

/** Marks <tuple,destination> pair as failed and schedules a retransmission at a later timepoint. */
void peisk_insertFailedTuple(PeisTuple *tuple,int destination);

/** Goes through the list of all failed tuples and attempts to retransmit them */
void peisk_retransmitTuples();

/** Looks for a pre-existing mimetype string, or creates and saves a new copy of the given string */
char *peisk_cloneMimetype(const char *mimetype);

/** Used for implementing callbacks triggered by changes in the pointed to tuple of a meta tuple, or by changes in the meta tuple */
void peisk_metaSubscriptionCallback(PeisTuple *tuple,void *userdata);

/*\}@ TuplesPrivate */
/*\}@ Ingroup Tuples */

#endif

