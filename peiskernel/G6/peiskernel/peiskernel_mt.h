/** \file peiskernel_mt.h
   A multithread (posix threads) wrapper for the peiskernel.template.h.
   If you use this wrapper library no function in peiskernel.h should be used *outside* of a callback routine. Callbacks are invoked from within the peiskernel_mt thread, and hence all calls from within then can and should use the non-mt version of the functions. 
*/
/*
    Copyright (C) 2005 - 2011  Mathias Broxvall

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

#ifndef PEISKERNEL_MT_H
#define PEISKERNEL_MT_H

#include <stdio.h>

#ifndef TUPLES_H
#include "peiskernel/tuples.h"

#define PEISK_TFLAG_OLDVAL         1
#define PEISK_TFLAG_BLOCKING       2
#endif

#ifndef PEISKERNEL_H
#define PEISK_CONNECT_FLAG_FORCE_BCAST  (1<<0)
#endif


/** \ingroup peisk */
/*\{@*/
/** \defgroup peiskmt Multithreaded PEIS kernel wrappers
    Provides a wrapper for multithreaded applications to use the PEIS
    kernel safely. 
    
    Most (but not all) operating systems and programming languages
    have an implementation of posix threads, for these systems and
    languages there exists a multithreaded wrapper around the
    peiskernel. This simplifies the use of the peiskernel
    significantly since it eliminates the need to periodcially call
    the peisk_step function from within the application. 
    To use these wrappers, make sure to link against -lpeiskernel_mt
    and use only the functions defined in peiskernel_mt.h, all these
    functions have the prefix peiskmt_. 

    This library is required when the application nativly uses
    threads and may call kernel functions from any threads. 

    When using callbacks some caution should be used. In this
    situation, callbacks etc. can be registered from any thread using
    the peiskmt_ functions. However, since the callbacks will be
    called by an internal peiskernel thread, they must make sure to
    terminate quickly (within approx 10-100ms), OR use the peisk_step
    function internally. Note that inside these callback functions it
    is an error to use any of the multithreaded wrappers, ie. do not
    use the peiskmt_* functions, only the peisk_* functions. 
*/
/*\{@*/


/** Initializes peiskernel using any appropriate commandline options
    and start a new running thread for it. */
void peiskmt_initialize(int *argc,char **args);

/******************************************************************/
/*                                                                */
/*                          DATATYPES                             */
/*                                                                */
/* See also tuples.h for the main tuple datatype and function     */
/* pointer datatypes for registering callbacks and error codes    */
/******************************************************************/

typedef int peiskmt_callbackHandle;
typedef int peiskmt_subscriptionHandle;

/********************************************************************/
/*                                                                  */
/*                          INITIALIZATION etc.                     */
/*                                                                  */
/********************************************************************/


/** Shutdowns the peiskernel, stops the peiskernel thread and frees
    all related datastructures. */
void peiskmt_shutdown();
/** Gives the ID this peis is currently running as. */
int peiskmt_peisid();                                             

/** Locks the semaphore used by the peiskernel thread to avoid
    conflicts when calling peisk_* functions directly. */
void peiskmt_lock();
/** Unlocks the semaphore used by the peiskernel thread to avoid
    conflicts when calling peisk_* functions directly. */
void peiskmt_unlock();

/** True if host is known and reachable according to routing tables. */
int peiskmt_isRoutable(int id);

/********************************************************************/
/*                                                                  */
/*                     SIMPLIFIED TUPLE INTERFACE                   */
/*                                                                  */
/* Provides a simplified interface to tuples, implemented as        */
/*   wrappers around normal tuple interface                         */
/*                                                                  */
/********************************************************************/

/** Simplifying wrapper for creating and setting a tuple in local
    tuplespace, propagating it to all subscribers. Provides backwards
    compatability with kernel G3 and earlier. Multithread version of
    peisk_setTuple() */
void peiskmt_setTuple(const char *key,int len,const void *data,const char *mimetype,int encoding);              

/** A wrapper function for setting tuples whose value are simple
    C-string. Provides backwards compatability with kernel G3 and
    earlier. multithread version of peisk_setStringTuple() */
void peiskmt_setStringTuple(const char *key,const char *data);

/** Wrapper for creating and setting a tuple in a tuplespace belonging
    to some other peis. Propagation is done by the other peis if
    successfull. Provides backwards compatability with kernel G3 and
    earlier. Multithread version of peisk_setRemoteTuple() */
void peiskmt_setRemoteTuple(int owner,const char *key,int len,const void *data,const char *mimetype,int encoding);

/** Wrapper for creating and setting a tuple in a tuplespace belonging
    to some other peis and blocking until acknowledged. 
    Propagation is done by the other peis if
    successfull. Provides backwards compatability with kernel G3 and
    earlier. Multithread version of peisk_setRemoteTuple() 

    \return Returns zero if successfull.
    (note. failure may be delayed if modifying tuples inside other
    components, such failures are not reported here) */
int peiskmt_setRemoteTupleBlocking(int owner,const char *key,int len,const void *data,const char *mimetype,int encoding);

/** Wrapper for creating and setting a tuple in a tuplespace belonging
    to some other peis. Propagation is done by the other peis if
    successfull. */
void peiskmt_setRemoteStringTuple(int owner,const char *key,const char *value);

/** Wrapper for creating and setting a tuple in a tuplespace belonging
    to some other peis. Propagation is done by the other peis if
    successfull. 
    \return Returns zero if successfull.
    (note. failure may be delayed if modifying tuples inside other
    components, such failures are not reported here) */
int peiskmt_setRemoteStringTupleBlocking(int owner,const char *key,const char *value);

/** Wrapper for getting a value from the local tuplespace for the
    fully qualitified given key and owner. 
    \param key The keyname of the tuple.
    \param owner The owner ID for the namespace in which tuple belong
    Returns nonzero if successfull (returns pointer to tuple
    structure, use with care!)
    \param flags Flags modifiying the behaviour of getTuple. Bitwise OR of
    the flags PEISK_KEEP_OLD, PEISK_FILTER_OLD, PEISK_BLOCKING and PEISK_NON_BLOCKING - respectively. 

    Multithread version of peisk_getTuple()
*/
struct PeisTuple *peiskmt_getTuple(int owner,const char *key,int flags);


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
int peiskmt_getTuples(int owner, const char *key,PeisTupleResultSet *);

/** Wrapper for subscribing to tuples with given key from given owner
    (or -1 for wildcard on owner). Multithread version of
    peisk_subscribe() */
PeisSubscriberHandle peiskmt_subscribe(int owner,const char *key);    


/** Wrapper to add a callback function to tuples with given fully
    qualified key and owner; which is called when tuple is changed in
    the local tuplespace. Provides backwards compatability with kernel
    G3 and earlier. Returns callback handle if successfull, 0 (or
    NULL) if unsuccessfull. Multithread version of
    peisk_registerTupleCallback() */
PeisCallbackHandle peiskmt_registerTupleCallback(int owner,const char *key,void *userdata,PeisTupleCallback *fn);

/** Wrapper to tests if anyone is currently subscribed to the given
    (full) key in our local tuplespace. Wrapper for backwards
    compatability. */
int peiskmt_hasSubscriber(const char *key);

/** If the peiskernel is currently supposed to be running or is
    aborted. */
int peiskmt_isRunning();                      

/** Sets the DEFAULT user defined timestamp (ts_user) for all comming tuples that are created. */
void peiskmt_tsUser(int ts_user_sec,int ts_user_usec);    

/** Unsubscribe to tuples. Returns zero on success, error number
    otherwise. Multithread version of peisk_unsubscribe() */
void peiskmt_unsubscribe(PeisSubscriberHandle);

/** Remove a given callback. Returns zero on success, otherwise error
    code. multithread version of peisk_unregisterTupleCallback() */
void peiskmt_unregisterTupleCallback(PeisCallbackHandle callback);

/** Sets a value to a tuple in our own namespace if it has not
    yet been given a  value. Usefull for providing default values to
    tuples which may be configured via the command line --set-tuple option */
void peiskmt_setDefaultStringTuple(const char *key,const char *value);
/** Sets a value to a tuple in our own namespace if it has not
    yet been given a  value. Usefull for providing default values to
    tuples which may be configured via the command line --set-tuple option */
void peiskmt_setDefaultTuple(const char *key,int datalen, void *data,const char *mimetype,int encoding);

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
void peiskmt_setDefaultMetaTuple(const char *key,int datalen, void *data,const char *mimetype,int encoding);
/** See peisk_defaultMetaTuple */
void peiskmt_setDefaultMetaStringTuple(const char *key,const char *value);

/** Test if the named tuple exists in our own namespace. Returns true
    if existing. */
int peiskmt_tupleExists(const char *key);

/** Convinience function for quickly deleting a tuple given it's name
    and owner. This works by intersting a tuple which expires
    immediatly. A callback functions can be aware of this by noticing
    the expiry field of the tuple which recives the value
    PEISK_TUPLE_EXPIRE_NOW in field 0 */
void peiskmt_deleteTuple(int owner,const char *key);


/** Returns the error code for the last user called function. */
int peiskmt_getTupleErrno();

/** Prints a tuple in human readable format to stdout, used
    for debugging. Multithreaded version of peisk_printTuple */
void peiskmt_printTuple(PeisTuple *);

/** Prints a tuple in human readable format to given buffer, used
    for debugging. Multithreaded version of peisk_snprintTuple */
void peiskmt_snprintTuple(char *buf,int len,PeisTuple *);

/** Clone a tuple (including the data field). The user of this
    function is responsible for freeing the memory eventually, see
    peisk_freeTuple. Updates peisk_tuples_errno if
    failed. Multithreaded version of peisk_cloneTuple */
PeisTuple *peiskmt_cloneTuple(PeisTuple *tuple);

/** Free a tuple (including the data field). */
void peiskmt_freeTuple(PeisTuple *tuple);

/** Subscribe using given abstract tuple as prototype */
PeisSubscriberHandle peiskmt_subscribeByAbstract(PeisTuple *);

/** Forces a resend of subscription and reload all subscribed
    tuples. */
int peiskmt_reloadSubscription(PeisSubscriberHandle subscriber);

/** Tests if anyone is currently subscribed to tuples comparable to the
    given prototype. Ie. if the prototype is concrete then this is a
    test if any subscriber would be triggered by it. If the prototype
    is abstract, then tests if it partially overlaps any subscription.
    For instance, if we have a subscription for A.*.C and we test with
    prototype A.B.* then the result is _true_. 

    Returns nonzero if test is true. */
int peiskmt_hasSubscriberByAbstract(PeisTuple *prototype);

/** Callback function using given abstract tuple as prototype. Returns
    callback handle if successfull, 0 (or NULL) if unsuccessfull. */
PeisCallbackHandle peiskmt_registerTupleCallbackByAbstract(PeisTuple *,void *userdata,PeisTupleCallback *fn);


/** Wrapper to add a callback function to tuples when they are
    deleted. Uses a given fully qualified key and owner.Returns
    callback handle if successfull, 0 (or NULL) if unsuccessfull.*/
PeisCallbackHandle peiskmt_registerTupleDeletedCallback(int owner,const char *key,void *userdata,PeisTupleCallback *fn);

/** Callback function using given abstract tuple as prototype. Invoked
    when matching tuples are deleted. Returns callback handle if
    successfull, 0 (or NULL) if unsuccessfull. */ 
PeisCallbackHandle peiskmt_registerTupleDeletedCallbackByAbstract(PeisTuple *,void *userdata,PeisTupleCallback *fn);


/** Initializes a tuple to reasonable default values for inserting
    into some tuplespace. Modify remaining fields before calling
    insertTuple or findTuple. Differs from peiskmt_initAbstractTuple in
    what the default values are.  */
void peiskmt_initTuple(PeisTuple*);

/** Initializes a tuple to reasonable default values for searching in
    local tuplespace. Modify remaining fields before calling
    insertTuple or findTuple. Differs from peiskmt_initTuple in what the
    default values are. */
void peiskmt_initAbstractTuple(PeisTuple*);

/** Decomposes a name into dot separated substrings and puts into the
    tuplefield. Returns zero on success. */
int peiskmt_setTupleName(PeisTuple*,const char *fullname);

/** Returns the key name of given tuple, eg. "foo.*.a" if the tuple
    has a three key parts where the second is a wildcard. Returns zero
    on success. */
int peiskmt_getTupleName(PeisTuple*,char *buffer,int buflen);


/** Test if a tuple is abstract (contain any wildcards). Returns nonzero if it is abstract. */
int peiskmt_tupleIsAbstract(PeisTuple *);

/** \brief Inserts a tuple into local or remote tuplespace. 

    The tuple is copied into private memory prior to insertion (thus it is safe to
    allocate tuples and their data on the stack). Tuples must be fully
    qualified and contain no wildcards. 

    \param tuple is a pointer to the tuple to be sent

    \return Returns zero if successfull.
    (note. failure may be delayed if modifying tuples inside other
    components, such failures are not reported here) */
int peiskmt_insertTuple(PeisTuple*);

/** \brief Inserts a tuple into local or remote tuplespace and BLOCKS until completion. 

    The tuple is copied into private memory prior to insertion (thus it is safe to
    allocate tuples and their data on the stack). Tuples must be fully
    qualified and contain no wildcards. Returns zero if successfull.

    \param tuple is a pointer to the tuple to be sent

    \return Returns zero if successfull.
    (note. failure may be delayed if modifying tuples inside other
    components, such failures are not reported here) */
int peiskmt_insertTupleBlocking(PeisTuple*);

/** Looks up a tuple in the local tuplespace and locally cached
    tuples using the fully qualified owner and key only (no wildcards
    permitted). Returns pointer to tuple if successfull. */
PeisTuple *peiskmt_getTupleByAbstract(PeisTuple *prototype);

/** Looks up a tuple in the local tuplespace and locally cached tuples. 
    May contain wildcards and may give multiple results. 
    A previously created resultSet must be passed as argument, freeing
    this resultSet is the responsibility of the caller. The function
    precomputes all matching tuples and sets the given
    PeisTupleResultSet to point to first found tuple and returns
    nonzero if successfull. Note that the resultSet is not reset by a
    call to this function, thus multiple getTuplesByAbstract calls in a row
    gives a tuple resultSet with all the results concatenated. 
    Returns number of new tuples added to the resultSet.
*/
int peiskmt_getTuplesByAbstract(PeisTuple *,PeisTupleResultSet *);

/** Creates a fresh resultSet that can be used for iterating over
    tuples, you must call peiskmt_resultSetNext before getting first
    value. */
PeisTupleResultSet *peiskmt_createResultSet();

/** Resets the resultSet to empty. Does not modify the cursor pointer. */
void peiskmt_resultSetReset(PeisTupleResultSet*);

/** Deletes the given resultSet and frees memory. */
void peiskmt_deleteResultSet(PeisTupleResultSet*);

/** Resets the resultSet cursor to point back to _before_ first entry, you
    must call peisk_resultSetNext before getting first value. */
void peiskmt_resultSetFirst(PeisTupleResultSet*);

/** Returns the tuple currently pointed to by the resultSet cursor */
PeisTuple *peiskmt_resultSetValue(PeisTupleResultSet*);

/** Steps the resultSet cursor to the next value and returns nonzero unless no more values exists. */
int peiskmt_resultSetNext(PeisTupleResultSet*);

/** \brief Returns false iff there are more values available in the result set AFTER the current cursor position. Does not step the cursor value. */
int peiskmt_resultSetIsEmpty(PeisTupleResultSet*);

/** See if two tuples are compatible, ie. could be unified. Returns
    zero if compatible, less then zero if tuple1 is < tuple2 and
    greater than zero if tuple1 is > tuple2.
    
    This function is suitable to be used when sorting lists of tuples
    or getting the maxima/mini of tuples as well as in the search routines.

    Note that the data field of tuples are compared using non case
    sensitive string compares. In the future this might be modifiable
    with tuple specific flags.
*/
int peiskmt_compareTuples(PeisTuple *tuple1, PeisTuple *tuple2);

/** Similar to peiskmt_comparTuples but treats wildcards
    differently. Returns true if tuple1 is a generlization or equal to
    tuple2 (eg. tuple1 has a wildcard or the same data as tuple2 in
    everyfield.)
*/
int peiskmt_isGeneralization(PeisTuple *tuple1, PeisTuple *tuple2);

/** Basic test for equality, wildcards match only if both are
    wildcards. Note that the data field of tuples are compared using non case
    sensitive string compares. In the future this might be modifiable
    with tuple specific flags. */
int peiskmt_isEqual(PeisTuple *tuple1, PeisTuple *tuple2);

/** Converts a tuple error code to a printable string */
const char *peiskmt_tuple_strerror(int error);


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
int peiskmt_parseMetaTuple(PeisTuple *meta,int *owner,char *key);

/** Creates a subscription to the tuple M and to the tuple [M]
    referenced by the latest content of T continously. Whenever 
    T is changed the subscription to [M] changes also. Tuple M must 
    have a fully qualified key and owner (ie. only point to exactly 
    one meta tuple) */
PeisSubscriberHandle peiskmt_subscribeIndirectlyByAbstract(PeisTuple *M);

/** Makes an indirect lookup and returns the tuple [M] referenced 
    by meta tuple M. Meta tuples always point only to one specific tuple. */
PeisTuple *peiskmt_getTupleIndirectlyByAbstract(PeisTuple *M);

/** Makes a lookup for latest value of meta tuple M and initializes 
   abstract tuple A to point to tuple referenced by M. 
   Returns zero on success, error code otherwise */
int peiskmt_initIndirectTuple(PeisTuple *M,PeisTuple *A);

/** Creates a subscription to the meta tuple given by metaKey and metaOwner. 
    Neither metaKey nor metaOwner may contain wildcard. */
PeisSubscriberHandle peiskmt_subscribeIndirectly(int metaOwner,const char *metaKey);

/** Use the meta tuple given by (metaOwner,metaKey) to find a reference to a 
   specific tuple. Returns this referenced tuple [metaOwner,metaKey] if found. 
   Must previously have created a metaSubscription to metaKey and metaOwner. */
PeisTuple *peiskmt_getTupleIndirectly(int metaOwner,const char *metaKey,int flags);

/** Inserts value into the tuple referenced by the tuple (metaKey,metaOwner). 
    Must already be subscribed to (metaKey,metaOwner) using atleast a normal 
    subscribe but also works with a metaSubscription or if metaOwner
    is ourselves.
    Returns zero on success, error code otherwise */
int peiskmt_setStringTupleIndirectly(int metaOwner,const char *metaKey,const char *value);

/** Returns true if the given meta tuple is currently pointing to a
    valid tuple. */
int peiskmt_isMetaTuple(int metaOwner,const char *metaKey);

/**  \brief Creates a meta tuple as the given meta-owner/meta-key. Initially points to no concrete data.  */
void peiskmt_declareMetaTuple(int metaOwner,const char *metaKey);

/**  Sets the meta tuple to point to given real tuple */
void peiskmt_setMetaTuple(int metaOwner,const char *metaKey,int realOwner,const char *realKey);

/** Blocks until an owner for the given key (in any possible owners namespace) have been found. Returns the ID of the found owner. */
int peiskmt_findOwner(char *key);



/********************************************************************/
/*                                                                  */
/*                        KERNEL FUNCTIONS                          */
/*                                                                  */
/********************************************************************/

/** Prints all peiskernel specific options */
void peiskmt_printUsage(FILE *stream,short argc,char **args);     

/** Gives the current time in seconds since the EPOCH with integer
    precision. */
int peiskmt_gettime();                                            
/** Gives the current time in seconds since the EPOCH with
    floatingpoint precision. */
double peiskmt_gettimef();
/** Gives the current time in seconds and microseconds since the EPOCH
    with two long int pointer arguments where the result is stored. */
void peiskmt_gettime2(int *t0,int *t1);
/** Force a connection to specific url. Returns -1 if failed */
int peiskmt_connect(char *url,int flags);                                   
/** Add to list of hosts that will repeatedly be attempted to be
    connected to */
void peiskmt_autoConnect(char *url); 

/** Appends the given data to the matching tuple in the tuple
    space. The argument tuple is here an ABSTRACT TUPLE that
    identifies which tuple that should be appended. Thus, you would
    normally have an empty data field in the given abstractTuple. If
    there is wildcards in the owner and or key parts of the abstract
    tuple, the data will be appended to each such tuple. If there is a
    concrete sequence number in the given tuple, then the append will
    only be performed if the two tuples have the same sequence number.

    Note that due to the semantics of appending tuples, attempting to perform an append on a tuple that does not yet exist will not result in any append operation on it.
*/
void peiskmt_appendTupleByAbstract(PeisTuple *abstractTuple,int difflen,const void *diff);
/** \brief Appends the given string to the matching tuple in the tuple space. 

    The argument tuple is here an ABSTRACT TUPLE that identifies which tuple that should be appended. 
    You would normally have an empty data field in the given abstractTuple. If
    there is wildcards in the owner and or key parts of the abstract
    tuple, the data will be appended to each such tuple. If there is a
    concrete sequence number in the given tuple, then the append will
    only be performed if the two tuples have the same sequence number.

    Note that due to the semantics of appending tuples, attempting to perform an append on a tuple that does not yet exist will not result in any append operation on it.
*/
void peiskmt_appendStringTupleByAbstract(PeisTuple *abstractTuple,const char *value);

/** \brief Appends the given data to the tuple(s) specified by the given owner and key. 
    
    This function disregard any checks to see that the appended tuple is as we expected since previous (ie. no sequence numbers can be specified). 
    Note that due to the semantics of appending tuples, attempting to perform an append on a tuple that does not yet exist will not result in any append operation on it.
 */
void peiskmt_appendTuple(int owner,const char *key,int difflen,const void *diff);
/** \brief Appends the given string to the tuple(s) specified by the given owner and key. 

    This function disregard any checks to see that the appended tuple is as we expected since previous (ie. no sequence numbers can be specified). 
    Note that due to the semantics of appending tuples, attempting to perform an append on a tuple that does not yet exist will not result in any append operation on it.
*/
void peiskmt_appendStringTuple(int owner,const char *key,const char *value);


/*\}@ PeisKernelMT */
/*\}@ Ingroup PeisKernel */

#endif

