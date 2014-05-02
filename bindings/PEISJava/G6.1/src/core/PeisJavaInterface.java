/*
    Copyright (C) 2008 - 2012 Federico Pecora
    
    Based on libpeiskernel (Copyright (C) 2005 - 2012  Mathias Broxvall).
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301 USA.
*/


package core;
import com.sun.jna.Callback;
import com.sun.jna.Library;
import com.sun.jna.Pointer;
import com.sun.jna.ptr.IntByReference;

public interface PeisJavaInterface extends Library {

	public static final String version = "v0.6.0.b";

	/*
	 * CALLBACK FUNCTION DEFS
	 */

	/**
	 * Callback hook for registering a tuple callback (triggered when value in tuple changes).
	 * @author fpa
	 */
	public interface PeisTupleCallback extends Callback {
		void callback(PeisTuple tuple, Pointer userdata);
	}
		
	/**
	 * 
	 * @author fpa
	 *
	 */
	public interface PeisCallbackHandle extends Callback {
		int callback();
	}
	

	/**
	 * 
	 * @author fpa
	 *
	 */
	public interface PeisSubscriberHandle extends Callback {
		int callback();
	}

	/**
	 * Callback hook for intercepting packages. Return non zero to stop further processing (routing).
	 * @author fpa
	 */
	public interface PeisHook extends Callback {
		int callback(int port, int destination, int sender, int datalen, Pointer data);
	}
	
	/**
	 * Callback hook for intercepting packages. Return non zero to stop further processing (routing).
	 * @author fpa
	 */
	public interface PeisPeriodic extends Callback {
		int callback(Pointer user);
	}

	/**
	 * Callback hook to be used when notified of hosts removed from the peis network.
	 * @author fpa
	 */
	public interface PeisDeadhostHook extends Callback {
		int callback(int id, Pointer user);
	}
	
	/**
	 * Wrapper to add a callback function to tuples with given fully
	 * qualified key and owner; which is called when tuple is changed in
	 * the local tuplespace. Provides backwards compatibility with kernel
	 * G3 and earlier.
	 * @param key
	 * @param owner
	 * @param userdata
	 * @param fn
	 * @return Callback handle if successful, {@code null}
	 * if unsuccessful.
	 */
	public PeisCallbackHandle peisk_registerTupleCallback(int owner, String key, Pointer userdata, PeisTupleCallback fn);
	
	
	/**
	 *  Wrapper to add a callback function to tuples when they are deleted. Uses a given fully qualified
	 *  key and owner. Returns callback handle if successful, 0 (or NULL) if unsuccessful.
	 * @param key
	 * @param owner
	 * @param userdata
	 * @param fn
	 * @return Callback handle if successful, 0 (or NULL) if unsuccessful.
	 */
	public PeisCallbackHandle peisk_registerTupleDeletedCallback(int owner, String key, Pointer userdata, PeisTupleCallback fn);

	
	/**
	 * Callback function using given abstract tuple as prototype. Returns callback handle if successful, 0 (or NULL) if unsuccessful.
	 * @param tuple
	 * @param userdata
	 * @param fn
	 * @return callback handle if successful, 0 (or NULL) if unsuccessful.
	 */
	//public PeisCallbackHandle peisk_registerAbstractTupleCallback(PeisTuple tuple, Pointer userdata, PeisTupleCallback fn);
	public PeisCallbackHandle peisk_registerTupleCallbackByAbstract(PeisTuple tuple, Pointer userdata, PeisTupleCallback fn);
	
	
 
	/**
	 * Callback function using given abstract tuple as prototype. Invoked when matching tuples are deleted. Returns callback handle if successful, 0 (or NULL) if unsuccessful.
	 * @param tuple
	 * @param userdata
	 * @param fn
	 * @return callback handle if successful, 0 (or NULL) if unsuccessful.
	 */
	//public PeisCallbackHandle peisk_registerAbstractTupleDeletedCallback(PeisTuple tuple, Pointer userdata, PeisTupleCallback fn);
	public PeisCallbackHandle peisk_registerTupleDeletedCallbackByAbstract(PeisTuple tuple, Pointer userdata, PeisTupleCallback fn);

	
	/**
	 * Unregister a tuple callback given its handle.
	 * @param handle The handle of the tuple callback to unregister.
	 */
	//DO EXCEPTION
	public int peisk_unregisterTupleCallback(PeisCallbackHandle handle);

	/*
	 * PEISKERNEL FUNCTIONS    
	 */
	
	/**
	 * Initializes peiskernel using any appropriate commandline options.
	 * @param argc The number of parameters 
	 * @param args The parameters
	 */
	public void peisk_initialize(IntByReference argc, String[] args);
	
	/**
	 * Prints a tuple in human readable format to stdout, used
	 * for debugging.
	 * @param tuple The tuple to print.
	 */
	public void peisk_printTuple(PeisTuple tuple);
	
	/**
	 * Sets the user defined timestamp (ts_user) for all comming tuples
	 *  that are created. This is for backwards compatability but also
	 *  convinience when many tuples are created.
	 * @param ts_user_sec Time (seconds)
	 * @param ts_user_usec Time (milliseconds)
	 */
	public void peisk_tsUser(int ts_user_sec,int ts_user_usec);


	/**
	 * A wrapper function for setting tuples whose value are simple
	 * String. Provides backwards compatibility with kernel G3 and earlier.
	 * @param key The key of the tuple to be set.
	 * @param value The value to set the tuple with.
	 */
	public void peisk_setStringTuple(String key, String value);
	
	/**
	 * Ascertain if the peiskernel is running.
	 * @return {@code true} after initialization until we have performed a shutdown.
	 */
	public boolean peisk_isRunning();
	
	/**
	 * Wait given number of microseconds while maintaining all peis services.
	 * @param useconds The amount of time in microseconds to wait.
	 */
	public void peisk_wait(int useconds);
	
	/**
	 * Waits until something interesting happens on the sockets, or until
	 * the given amount of time has passed. Does not call any other PEIS
	 * related functions (i.e., it does not invoke the step function, and hence
	 * the maxUSeconds should be less than the cycle time of 10.000uS (100Hz))
	 * @param maxUSeconds The amount of time in microseconds to wait.
	 */
	public void peisk_waitOneCycle(int maxUSeconds);
	
	/**
	 * Wrapper for getting a value from the local tuplespace for the fully qualified given key and owner. 
	 * Provides backwards compatibility with kernel G3 and earlier.
	 * @param key The keyname of the tuple.
	 * @param owner The owner of the tuple.
	 * @param flags Flags modifying the behaviour of getTuple.
	 * Logical or of flags: PEISK_TFLAG_OLDVAL,  PEISK_TFLAG_BLOCKING.
	 * @return The tuple if successful {@code null} otherwise.
	 */
	public PeisTuple peisk_getTuple(int owner, String key, int flags);

	/**
	 * Wrapper for getting values, one at a a time, from local tuplespace
	 * for the given fully qualified key and any producer. Uses an
	 * integer "handle" to iterate over all values. Returns non-{@code null} if a
	 * value existed and if so modifies len, ptr to the data for this
	 * tuple and sets owner to the owner of this tuple.
	 * @param owner The owner of the sought tuples.
	 * @param key The keyname of the tuples.
	 * @param result a result set containing the found tuples.
	 * @return The tuples if successful {@code null} otherwise.
	 */
	public int peisk_getTuples(int owner, String key, PeisTupleResultSet result);
	
	/**
	 * Clone a tuple (including the data field). The user of this
	 * function is responsible for freeing the memory eventually, see
	 * peisk_freeTuple.
	 * @param tuple The tuple to clone.
	 * @return The new tuple.
	 */
	public PeisTuple peisk_cloneTuple(PeisTuple tuple);
	
	/**
	 * Free a tuple (including the data field).
	 * @param tuple
	 */
	public void peisk_freeTuple(PeisTuple tuple);
	
	/**
	 * Wrapper for subscribing to tuples with given key from given owner
	 * (or -1 for wildcard on owner).
	 * @param key
	 * @param owner
	 * @return The handle for this tuple subscription if success, {@code null} otherwise.
	 */
	public PeisSubscriberHandle peisk_subscribe(int owner, String key);
	
	/**
	 * Subscribe using given abstract tuple as prototype.
	 * @param tuple The abstract tuple to use as prototype. 
	 * @return The handle for this tuple subscription if success, {@code null} otherwise.
	 */
	//public PeisSubscriberHandle peisk_subscribeAbstract(PeisTuple tuple);
	public PeisSubscriberHandle peisk_subscribeByAbstract(PeisTuple tuple);
	
	/**
	 * Unsubscribe to tuples.
	 * @param handle The handle of the subscription to unsunbscribe.
	 * @return Zero on success, error number otherwise.
	 */
	public boolean peisk_unsubscribe(PeisSubscriberHandle handle);
	
	/**
	 * Forces a resend of subscription and reload all subscribed
	 * tuples.
	 * @param subscriber The handle of the subscription to reload.
	 * @return Zero on success, error number otherwise.
	 */
	public int peisk_reloadSubscription(PeisSubscriberHandle subscriber);
	
	/**
	 * Tests if anyone is currently subscribed to tuples comparable to the
	 * given prototype. I.e., if the prototype is concrete then this is a
	 * test if any subscriber would be triggered by it. If the prototype
	 * is abstract, then tests if it partially overlaps any subscription.
	 * For instance, if we have a subscription for A.*.C and we test with
	 * prototype A.B.* then the result is _true_.
	 * @param prototype The tuple to compare against.
	 * @return {@code true} iff there is at least one subscriber to this tuple.
	 */
	//public boolean peisk_isSubscribed(PeisTuple prototype);
	public boolean peisk_hasSubscriberByAbstract(PeisTuple prototype);
	
	
	/**
	 * Wrapper to tests if anyone is currently subscribed to the given (full) key in our local tuplespace. Wrapper for backwards compatability.
	 * @param key
	 * @return Whether anyone is subscribed to a tuple with the given key.
	 */
	public boolean peisk_hasSubscriber(String key);

	/**
	 * Decomposes a name into dot separated substrings and puts into the
	 * tuplefield.
	 * @param tup The tuple of which to set the name.
	 * @param fullname The full name of the tuple.
	 * @return {@code true} iff the operation was successful.
	 */
	public int peisk_setTupleName(PeisTuple tup, String fullname);


	/**
	 * Initializes a tuple to reasonable default values for inserting
	 * into some tuplespace. Modify remaining fields before calling
	 * insertTuple or findTuple. Differs from peisk_initAbstractTuple in
	 * what the default values are.
	 * @param tup The tuple to initialize.
	 */
	public void peisk_initTuple(PeisTuple tup);
	
	/**
	 * Initializes a tuple to reasonable default values for searching in
	 * local tuplespace. Modify remaining fields before calling
	 * insertTuple or findTuple. Differs from peisk_initTuple in what the
	 * default values are.
	 * @param tup The abstract tuple to be initialized.
	 */
	public void peisk_initAbstractTuple(PeisTuple tup);
	
	/**
	 * Inserts a tuple into local or remote tuplespace. The tuple is
	 * copied into private memory prior to insertion (thus it is safe to
	 * allocate tuples and their data on the stack). Tuples must be fully
	 * qualified and contain no wildcards.
	 * @param tuple The tuple to insert.
	 * @return {@code true} if successful (NOTE: failure may be delayed
	 * if modifying tuples inside other components, such failures are not reported here).
	 */
	public boolean peisk_insertTuple(PeisTuple tuple);
	
	/**
	 * Inserts a tuple into local or remote tuplespace and BLOCKS until completion. The tuple is
	 * copied into private memory prior to insertion (thus it is safe to
	 * allocate tuples and their data on the stack). Tuples must be fully
	 * qualified and contain no wildcards.
	 * @param tuple The tuple to insert.
	 * @return {@code true} if successful (NOTE: failure may be delayed
	 * if modifying tuples inside other components, such failures are not reported here).
	 */
	public boolean peisk_insertTupleBlocking(PeisTuple tuple);


	/**
	 * Appends the given data to the matching tuple in the tuple
    space. The argument tuple is here an ABSTRACT TUPLE that
    identifies which tuple should be appended to. Thus, you would
    normally have an empty data field in the given abstractTuple. If
    there are wildcards in the owner and/or key parts of the abstract
    tuple, the data will be appended to each such tuple. If there is a
    concrete sequence number in the given tuple, then the append will
    only be performed if the two tuples have the same sequence number.
	 * @param abstractTuple
	 * @param difflen
	 * @param diff
	 */
	//public void peisk_appendTuple(PeisTuple abstractTuple, int difflen, Pointer diff);
	public void peisk_appendTupleByAbstract(PeisTuple abstractTuple, int difflen, Pointer diff);


	/**
	 * Appends the given data to the matching tuple in the tuple
    space. The argument tuple is here an ABSTRACT TUPLE that
    identifies which tuple should be appended to. Thus, you would
    normally have an empty data field in the given abstractTuple. If
    there are wildcards in the owner and/or key parts of the abstract
    tuple, the data will be appended to each such tuple. If there is a
    concrete sequence number in the given tuple, then the append will
    only be performed if the two tuples have the same sequence number.
	 * @param abstractTuple
	 * @param diff
	 */
	//public void peisk_appendTuple(PeisTuple abstractTuple, int difflen, Pointer diff);
	public void peisk_appendStringTupleByAbstract(PeisTuple abstractTuple, String diff);

	
	/**
	 * Looks up a tuple in the local tuplespace and locally cached
	 * tuples using the fully qualified owner and key only (no wildcards
	 * permitted).
	 * @param tuple The tuple to find.
	 * @return {@code null} if the tuple was not found. 
	 */
	//public PeisTuple peisk_findSimpleTuple(PeisTuple tuple);
	public PeisTuple peisk_getTupleByAbstract(PeisTuple tuple);
	
	/**
	 * Looks up a tuple in the local tuplespace and locally cached tuples.
	 * May contain wildcards and may give multiple results.
	 * A previously created resultSet must be passed as argument, freeing
	 * this resultSet (using the deleteResultSet command) is the responsibility
	 * of the caller. The function precomputes all matching tuples and sets the given
	 * PeisTupleResultSet to point to first found tuple and returns
	 * nonzero if successful. Note that the resultSet is not reset by a
	 * call to this function, thus multiple findTuples calls in a row
	 * gives a tuple resultSet with all the results concatenated.
	 * @param tuple The (abstract) tuple with which to perform the search. 
	 * @param resultset The results set object into which to put the result.
	 * @return The number of new tuples added to the resultSet.
	 */
	//public int peisk_findTuples(PeisTuple tuple, PeisTupleResultSet resultset);
	public int peisk_getTuplesByAbstract(PeisTuple tuple, PeisTupleResultSet resultset);
	
	/**
	 * Creates a fresh resultSet that can be used for iterating over
	 * tuples, you must call peisk_resultSetNext before getting first
	 * value.
	 * @return The new result set (to pass to findTuples()).
	 */
	public PeisTupleResultSet peisk_createResultSet();
	
	/**
	 * Resets the resultSet to empty. Does not modify the index pointer.
	 * @param resultSet The result set to reset.
	 */
	public void peisk_resultSetReset(PeisTupleResultSet resultSet);
	
	/**
	 * Deletes the given resultSet and frees memory.
	 * @param resultSet The result set to reset.
	 */
	public void peisk_deleteResultSet(PeisTupleResultSet resultSet);
	
	/**
	 * Resets an resultSet to point back to _before_ first entry, you
	 * must call peisk_resultSetNext before getting first value.
	 * @param resultSet The result set to reset.
	 */
	public void peisk_resultSetFirst(PeisTupleResultSet resultSet);
	
	/**
	 * Returns the tuple currently pointed to by resultSet.
	 * @param resultSet The result set to access. 
	 * @return The tuple currently pointed to by result set.
	 */
	public PeisTuple peisk_resultSetValue(PeisTupleResultSet resultSet);
	
	/**
	 * Steps the resultSet to the next value.
	 * @param resultSet The result set to access.
	 * @return Non-zero unless no more values exists.
	 */
	public int peisk_resultSetNext(PeisTupleResultSet resultSet);
	
	/**
	 * See if two tuples are compatible, i.e., could be unified.
	 * This function is suitable to be used when sorting lists of tuples
	 * or getting the max/min of tuples as well as in the search routines.
	 * Note that the data field of tuples are compared using non case
	 * sensitive string compares. In the future this might be modifiable
	 * with tuple specific flags.
	 * @param tuple1 The first tuple.
	 * @param tuple2 The second tuple.
	 * @return Zero if compatible, less then zero if tuple1 is < tuple2 and
	 * greater than zero if tuple1 is > tuple2.
	 */
	public int peisk_compareTuples(PeisTuple tuple1, PeisTuple tuple2);
	
	/**
	 * Similar to {@link #peisk_compareTuples(PeisTuple tuple1, PeisTuple tuple2)} but treats wildcards
	 * differently. 
	 * @param tuple1 The first tuple.
	 * @param tuple2 The tuple to compare against.
	 * @return {@code true} if tuple1 is a generalization or equal to
	 * tuple2 (e.g., tuple1 has a wildcard or the same data as tuple2 in
	 * every field).
	 */
	public boolean peisk_isGeneralization(PeisTuple tuple1, PeisTuple tuple2);
	
	/**
	 * Basic test for equality, wildcards match only if both are
	 * wildcards. Note that the data field of tuples are compared using non case
	 * sensitive string compares. In the future this might be modifiable
	 * with tuple specific flags.
	 * @param tuple1 The first tuple.
	 * @param tuple2 The tuple to compare against.
	 * @return {@code true} iff tuples have the same keys.
	 */
	public boolean peisk_isEqual(PeisTuple tuple1, PeisTuple tuple2);
	
	/**
	 * Converts a tuple error code to a printable string.
	 * @param error The error code to translate.
	 * @return The String representation of the error code.
	 */
	public String peisk_tuple_strerror(int error);
	
	/**
	 * Sets a value to a tuple in our own namespace if the tuple does not yet exist.
	 * Useful for providing default values to
	 * tuples which may be configured via the command line --set-tuple option.
	 * @param key The key of the tuple o set.
	 * @param value The value to set.
	 */
	public void peisk_setDefaultStringTuple(String key, String value);

	/**
	 * Sets a value to a tuple in our own namespace if the tuple does not yet exist.
	 * Useful for providing default values to
	 * tuples which may be configured via the command line --set-tuple option.
	 * @param key The key of the tuple o set.
	 * @param datalen The length of the data to be set.
	 * @param data The value to set.
	 * @param mimetype The mimetype of this tuple.
	 * @param encoding The Encoding of this tuple.
	 */
	public void peisk_setDefaultTuple(String key, int datalen, Pointer data, String mimetype, int encoding);

	/**
	 * Sets a default value to a tuple in our name space it it does not
	 *  yet exists. Also creates a corresponding meta tuple that points to
	 *  it. This allows for this meta tuple to be re-configured to point
	 *  somewhere else at a later timepoint.
	 *  
	 *   $key <- data  (if not already existing)
	 *   
	 *   mi-$key <- (meta SELF $key)
	 *   
	 *   Examples: Assuming that peiscam invokes defaultMetaTuple with arguments "position.x" and the data+len "42".
	 *   
	 *    ./peiscam
	 *    position.x = 0
	 *    position.mi-x = (meta SELF position.x)
	 *    
	 *    ./peiscam --peis-set-tuple position.x = 42
	 *    position.x = 42
	 *    position.mi-x = (meta SELF position.x)
	 *    
	 *    ./peiscam --peis-set-tuple position.x = 42  + configurator makes changes in my position.mi-x
	 *    position.x = 42
	 *    position.mi-x = (meta pippi pos.cartesian.x)
     * @param key The key of the tuple o set.
	 * @param datalen The length of the data to be set.
	 * @param data The value to set.
	 * @param mimetype The mimetype of this tuple.
	 * @param encoding The Encoding of this tuple.
	 */
	//ASK Mathias
	public void peisk_setDefaultMetaTuple(String key, int datalen, Pointer data, String mimetype, int encoding);

	
	/**
	 * Same as setDefaultMetaTuple but with String tuple.
	 * @param key The key of the tuple o set.
	 * @param datalen The length of the data to be set.
	 * @param data The value to set.
	 */
	//ASK Mathias
	public void peisk_setDefaultMetaStringTuple(String key, String data);

	
	/**
	 * Test if the named tuple exists in our own namespace.
	 * @param key The key to search for.
	 * @return {@code true} iff a tuple with the given key exists.
	 */
	public boolean peisk_tupleExists(String key);

	/**
	 * Convinience function for quickly deleting a tuple given its name
	 * and owner. This works by intersting a tuple which expires
	 * immediately. A callback functions can be aware of this by noticing
	 * the expiry field of the tuple which receives the value
	 * PEISK_TUPLE_EXPIRE_NOW in field 0.
	 * @param owner The owner of the tuple to delete.
	 * @param key The key of the tuple to delete.
	 */
	public void peisk_deleteTuple(int owner, String key);
	
	/**
	 * Test if a tuple is abstract (contains any wildcards).
	 * @param tuple The tuple to test.
	 * @return {@code true} if the tuple is abstract.
	 */
	public boolean peisk_tupleIsAbstract(PeisTuple tuple);

	
	/*
	 * META TUPLES
	 */
	
	/**
	 * Creates a subscription to the tuple M and to the tuple [M]
	 * referenced by the latest content of T continuously. Whenever
	 * T is changed the subscription to [M] changes also. Tuple M must
	 * have a fully qualified key and owner (i.e., only point to exactly
	 * one meta tuple) 
	 * @param M The meta tuple containing a reference to the tuple to subscribe to. 
	 * @return The subscription handle, {@code null} if the operation fails. 
	 */
	//public PeisSubscriberHandle peisk_subscribeIndirectTuple(PeisTuple M);
	public PeisSubscriberHandle peisk_subscribeIndirectlyByAbstract(PeisTuple M);
	
	/**
	 * Makes an indirect lookup and returns the tuple [M] referenced
	 * by the given abstract meta tuple M. This first makes a lookup of
	 * the concrete tuple references by M (called [M]). Secondly it makes
	 * a lookup of the concrete tuple pointed to by [M] and uses the
	 * special flags of M (isNew etc) for this lookup.
	 * Note that Meta tuples always point only to one specific tuple.
	 * @param M The abstract meta tuple to be looked up for obtaining a tuple to subscribe to.
	 * @return The subscription handle, {@code null} if the operation fails.
	 */
	//public PeisTuple peisk_findIndirectTuple(PeisTuple M);
	public PeisTuple peisk_getTupleIndirectlyByAbstract(PeisTuple M);
	
	/**
	 * Makes a lookup for latest value of meta tuple M and initializes
	 * abstract tuple A to point to tuple referenced by M. 
	 * @param M The reference meta tuple.
	 * @param A The abstract tuple to point to the tuple referenced by the meta tuple.
	 * @return Zero on success, error code otherwise
	 */
	//public int peisk_initIndirectTuple(PeisTuple M, PeisTuple A);
	
	/**
	 * Creates a subscription to the meta tuple given by metaKey and metaOwner.
	 * Neither metaKey nor metaOwner may contain wildcard.
	 * @param metaKey The key of the meta tuple to subscribe to.
	 * @param metaOwner The owner of the meta tuple to subscribe to.
	 * @return The subscription handle, {@code null} if the operation fails.
	 */
	//public PeisSubscriberHandle peisk_subscribeIndirect(String metaKey, int metaOwner);
	public PeisSubscriberHandle peisk_subscribeIndirectly(int metaOwner, String metaKey);
	
	/**
	 * Use the meta tuple given by (metaOwner,metaKey) to find a reference to a
	 * specific tuple. Returns this referenced tuple [metaOwner,metaKey] if found.
	 * Must previously have created a metaSubscription to metaKey and metaOwner.
	 * @param metaOwner The owner of the meta tuple. 
	 * @param metaKey The key of the meta tuple.
	 * @param flags Flags modifying the behaviour of internal getTuple call.
	 * @return Tuple [metaOwner,metaKey] if found, {@code null} otherwise.
	 */
	//public PeisTuple peisk_getIndirectTuple(String metaKey, int metaOwner, IntByReference len, PointerByReference ptr, int flags);
	public PeisTuple peisk_getTupleIndirectly(int metaOwner, String metaKey, int flags);
	
	/**
	 * Inserts value into the tuple referenced by the tuple (metaKey,metaOwner).
	 * Must already be subscribed to (metaKey,metaOwner) using at least a normal
	 * subscribe but also works with a metaSubscription.
	 * @param metaKey The key of the meta tuple.
	 * @param metaOwner The owner of the meta tuple.
	 * @param value The String value to set.
	 * @return Zero on success, error code otherwise.
	 */
	//public int peisk_setIndirectStringTuple(String metaKey, int metaOwner, String value);
	//DO EXCEPTION
	public int peisk_setStringTupleIndirectly(int metaOwner, String metaKey, String value);

	/**
	 * Inserts value into the tuple referenced by the tuple (metaKey,metaOwner).
	 * Must already be subscribed to (metaKey,metaOwner) using at least a normal
	 * subscribe but also works with a metaSubscription.
	 * @param metaKey The key of the meta tuple.
	 * @param metaOwner The owner of the meta tuple.
	 * @param value The String value to set.
	 * @return Zero on success, error code otherwise.
	 */ //ADDED
	//public int peisk_setIndirectStringTuple(String metaKey, int metaOwner, String value);
	//DO EXCEPTION
	public int peisk_setTupleIndirectly(int metaOwner, String metaKey, int datalen, Pointer value, String mimetype, int encoding);

	/**
	 * Returns true if the given meta tuple is currently pointing to a
	 * valid tuple.
	 * @param metaKey The key of the meta tuple.
	 * @param metaOwner The owner of the meta tuple.
	 * @return {@code true} if the given meta tuple is currently pointing to a
	 * valid tuple.
	 */
	//public boolean peisk_isIndirectTuple(String metaKey, int metaOwner);
	public boolean peisk_isMetaTuple(int metaOwner, String metaKey);
	
	/**
	 * Initializes a meta tuple to reasonable default values.
	 * @param metaOwner The owner of the meta tuple.
	 * @param metaKey The key of the meta tuple.
	 */
	public void peisk_declareMetaTuple(int metaOwner, String metaKey);
	
	/**
	 * Sets the meta tuple to point to given real tuple.
	 * @param metaOwner The owner of the meta tuple.
	 * @param metaKey The key of the meta tuple.
	 * @param realOwner The owner of the real tuple.
	 * @param realKey The key of the real tuple.
	 */
	public void peisk_setMetaTuple(int metaOwner, String metaKey, int realOwner, String realKey);
	
	/**
	 * Get the id of this peiskernel.
	 * @return The id of this peiskernel.
	 */
	public int peisk_peisid();
	
	/**
	 * Stops the running peis kernel.
	 */
	public void peisk_shutdown();
	
	/**
	 * Simplifying wrapper for creating and setting a tuple in local tuplespace,
	 * propagating it to all subscribers. Provides backwards
	 * compatibility with kernel G3 and earlier.
	 * @param key The key of the tuple to set.
	 * @param len The length of the data to set.
	 * @param data The data to write into the tuple.
	 * @param mimetype The mimetype of the tuple to set.
	 */ //ADDED
	public void peisk_setTuple(String key, int len, Pointer data, String mimetype, int encoding);
	

	/**
	 * Wrapper for creating and setting a tuple in a tuplespace belonging
	 * to some other peis. Propagation is done by the other peis if
	 * successful. Provides backwards compatibility with kernel G3 and
	 * earlier.
	 * @param key The key of the tuple to set.
	 * @param owner The owner of the tuple to set.
	 * @param len The length of the data.
	 * @param data The data to set.
	 * @param mimetype The mimetype of the tuple to set.
	 */
	public void peisk_setRemoteTuple(int owner, String key, int len, Pointer data, String mimetype, int encoding);

	
	/**
	 * Wrapper for creating and setting a tuple in a tuplespace belonging
	 * to some other peis and blocking until acknowledged. Propagation is done by the other peis if
	 * successful. Provides backwards compatibility with kernel G3 and
	 * earlier.
	 * @param key The key of the tuple to set.
	 * @param owner The owner of the tuple to set.
	 * @param len The length of the data.
	 * @param data The data to set.
	 * @param mimetype The mimetype of the tuple to set.
	 */
	public void peisk_setRemoteTupleBlocking(int owner, String key, int len, Pointer data, String mimetype, int encoding);

	
	/**
	 * Wrapper for creating and setting a tuple in a tuplespace belonging
	 * to some other peis. Propagation is done by the other peis if
	 * successful. Provides backwards compatibility with kernel G3 and
	 * earlier.
	 * @param key The key of the tuple to set.
	 * @param owner The owner of the tuple to set.
	 * @param len The length of the data.
	 * @param value The data to set.
	 */
	public void peisk_setRemoteStringTuple(int owner, String key, String value);

	
	/**
	 * Wrapper for creating and setting a tuple in a tuplespace belonging
	 * to some other peis. Propagation is done by the other peis if
	 * successful. Blocks until set operation has been acknowledged. Provides backwards compatibility with kernel G3 and
	 * earlier.
	 * @param key The key of the tuple to set.
	 * @param owner The owner of the tuple to set.
	 * @param len The length of the data.
	 * @param value The data to set.
	 */
	public void peisk_setRemoteStringTupleBlocking(int owner, String key, String value);
	
	
	/**
	 * Prints all peiskernel specific options.
	 * @param stream The stream to print to (e.g., stderr, ...)
	 * @param argc The number of command line arguments.
	 * @param args The command line arguments.
	 */
	public void peisk_printUsage(Pointer stream, short argc, String[] args);

	/**
	 * Performs one full event step for all connections.
	 */
	public void peisk_step();
	
	/**
	 * Add to list of hosts that will repeatedly be connected to.
	 * @param url The host to add.
	 */
	public void peisk_autoConnect(String url);
	
	/**
	 * Adds a new hook to the given port, fails (false) if port is
	 * invalid. Hooks are stacked with the most recently added one being
	 * the first  hook to call.
	 * @param port The port to hook to.
	 * @param hook The callback function.
	 * @return {@code true} iff the operation was successful.
	 */
	public boolean peisk_registerHook(int port, PeisHook hook);
	
	/**
	 * Frees the given hook at the given port number. Returns {@code true} if hook was unused.
	 * @param port The port on which the hook is registered.
	 * @param hook The hook to free.
	 * @return {@code true} if hook was unused.
	 */
	public boolean peisk_unregisterHook(int port, PeisHook hook);
	
	/**
	 * Register a hook to be called when a host is removed from the routing table (deleted from the ecology).
	 * @param fn The function to register.
	 * @param arg The arguments of the function (?)
	 */
	public void peisk_registerDeadhostHook(PeisDeadhostHook fn, Pointer arg);
	
	/**
	 * Hashes the given string with a simple hash function.
	 * @return The hash key.
	 */
	public int peisk_hashString(String string);

	/**
	 * Gives the current time in seconds since the EPOCH with integer
	 * precision. Uses the synchronized PEIS vector clock.
	 * @return The current time in seconds since the EPOCH.
	 */
	public int peisk_gettime(); 

	/**
	 * Gives the current time in seconds since the EPOCH with
	 * floating-point precision. Uses the synchronized PEIS vector clock.
	 * @return The current time in seconds since the EPOCH.
	 */
	public double peisk_gettimef();
	
	/**
	 * Gives the current time in seconds and microseconds since the EPOCH
	 * with two long int pointer arguments where the result is
	 * stored. Uses the synchronized PEIS vector clock.
	 * @param t0 Current time (seconds).
	 * @param t1 Current time (microseconds).
	 */
	public void peisk_gettime2(IntByReference t0, IntByReference t1);
	
	/**
	 * Gives the current time in seconds and microseconds since the EPOCH
	 * with two long int pointer arguments where the result is
	 * stored. Uses the system clock.
	 * @param t0 Current time (seconds).
	 * @param t1 Current time (microseconds).
	 */
	public void peisk_getrawtime2(IntByReference t0, IntByReference t1);

	/**
	 * Gives the current time in seconds since the EPOCH with
	 *  floating-point precision. Uses the native system clock.
	 * @return The current time in seconds since the EPOCH.
	 */
	public double peisk_getrawtimef();
	
	/**
	 * Attempts to setup a connection to the given URL. Sets up a
	 * "pending" connection. Returns -1 if failure can be detected
	 * immediately, otherwise connection number of pending
	 * connection. Accepts the different PEISK_CONNECT_FLAG_* flags.
	 * @param url The URL to connect to.
	 * @param flags The PEISK_CONNECT_FLAG_* flags.
	 * @return The connection number of the pending connection in case of
	 * success, {@code -1} otherwise.
	 */
	public int peisk_connect(String url, int flags);
	
	/**
	 * Basic package sending function, broadcast over whole network at
	 * given port number.
	 * @param port The port over which to broadcast.
	 * @param len The length of the data.
	 * @param data The data to broadcast.
	 * @return {@code true} on success.
	 */
	public boolean peisk_broadcast(int port, int len, Pointer data);
	
	/**
	 * Special version of broadcasting method, specify special state
	 * using the PEISK_BPACKAGE_* flags.
	 * @param port The port over which to broadcast.
	 * @param len The length of the data.
	 * @param data The data to broadcast.
	 * @param flags The flags indicating the special state.
	 * @return {@code true} on success.
	 */
	public boolean peisk_broadcastSpecial(int port, int len, Pointer data, int flags);
	
	/**
	 * Basic package sending function, broadcast over whole network at
	 * given port number. Send from a specific sender id.
	 * @param from The sender's ID.
	 * @param port The port over which to broadcast.
	 * @param len The length of the data.
	 * @param data The data to broadcast.
	 * @return {@code true} on success.
	 */
	public boolean peisk_broadcastFrom(int from, int port, int len, Pointer data);
	
	/**
	 * Special version of broadcasting method, specify special state
	 * using the PEISK_BPACKAGE_* flags. Send from a specific sender
	 * id.
	 * @param from The sender's ID.
	 * @param port The port over which to broadcast.
	 * @param len The length of the data.
	 * @param data The data to broadcast.
	 * @param flags The flags indicating the special state.
	 * @return {@code true} on success.
	 */
	public boolean peisk_broadcastSpecialFrom(int from, int port, int len, Pointer data, int flags);
	

	/**
	 * peis-to-peis communication by sending replies to earlier
	 * messages.
	 * @param port The port to send to.
	 * @param destination The destination peis.
	 * @param len The length of the data.
	 * @param data The data to send.
	 * @param flags Flags indicating the state.
	 * @return {@code true} on success.
	 */
	public boolean peisk_sendMessage(int port, int destination, int len, Pointer data, int flags);
	
	/**
	 * Special form of sendMessage allowing spoofed source
	 * address.
	 * @param from The sender's ID.
	 * @param port The port to send to.
	 * @param destination The destination peis.
	 * @param len The length of the data.
	 * @param data The data to send.
	 * @param flags Flags indicating the state.
	 * @return {@code true} on success.
	 */
	public boolean peisk_sendMessageFrom(int from, int port, int destination, int len, Pointer data, int flags);
	
	/**
	 * Blocks until and owner for the given keu (in any possible owner namespace has been found).
	 * Returns the ID of the found owner. 
	 * @param key  The key of the tuple whose owner we are looking for.
	 * @return the ID of the found owner.
	 */ //ADDED
	public int peisk_findOwner(String key);

	/**
	 * Prints a tuple in human readable format to the given buffer.
	 * @param buf  The string to which to print.
	 * @param len  The length of the string.
	 * @param tuple The tuple to print.
	 */
	public void peisk_snprintTuple(Pointer buf, int len, PeisTuple tuple);

		
	//-----------------------------------------------------------------------------------
	//PEISKernel service-related typedefs, functions and constants not mapped (for now).
	//-----------------------------------------------------------------------------------

	//-----------------------------------------------------------------------------------
	//The following seems really low level -- not mapping it for now.
	//-----------------------------------------------------------------------------------
	//void peisk_setSelectReadSignals(int *n,fd_set *read,fd_set *write,fd_set *excp);
	//void peisk_hexDump(void *data,int datalen);
	//char *peisk_getToken(FILE *);
	//int peisk_localTCPServerPort();
	//void peisk_updateTopology();
	//void peisk_sendTrace(int target);
	//-----------------------------------------------------------------------------------
}
