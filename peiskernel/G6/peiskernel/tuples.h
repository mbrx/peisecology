/** \file tuples.h
	The public and private interface to tuples.h
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

#ifndef TUPLES_H
#define TUPLES_H

/** \ingroup peisk */
/*\{@*/
/** \defgroup tuples Tuplespace layer

    \brief This is the public interface to the tuplespace layer of the peiskernel. 

    The tuplespace is implemented on top of the \ref P2PLayer 
    "P2P layer" and implementation details can be found in the \ref
    tuples_internal "Tuplespace internals". 
    
    From the perspective of the tuplespace keys consists of three
    parts: (name, owner, data) where \em name is a string key for the
    tuple, \em owner is the address of a peis responsible for this
    tuple (see below) and data is the value of the tuple. The tuples
    are indexed by \em name and \em owner meaning that tuples with the
    same name but different owners are allowed to coexist while there
    can (ideally) only be one instance at a time of tuples with the
    same \em name, \em owner (but different data).
    
    
    There are (currently) two basic methods that tuple values are
    propagated.
    - The \em owner peis writes to the tuple and the updated value is
    propagated to all \em subscribed peis.
    - A peis writes to the tuple by sending a message to the \em owner
    peis which stores the last written value and propagates it to all
    \em subscribed peis. The message passing is handled transparantly
    be the peis kernel and is not visible to the application other than
   by a delay before the value is updated in the local tupelspace.
   
   The \em owner of a tuple can always access the latest value of that
   tuple by a direct memory access. In order for other peis to have
   access the latest value of a tuple they need to be subscribed to the
   tuple. Subscriptions are established by calling peisk_subscribe()
   which will make sure that subscription messages are reguarly sent to
   relevant parts of the network. Subscriptions come in two varieties:
   - Qualified subscribtion to (\em name, \em owner) tuples. These
   subscriptions are in essence a direct oneway communication from the
   owner of the tuple to the receiving peis which will get the latest
   value of all written tuple values.
   - Wildcard subscriptions (\em name, *) which registers a
   subscription with all peis on the network. All matching produced
   tuples will be propagated to this peis. 
   
   Corresponding to these two subscription mechanism tuples can also
   be accessed by a call to peisk_getTuple(). We have four cases:
   - A peis accessing a tuple with itself as \em owner. Returns the
   last value written to the tuple by itself or by some other peis
   writing to the tuple (and propagating the message to this owner). 
   - A peis accessing a tuple with another peis as \em owner. Returns
   the last value of the tuple that has been propagated from the owner
   to this peis. 
   - Accessing a tuple with a wildcard (-1) as \em owner. Returns a
   value for each owner that has propagated a value for this \em
   keyname to this peis.
   
   Note that the last two accesses requires active subscriptions in
   order to receive the \em correct value. 

   Everyone interested in data should subscribe to key(s)
   periodically. Everyone producing data should send a copy of it to
   everyone currently subscribed to it.
   
   \section Special tuples
   There exists a number of special tuples maintained automatically by
   the peiskernel to give information about the internal state of this
   peis. All of these special tuples have the prefix "peiskernel."

   - peiskernel.all-keys: The value of this tuple is managed by the peiskernel
   and is always maintained as a lisp-list of the names of all
   currently available tuples.
   - peiskernel.connections: Gives connection and traffic information
     about all currently established network connections
   - peiskernel.do-quit: if set to "yes" this forces the application
   to exist.
   - peiskernel.hostname: gives the hostname of this peis
   - peiskernel.name: gives the application name of this component
   - peiskernel.routingTable: gives routing information to tupleview,
   mainly for debugging
   - peiskernel.step-time: gives an indication of how often the kernel
   is executing, must be below 0.1 at all times. Preferably below
   0.01. 
   - peiskernel.subscribers: gives a list of all peis currently
   subscribed to any tuples in this peis ownership.
   - peiskernel.traffic: gives traffic statistics
   - peiskernel.user: gives the username (if appropriate) of the user
   running this component/application
   - peiskernel.version: shows the compile version of this kernel
*/ 
/*\{@*/


/** The maximum length of the key any tuple */
#define PEISK_KEYLENGTH 128             

/** Decides if tuple requests should filter away or keep old values. */
#define PEISK_FILTER_OLD           0
#define PEISK_KEEP_OLD             1
/** Makes tuple requests block until value is available. */
#define PEISK_NON_BLOCKING         0
#define PEISK_BLOCKING             2    

#define PEISK_ENCODING_WILDCARD   -1
#define PEISK_ENCODING_ASCII       0
#define PEISK_ENCODING_BINARY      1


/* These are the error codes that can be returned from the tuple functions */
/** Generic error covering all kinds of failures from the tuple functions*/
#define PEISK_TUPLE_ERROR           1
#define PEISK_TUPLE_BADKEY          2
#define PEISK_TUPLE_BUFFER_OVERFLOW 3
#define PEISK_TUPLE_IS_ABSTRACT     4
#define PEISK_TUPLE_OUT_OF_MEMORY   5
#define PEISK_TUPLE_HASHTABLE_ERROR 6
#define PEISK_TUPLE_BAD_HANDLE      7
#define PEISK_TUPLE_INVALID_INDEX   8
#define PEISK_TUPLE_BAD_ARGUMENT    9
#define PEISK_TUPLE_INVALID_META    10
#define PEISK_TUPLE_LAST_ERROR      10

/** Use this expire time in field 0 as a convention if the tuple
    should be deleted immediatly. Callback functions can trigger on
    this special value. */
#define PEISK_TUPLE_EXPIRE_NOW   1

/** \brief Basic datastructure for reading and manipulating tuples. 

    Represents a tuple in the local tuplespace. Also used for
    (partial) tuple as used when doing searches in the local and in
    the distributed tuple space. Wildcards on specific fields are
    specified using NULL values (for pointers) or -1 for integers. */
typedef struct PeisTuple {
  
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
  
  /** Pointer to MIMETYPE or NULL for wildcard */
  char *mimetype;

  /** Data of tuple or NULL if wildcard. */
  char *data;
  /** Used length of data for this tuple if data is non null,
      otherwise -1 */
  int datalen;

  /*                            */
  /*   For internal use only    */
  /* should be "private" if C++ */
  /*                            */

  /** Key of tuple split up into subparts and represented as
      C-strings. Eg. "foo.boo.moo" becomes the three strings "foo"
      "boo" and "moo", representing a tuple of key depth 3. Use NULL
      pointers as wildcards. 

      May not be modified directly by the user, use peisk_setTupleName
      function instead. */
  char *keys[7];
  /** Depth of key or -1 as wildcard. */
  int keyDepth;

  /** Size of allocated data buffer. Usually only used by the tuple
      space privatly. Should be zero (uses datalen size instead) or a
      value >= datalen */
  int alloclen;

  /** Memory for storing the (sub)keys belonging to this tuple. Only
      for internal use. */
  char keybuffer[PEISK_KEYLENGTH];

  /*                                    */
  /* Public when doing advanced queries */
  /*                                    */

  /** True if tuple has not been read (locally) since last received
      write. If used in a prototype, then any nonzero value means that
      only new tuples will be accepted. (findTuple returns zero if not
      new). */
  int isNew;                     

  /** Sequence number incremented by owner when setting the tuple */
  int seqno;
  /** Sub-sequence number used to arrange the order of append messages */
  int appendSeqNo;
} PeisTuple;

/** Function pointer datatype for registering callbacks */
typedef void PeisTupleCallback(PeisTuple *tuple,void *userdata);
typedef int PeisCallbackHandle;
typedef int PeisSubscriberHandle;

/** Set to true to generate additional debugging information about
    the tuple routines */
extern int peisk_debugTuples;
/** Updated by some functions instead of returning an error code */
extern int peisk_tuples_errno;

#ifdef PEISK_PRIVATE
/* Note, we hide the implementation of this for normal users
   to avoid the risk of someone accessing the data directly
   instead of through the resultSet functions. */
typedef struct PeisTupleResultSet {
  PeisTuple **tuples;
  int index, nTuples, maxTuples;
} PeisTupleResultSet;
#else
/** Used to iterate over tuples using the resultSet functions */
typedef void *PeisTupleResultSet;
#endif

/** \brief Gives the PEIS-ID of our currently running peiskernel */
extern int peisk_peisid();

/** Prints a tuple in human readable format to stdout. */
void peisk_printTuple(PeisTuple *);

/** Prints a tuple in human readable format to the given buffer. */
void peisk_snprintTuple(char *buf,int len,PeisTuple *);

/** Sets the user defined timestamp (ts_user) for all comming tuples
    that are created. This is for backwards compatability but also
    convinience when many tuples are created. */
void peisk_tsUser(int ts_user_sec,int ts_user_usec);

/** Simplifying wrapper for creating and setting a tuple in local
    tuplespace, propagating it to all subscribers. */
void peisk_setTuple(const char *key,int len,const void *data,const char *mimetype, int encoding);

/** A wrapper function for setting tuples whose value are simple
    C-string. Provides backwards compatability with kernel G3 and
    earlier. */
void peisk_setStringTuple(const char *key,const char *value);

/** Wrapper for creating and setting a tuple in a tuplespace belonging
    to some other peis. Propagation is done by the other peis if
    successfull. Provides backwards compatability with kernel G3 and
    earlier. */
void peisk_setRemoteTuple(int owner,const char *key,int len,const void *data,const char *mimetype, int encoding);

/** Wrapper for creating and setting a tuple in a tuplespace belonging
    to some other peis and blocking until acknowledged.     

    \return Returns zero if successfull.
    (note. failure may be delayed if modifying tuples inside other
    components, such failures are not reported here) */
int peisk_setRemoteTupleBlocking(int owner,const char *key,int len,const void *data,const char *mimetype, int encoding);

/** Wrapper for creating and setting a tuple in a tuplespace belonging
    to some other peis. Propagation is done by the other peis if
    successfull. */
void peisk_setRemoteStringTuple(int owner,const char *key,const char *value);

/** Wrapper for creating and setting a tuple in a tuplespace belonging
    to some other peis and blocks until set operation have been acknowledged.  

    \return Returns zero if successfull.
    (note. failure may be delayed if modifying tuples inside other
    components, such failures are not reported here) */
int peisk_setRemoteStringTupleBlocking(int owner,const char *key,const char *value);

/** Wrapper for getting a value from the local tuplespace for the
    fully qualitified given key and owner. 
    \param key The keyname of the tuple.
    \param owner The owner ID for the namespace in which tuple belong
    Returns nonzero if successfull (returns pointer to tuple
    structure, use with care!)
    \param flags Flags modifiying the behaviour of getTuple. Bitwise OR of
    the flags PEISK_KEEP_OLD, PEISK_FILTER_OLD, PEISK_BLOCKING and PEISK_NON_BLOCKING - respectively. 
*/
PeisTuple *peisk_getTuple(int owner, const char *key,int flags);

/** Clone a tuple (including the data field). The user of this
    function is responsible for freeing the memory eventually, see
    peisk_freeTuple. Updates peisk_tuples_errno if failed */
PeisTuple *peisk_cloneTuple(PeisTuple *tuple);

/** Free a tuple (including the data field). */
void peisk_freeTuple(PeisTuple *tuple);

/** Wrapper for subscribing to tuples with given key from given owner
    (or -1 for wildcard on owner). */
PeisSubscriberHandle peisk_subscribe(int owner,const char *key);

/** Subscribe using given abstract tuple as prototype. Returns handle
    or 0 (NULL) if failed. */
PeisSubscriberHandle peisk_subscribeByAbstract(PeisTuple *);

/** Unsubscribe to tuples. Returns zero on success, error number
    otherwise  */
int peisk_unsubscribe(PeisSubscriberHandle);

/** Forces a resend of subscription and reload all subscribed tuples. */
int peisk_reloadSubscription(PeisSubscriberHandle subscriber);

/** Tests if anyone is currently subscribed to tuples comparable to the
    given prototype. Ie. if the prototype is concrete then this is a
    test if any subscriber would be triggered by it. If the prototype
    is abstract, then tests if it partially overlaps any subscription.
    For instance, if we have a subscription for A.*.C and we test with
    prototype A.B.* then the result is _true_. 

    Returns nonzero if test is true. */
int peisk_hasSubscriberByAbstract(PeisTuple *prototype);

/** Wrapper to tests if anyone is currently subscribed to the given
    (full) key in our local tuplespace. Wrapper for backwards
    compatability. */
int peisk_hasSubscriber(const char *key);

/** Wrapper to add a callback function to tuples with given fully
    qualified key and owner; which is called when tuple is changed in
    the local tuplespace. Provides backwards compatability with kernel
    G3 and earlier. Returns callback handle if successfull, 0 (or
    NULL) if unsuccessfull.*/
PeisCallbackHandle peisk_registerTupleCallback(int owner,const char *key,void *userdata,PeisTupleCallback *fn);

/** Wrapper to add a callback function to tuples when they are
    deleted. Uses a given fully qualified key and owner.Returns
    callback handle if successfull, 0 (or NULL) if unsuccessfull.*/
PeisCallbackHandle peisk_registerTupleDeletedCallback(int owner,const char *key,void *userdata,PeisTupleCallback *fn);

/** Callback function using given abstract tuple as prototype. Returns
    callback handle if successfull, 0 (or NULL) if unsuccessfull. */
PeisCallbackHandle peisk_registerTupleCallbackByAbstract(PeisTuple *,void *userdata,PeisTupleCallback *fn);

/** Callback function using given abstract tuple as prototype. Invoked
    when matching tuples are deleted. Returns callback handle if
    successfull, 0 (or NULL) if unsuccessfull. */ 
PeisCallbackHandle peisk_registerTupleDeletedCallbackByAbstract(PeisTuple *,void *userdata,PeisTupleCallback *fn);

/** Remove a given callback. Returns zero on success, otherwise error code */
int peisk_unregisterTupleCallback(PeisCallbackHandle callback);

/** Initializes a tuple to reasonable default values for inserting
    into some tuplespace. Modify remaining fields before calling
    insertTuple or findTuple. Differs from peisk_initAbstractTuple in
    what the default values are.  

    \param tuple is a pointer to the tuple to be initialized
*/
void peisk_initTuple(PeisTuple *tuple);

/** Initializes a tuple to reasonable default values for searching in
    local tuplespace. Modify remaining fields before calling
    insertTuple or findTuple. Differs from peisk_initTuple in what the
    default values are. 

    \param tuple is a pointer to the abstract tuple to be initialized
*/
void peisk_initAbstractTuple(PeisTuple *tuple);

/** Decomposes a name into dot separated substrings and puts into the
    tuplefield. 
    
    \return zero on success. 
*/
int peisk_setTupleName(PeisTuple *tuple,const char *fullname);

/** Returns the key name of given tuple, eg. "foo.*.a" if the tuple
    has a three key parts where the second is a wildcard. Returns zero
    on success. */
int peisk_getTupleName(PeisTuple*,char *buffer,int buflen);

/** Test if a tuple is abstract (contain any wildcards). Returns nonzero if it is abstract. */
int peisk_tupleIsAbstract(PeisTuple *);

/** \brief Inserts a tuple into local or remote tuplespace. 

    The tuple is copied into private memory prior to insertion (thus it is safe to
    allocate tuples and their data on the stack). Tuples must be fully
    qualified and contain no wildcards. 

    \param tuple is a pointer to the tuple to be sent

    \return Returns zero if successfull.
    (note. failure may be delayed if modifying tuples inside other
    components, such failures are not reported here) */
int peisk_insertTuple(PeisTuple *tuple);

/** \brief Inserts a tuple into local or remote tuplespace and BLOCKS until completion. 

    The tuple is copied into private memory prior to insertion (thus it is safe to
    allocate tuples and their data on the stack). Tuples must be fully
    qualified and contain no wildcards. Returns zero if successfull.

    \param tuple is a pointer to the tuple to be sent

    \return Returns zero if successfull.
    (note. failure may be delayed if modifying tuples inside other
    components, such failures are not reported here) */
int peisk_insertTupleBlocking(PeisTuple *tuple);


/** \brief Appends the given data to the matching tuple in the tuple space. 
    The argument tuple is here an ABSTRACT TUPLE that identifies which tuple that should be appended. 
    You would normally have an empty data field in the given abstractTuple. If
    there is wildcards in the owner and or key parts of the abstract
    tuple, the data will be appended to each such tuple. If there is a
    concrete sequence number in the given tuple, then the append will
    only be performed if the two tuples have the same sequence number.

    Note that due to the semantics of appending tuples, attempting to perform an append on a tuple that does not yet exist will not result in any append operation on it.
*/
void peisk_appendTupleByAbstract(PeisTuple *abstractTuple,int difflen,const void *diff);
/** \brief Appends the given string to the matching tuple in the tuple space. 

    The argument tuple is here an ABSTRACT TUPLE that identifies which tuple that should be appended. 
    You would normally have an empty data field in the given abstractTuple. If
    there is wildcards in the owner and or key parts of the abstract
    tuple, the data will be appended to each such tuple. If there is a
    concrete sequence number in the given tuple, then the append will
    only be performed if the two tuples have the same sequence number.

    Note that due to the semantics of appending tuples, attempting to perform an append on a tuple that does not yet exist will not result in any append operation on it.
*/
void peisk_appendStringTupleByAbstract(PeisTuple *abstractTuple,const char *value);

/** \brief Appends the given data to the tuple(s) specified by the given owner and key. 
    
    This function disregard any checks to see that the appended tuple is as we expected since previous (ie. no sequence numbers can be specified). 
    Note that due to the semantics of appending tuples, attempting to perform an append on a tuple that does not yet exist will not result in any append operation on it.
 */
void peisk_appendTuple(int owner,const char *key,int difflen,const void *diff);
/** \brief Appends the given string to the tuple(s) specified by the given owner and key. 

    This function disregard any checks to see that the appended tuple is as we expected since previous (ie. no sequence numbers can be specified). 
    Note that due to the semantics of appending tuples, attempting to perform an append on a tuple that does not yet exist will not result in any append operation on it.
*/
void peisk_appendStringTuple(int owner,const char *key,const char *value);

/** \brief Looks up a tuple in the local tuplespace and locally cached
    tuples using the fully qualified owner and key only (no wildcards
    permitted). 
    
    It is an error to specify a tuple with withcards in the owner and/or key parts in this function call since this may result in multiple tuple results and require more advanced search functionalities.
    Returns pointer to tuple if found, otherwise NULL. */
PeisTuple *peisk_getTupleByAbstract(PeisTuple *);

/** Blocks until an owner for the given key (in any possible owners namespace) have been found. Returns the ID of the found owner. */
int peisk_findOwner(char *key);


/** Looks up a tuple in the local tuplespace and locally cached tuples. 
    May contain wildcards and may give multiple results. 
    A previously created resultSet must be passed as argument, freeing
    this resultSet (using the deleteResultSet command) is the responsibility 
    of the caller. The function precomputes all matching tuples and sets the given
    PeisTupleResultSet to point to first found tuple and returns
    nonzero if successfull. Note that the resultSet is not reset by a
    call to this function, thus multiple getTuplesByAbstract calls in a row
    gives a tuple resultSet with all the results concatenated. 
    Returns number of new tuples added to the resultSet.
*/
int peisk_getTuplesByAbstract(PeisTuple *,PeisTupleResultSet *);

/** Looks up a tuple in the local tuplespace and locally cached tuples. 
    May contain wildcards and may give multiple results. 
    A previously created resultSet must be passed as argument, freeing
    this resultSet (using the deleteResultSet command) is the responsibility 
    of the caller. The function precomputes all matching tuples and sets the given
    PeisTupleResultSet to point to first found tuple and returns
    nonzero if successfull. Note that the resultSet is not reset by a
    call to this function, thus multiple getTuplesByAbstract calls in a row
    gives a tuple resultSet with all the results concatenated. 
    Returns number of new tuples added to the resultSet.
*/
int peisk_getTuples(int owner, const char *key,PeisTupleResultSet *);

/** Creates a fresh resultSet that can be used for iterating over
    tuples, you must call peisk_resultSetNext before getting first
    value. */
PeisTupleResultSet *peisk_createResultSet();

/** Resets the resultSet to empty. Does not modify the cursor pointer. */
void peisk_resultSetReset(PeisTupleResultSet*);

/** Deletes the given resultSet and frees memory. */
void peisk_deleteResultSet(PeisTupleResultSet*);

/** Resets the resultSet cursor to point back to _before_ first entry, you
    must call peisk_resultSetNext before getting first value. */
void peisk_resultSetFirst(PeisTupleResultSet*);

/** Returns the tuple currently pointed to by the resultSet cursor */
PeisTuple *peisk_resultSetValue(PeisTupleResultSet*);

/** Steps the resultSet cursor to the next value and returns nonzero unless no more values exists. */
int peisk_resultSetNext(PeisTupleResultSet*);

/** \brief Returns false iff there are more values available in the result set AFTER the current cursor position. Does not step the cursor value. */
int peisk_resultSetIsEmpty(PeisTupleResultSet*);

/** See if two tuples are compatible, ie. could be unified. Returns
    zero if compatible, less then zero if tuple1 is < tuple2 and
    greater than zero if tuple1 is > tuple2.
    
    This function is suitable to be used when sorting lists of tuples
    or getting the maxima/mini of tuples as well as in the search routines.

    Note that the data field of tuples are compared using non case
    sensitive string compares. In the future this might be modifiable
    with tuple specific flags.
*/
int peisk_compareTuples(PeisTuple *tuple1, PeisTuple *tuple2);

/** Similar to peisk_comparTuples but uses treats wildcards
    differently. Returns true if tuple1 is a generlization or equal to
    tuple2 (eg. tuple1 has a wildcard or the same data as tuple2 in
    everyfield.)
*/
int peisk_isGeneralization(PeisTuple *tuple1, PeisTuple *tuple2);

/** Basic test for equality, wildcards match only if both are
    wildcards. Note that the data field of tuples are compared using non case
    sensitive string compares. In the future this might be modifiable
    with tuple specific flags. */
int peisk_isEqual(PeisTuple *tuple1, PeisTuple *tuple2);

/** Converts a tuple error code to a printable string */
const char *peisk_tuple_strerror(int error);

/** Sets a value to a tuple in our own namespace if it does not yet
    exist. Usefull for providing default values to tuples which may be
    configured via the command line --set-tuple option. */
void peisk_setDefaultTuple(const char *key,int datalen, void *data, const char *mimetype, int encoding);

/** Sets a default value to a tuple in our name space it it does not
    yet exists. Also creates a corresponding meta tuple that points to
    it. This allows for this meta tuple to be re-configured to point
    somewhere else at a later timepoint. 

    $key <- data  (if not already existing)
    mi-$key <- (meta SELF $key)
    
    Examples: Assuming that peiscam invokes defaultMetaTuple with arguments "position.x" and the data+len "42". 
    
    ./peiscam
    position.x = 0
    position.mi-x = (meta SELF position.x)
    
    ./peiscam --peis-set-tuple position.x = 42
    position.x = 42
    position.mi-x = (meta SELF position.x)
    
    ./peiscam --peis-set-tuple position.x = 42  + configurator makes changes in my position.mi-x
    position.x = 42
    position.mi-x = (meta pippi pos.cartesian.x)
*/
void peisk_setDefaultMetaTuple(const char *key,int datalen,const void *data,const char *mimetype, int encoding);

/** Sets a value to a tuple in our own namespace if it does not yet
    exist. Usefull for providing default values to tuples which may be
    configured via the command line --set-tuple option. */
void peisk_setDefaultStringTuple(const char *key,const char *value);

/** See peisk_defaultMetaTuple */
void peisk_setDefaultMetaStringTuple(const char *key,const char *data);

/** Test if the named tuple exists in our own namespace. Returns true
    if existing. */
int peisk_tupleExists(const char *key);

/** Convinience function for quickly deleting a tuple given it's name
    and owner. This works by intersting a tuple which expires
    immediatly. A callback functions can be aware of this by noticing
    the expiry field of the tuple which recives the value
    PEISK_TUPLE_EXPIRE_NOW in field 0 */
void peisk_deleteTuple(int owner,const char *key);


/*********************************************************/
/*                                                       */
/*                        META TUPLES                    */
/*                                                       */
/* These functions are used to access meta tuples        */
/* A meta tuple can be accessed just like a normal tuple */
/* but additionally it can be used as an indirect        */
/* reference to other tuples by interpreting the content */
/* of the meta tuple as a reference to another tuple.    */
/*********************************************************/

/** Picks apart the syntax of a meta tuple to give the owner, key of it. */
int peisk_parseMetaTuple(PeisTuple *meta,int *owner,char *key);

/** Creates a subscription to the tuple M and to the tuple [M]
    referenced by the latest content of T continously. Whenever 
    T is changed the subscription to [M] changes also. 
    Tuple M must have a fully qualified key and owner (ie. only point to exactly 
    one meta tuple) */
PeisSubscriberHandle peisk_subscribeIndirectlyByAbstract(PeisTuple *M);

/** Makes an indirect lookup and returns the tuple [M] referenced 
    by the given abstract meta tuple M. This first makes a lookup of
    the concrete tuple refernces by M (called [M]). Secondly it makes
    a lookup of the concrete tuple pointed to by [M] and uses the
    special flags of M (isNew etc) for this lookup. 
    Note that Meta tuples always point only to one specific tuple.
*/
PeisTuple *peisk_getTupleIndirectlyByAbstract(PeisTuple *M);

/** \todo Implement a function getTuplesIndirectly (plural). What should the semantics be? */
/*PeisTuple *peisk_getTuplesIndirectly(PeisTuple *M,PeisResultSet *);*/

/** Makes a lookup for latest value of meta tuple M and initializes 
   abstract tuple A to point to tuple referenced by M. 
   Returns zero on success, error code otherwise */
/* Hidden until we find a compelling use for this...
  int peisk_initIndirectTuple(PeisTuple *M,PeisTuple *A);
*/


/**** These functions are convienience functions that implement behaviours which 
      can be accomplished with the functions above directly. */

/** Creates a subscription to the meta tuple given by metaKey and metaOwner. 
    Neither metaKey nor metaOwner may contain wildcard. */
PeisSubscriberHandle peisk_subscribeIndirectly(int metaOwner,const char *metaKey);

/** Use the meta tuple given by (metaOwner,metaKey) to find a reference to a 
   specific tuple. Returns this referenced tuple [metaOwner,metaKey] if found. 
   Must previously have created a metaSubscription to metaKey and metaOwner. */
PeisTuple *peisk_getTupleIndirectly(int metaOwner,const char *metaKey,int flags);

/** Inserts value into the tuple referenced by the tuple (metaKey,metaOwner). 
    Must already be subscribed to (metaKey,metaOwner) using atleast a normal 
    subscribe but also works with a metaSubscription. 
    Returns zero on success, error code otherwise */
int peisk_setTupleIndirectly(int metaOwner,const char *metaKey,int datalen, const void *data, const char *mimetype, int encoding);

/** Inserts value into the tuple referenced by the tuple (metaKey,metaOwner). 
    Must already be subscribed to (metaKey,metaOwner) using atleast a normal 
    subscribe but also works with a metaSubscription. 
    Returns zero on success, error code otherwise */
int peisk_setStringTupleIndirectly(int metaOwner,const char *metaKey,const char *value);

/** Returns true if the given meta tuple is currently pointing to some
    other tuple. */
int peisk_isMetaTuple(const int metaOwner,const char *metaKey);

/**  \brief Creates an EMPTY meta tuple as the given meta-owner/meta-key if it does not already exist. */
void peisk_declareMetaTuple(int metaOwner,const char *metaKey);

/**  \brief Sets the meta tuple to point to a specific given real tuple */
void peisk_setMetaTuple(int metaOwner,const char *metaKey,int realOwner,const char *realKey);

/*\}@ Tuples */
/*\}@ Ingroup PeisKernel */


#ifdef PEISK_PRIVATE
#include <peiskernel/tuples_private.h>
#endif

#endif

