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
import java.util.Arrays;
import java.util.Date;
import java.util.HashSet;
import java.util.Set;
import java.util.logging.Level;
import java.util.logging.Logger;

import com.sun.jna.Memory;
import com.sun.jna.Native;
import com.sun.jna.NativeLibrary;
import com.sun.jna.Pointer;
import com.sun.jna.ptr.IntByReference;

import core.PeisJavaInterface.PeisSubscriberHandle;
import core.PeisJavaInterface.PeisTupleCallback;

/**
 * This class implements the {@link core.PeisJavaInterface} interface, providing static access to
 * the peiskernel functionality.  The class provides convenient multithread wrappers for the
 * peiskernel library.  It allows the programmer to access core peiskernel functionality at two
 * levels:
 * <ul>
 * <li><i>thread-safe, high-level access to peiskernel functionality</i>, e.g., initializing,
 * reading and writing tuples, and various convenience methods;</li>
 * <li><i>non-thread-safe access to low level peiskernel functionalities</i>, e.g.,
 * accessing the underlying peer-to-peer network or broadcasting messages to
 * known hosts.</i></li>
 * </ul>
 * The first level of functionality is provided through static methods of this class
 * of the form {@code peisjava_XXX(...)}.  The second level of access is provided through
 * the {@link #INTANCE} interface, and specifically through the methods {@code INTANCE.peisk_XXX(...)}.
 * If these latter non-thread-safe methods are invoked, they must be synchronized on the
 * {@link #peiskCriticalSection} object.
 * <br><br>
 * This class is intended to be used mostly through its high-level methods ({@code peisjava_XXX(...)}).
 * One-to-one and thread-safe access to the peiskernel is possible, although it should not be necessary
 * for most applications.  Access to the non-thread-safe methods is never necessary to harness
 * all the features of the peiskernel, but is maintained as it allows to implement a single-thread
 * peis in Java.
 *  
 * @author fpa
 *
 */

public class PeisJavaMT {
	
	private static final Logger logger = Logger.getLogger(PeisJavaMT.class.getPackage().getName());
	
	/**
	 * Flags determining whether tuple access should be blocking and/or filter old values.
	 */
	//(1<<)0 = filter old values, (1<<)1 = blocking
	public static enum FLAG { FILTER_OLD, BLOCKING };
	private static final FLAG[] NO_FLAGS = new FLAG[]{};
	
	/**
	 * Processes the flags
	 * @param flags
	 * @return the integer representation of the options flags
	 */
	private final static int processOptions(FLAG... flags) {
		
		final Set<FLAG> flagSet = new HashSet<FLAG>(Arrays.asList(flags));
		
		//Make sure we don't have duplicate flags
		if(flagSet.size() != flags.length) {
			logger.log(Level.WARNING, "Redundant flags");
		}
		
		int bitwise = 1;
		if (flagSet.contains(FLAG.FILTER_OLD))
			//bitwise = bitwise - 1;
			bitwise &= ~(1 << 0); //Unset bit 0
		if (flagSet.contains(FLAG.BLOCKING))
			//bitwise = bitwise + 2;
			bitwise |= (1 << 1); // set bit 1
		return bitwise;
	}
	
	
	/**
	 * Predefined encoding types for tuples.
	 */
	//-1 = unspecified, 0 = ASCII, 1 = binary
	public static enum ENCODING { ASCII, BINARY };
	
	/**
	 * Processes the encoding
	 * @param flags
	 * @return the integer representation of the encoding flags.
	 */
	private final static int processEncoding(ENCODING encoding) {
		final int enc;
		if(encoding.equals(ENCODING.ASCII)) {
			enc = 0;
		} else if(encoding.equals(ENCODING.BINARY)) {
			enc = 1;
		} else {
			enc = -1;
		}
		return enc;
	}

	
	/**
	 * Reference to PEISKernel library instance.  Use if you need to access the
	 * peisk_xxx() calls directly (otherwise just invoke the static methods
	 * of PEISJava). 
	 */
	public static PeisJavaInterface INSTANCE = (PeisJavaInterface)Native.loadLibrary("peiskernel",PeisJavaInterface.class);
	
	
	/**
	 * Object to synchronize against when using the peisk_XXX methods directly.
	 * This was set to the class so the methods can be made synchronized. While retaining
	 * backwards compability with methods that synchronize explicitly on this object.
	 */
	public static final Object peiskCriticalSection = PeisJavaMT.class;
	

	private static PeiskThread peiskThread = null;
	
	/**
	 * Get the error code for the last user called function.
	 * @return The error code for the last user called function.
	 */
	public static synchronized int peisjava_getTupleErrno() {
		return NativeLibrary.getInstance("peiskernel").getGlobalVariableAddress("peisk_tuples_errno").getInt(0);
	}
	
	static private String[] PEISJAVA_DEFAULT_ARGUMENTS = new String[]{}; 
	static private String[] peisjavaArguments = PEISJAVA_DEFAULT_ARGUMENTS;
	
	static private int peisjavaIntializationCount = 0; 
	
	/**
	 * @param args The command line arguments to pass to the underlying peisk_initialize() call.
	 */
	static public synchronized boolean peisjava_setarguments(String[] args) {
		
		if(args == null) {
			new Error("Peisjava arguments should not be null");
		}
		
		if(peisjavaIntializationCount > 0) {
			logger.log(Level.WARNING, "Trying to set peisjava arguments after initialization");
			return false;
		}
		
		if(peisjavaArguments != PEISJAVA_DEFAULT_ARGUMENTS) {
			logger.log(Level.WARNING, "Overriding old peisjava arguments before initializaion: '" + peisjavaArguments + "'");
		}
		
		peisjavaArguments = args;
		return true;
	}
	
	/**
	 * Initializes peiskernel using any appropriate command line options.
	 */
	public static synchronized void peisjava_initialize(String[] args) throws WrongPeisKernelVersionException {
		
		if(args == null) {
			args = PEISJAVA_DEFAULT_ARGUMENTS.clone();
		}
		
		peisjava_setarguments(args);
		peisjava_initialize();
	}
	
	
	/**
	 * Initializes the peiskernel if not already initialized 
	 * using any command line arguments previously specified by a call to the 
	 * {@link #peisjava_setarguments(String[])} function.
	 */
	public static synchronized void peisjava_initialize() throws WrongPeisKernelVersionException {
		
		if(peisjavaIntializationCount > 0) {
			//The PEIS kernel has already been initialized, 
			//increment the initialization counter and return.
			++peisjavaIntializationCount;
			return;
		} else {
			++peisjavaIntializationCount;
			//Check if the user wants help.
			if(Arrays.asList(peisjavaArguments).contains("--help") 	|| Arrays.asList(peisjavaArguments).contains("--h")) {
				peisjava_printUsage(peisjavaArguments);
				System.exit(0);
			}
			
			//Determine the name of the calling class and use it as the peis component name 
			String callingClass = null;
			try {
				throw new Exception("Who called me?"); 
			} catch( Exception e ) {
				callingClass = e.getStackTrace()[1].getClassName();
			}
			
			//Create a new command line argument array with the component name prepended
			//as the first argument to the PEIS kernel 
			String[] newArgs = new String[peisjavaArguments.length+1];
			
			newArgs[0] = callingClass;
			System.arraycopy(peisjavaArguments, 0, newArgs, 1, peisjavaArguments.length);
			System.out.println(Arrays.asList(newArgs));
			
			INSTANCE.peisk_initialize(new IntByReference(newArgs.length), newArgs);
			
			INSTANCE.peisk_setStringTuple("kernel.PeisJava.version", PeisJavaInterface.version);
			System.out.println("PeisJava version " + PeisJavaInterface.version);
			
			String peiskVersion = peisjava_getStringTuple("kernel.version", FLAG.BLOCKING);
			if (!peiskVersion.substring(0, peiskVersion.lastIndexOf('.')).equals(PeisJavaInterface.version.substring(0, peiskVersion.lastIndexOf('.')))) {
				throw new WrongPeisKernelVersionException(PeisJavaInterface.version, peiskVersion);
			}
			
			//Spawn the peiskmt thread which steps the native kernel
			peiskThread = new PeiskThread();
			peiskThread.start();
		}
	}
	
	/**
	 * Stops the running peis kernel.
	 */
	public static synchronized void peisjava_shutdown() {
		
		--peisjavaIntializationCount;
		
		if(peisjavaIntializationCount <= 0) {
			peiskThread.interrupt();
			
			//Wait for the worker thread to finish
			try {
				//Wait at most one second
				peiskThread.join(1000);
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
			peiskThread = null;			
			//Release the reference to the thread
			INSTANCE.peisk_shutdown();
		}
	}
	
	
	/**
	 * Get a tuple from the local tuplespace. The owner of the tuple must be the caller.  Note: this is a non-blocking read, and the same value can be read more than once.
	 * @param owner The owner of the tuple.
	 * @param key The fully qualified key of the tuple to get.
	 * @param flags Flags indicating whether call should be blocking and/or filter old values.
	 * @return The tuple matching the given key.
	 */
	public static synchronized PeisTuple peisjava_getTuple(int owner, String key, FLAG... flags) {
		return INSTANCE.peisk_getTuple(owner, key, processOptions(flags));
	}
	
	/**
	 * Get a tuple from the local tuplespace. The owner of the tuple must be the caller.  Note: this is a non-blocking read, and the same value can be read more than once.
	 * @param owner The owner of the tuple.
	 * @param key The fully qualified key of the tuple to get.
	 * @return The tuple matching the given key.
	 */
	public static PeisTuple peisjava_getTuple(int owner, String key) {
		return peisjava_getTuple(owner, key, NO_FLAGS);
	}
	
	
	
	/**
	 * Get the data in a tuple in the form of a String.
	 * @param name The name of the tuple.
	 * @param owner The owner of the tuple. 
	 * @param flags Flags indicating whether call should be blocking and/or filter old values. 
	 * @return A String containing the tuple data if the tuple was found, {@code null} otherwise.
	 */
	public static synchronized String peisjava_getStringTuple(int owner, String name, FLAG... flags) {
		final PeisTuple tup = peisjava_getTuple(owner, name, flags);
		if (tup != null && tup.getStringData() != null)
			return tup.getStringData();
		return null;
	}
	
	/**
	 * Get the data in a tuple in the form of a String, given the name of the tuple (which should be in local tuplespace).
	 * @param name The name of the tuple.
	 * @param flags Flags indicating whether call should be blocking and/or filter old values, defaults to a non-blocking read.
	 * @return A String containing the tuple data if the tuple was found, {@code null} otherwise. 
	 */
	public static String peisjava_getStringTuple(String name, FLAG... flags) {
		return peisjava_getStringTuple(peisjava_peisid(), name, flags);
	}
	
	/**
	 * Get the data in a tuple in the form of a String, given the name of the tuple (which should be in local tuplespace).
	 * Note: this is a non-blocking read, and the same value can be read more than once.
	 * @param name The name of the tuple.
	 * @return A String containing the tuple data if the tuple was found, {@code null} otherwise. 
	 */
	public static String peisjava_getStringTuple(String name) {
		return peisjava_getStringTuple(peisjava_peisid(), name, NO_FLAGS);
	}
	
	
	/**
	 * Get the data in a tuple in the form of a String.  Note: this is a non-blocking read, and the same value can be read more than once.
	 * @param name The name of the tuple.
	 * @param owner The owner of the tuple.
	 * @return A String containing the tuple data if the tuple was found, {@code null} otherwise. 
	 */
	public static String peisjava_getStringTuple(int owner, String name) {
		return peisjava_getStringTuple(owner, name, NO_FLAGS);
	}
	
	
	/**
	 * Get the data in a tuple in the form of a byte array.
	 * @param name The name of the tuple.
	 * @param owner The owner of the tuple.
	 * @param flags Flags indicating whether call should be blocking and/or filter old values.
	 * @return A byte array containing the tuple data if the tuple was found, {@code null} otherwise. 
	 */
	public static synchronized byte[] peisjava_getByteTuple(int owner, String name, FLAG... flags) {
		PeisTuple tup = peisjava_getTuple(owner, name, flags);
		if (tup != null && tup.getByteData() != null)
			return tup.getByteData();
		return null;
	}
	
	
	/**
	 * Get the data in a tuple in the form of a byte array.  The owner of the tuple must
	 * be the caller component.  Note: this is a non-blocking read, and the same value can be read more than once.
	 * @param name The name of the tuple.
	 * @return A byte array containing the tuple data if the tuple was found, {@code null} otherwise. 
	 */
	public static byte[] peisjava_getByteTuple(String name) {
		return peisjava_getByteTuple(peisjava_peisid(), name, NO_FLAGS);
	}
	
	/**
	 * Get the data in a tuple in the form of a byte array.  The owner of the tuple must
	 * be the caller component.
	 * @param name The name of the tuple.
	 * @param flags Flags indicating whether call should be blocking and/or filter old values.
	 * @return A byte array containing the tuple data if the tuple was found, {@code null} otherwise. 
	 */
	public static byte[] peisjava_getByteTuple(String name, FLAG... flags) {
		return peisjava_getByteTuple(peisjava_peisid(), name, flags);
	}
	
	/**
	 * Get the data in a tuple in the form of a byte array.  Note: this is a non-blocking read, and the same value can be read more than once.
	 * @param name The name of the tuple.
	 * @param owner The owner of the tuple.
	 * @return A byte array containing the tuple data if the tuple was found, {@code null} otherwise. 
	 */
	public static synchronized byte[] peisjava_getByteTuple(int owner, String name) {
		return peisjava_getByteTuple(owner, name, NO_FLAGS);
	}
	
	/**
	 * Factory method for creating a {@link PeisTuple} with a given fully qualified name.
	 * @param name The fully qualified name of the tuple.
	 * @return A new tuple.
	 */
	/*
	public static PeisTuple peisjava_createTuple(String name) {
		PeisTuple tuple = new PeisTuple();
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_initTuple(tuple);
		}
		if (PeisJavaMT.peisjava_setTupleName(tuple, name) != 0) return null;
		return tuple;
	}
	 */
	
	/**
	 * Initializes a tuple to reasonable default values for inserting
	 * into some tuplespace. Modify remaining fields before calling
	 * insertTuple or findTuple. Differs from peisk_initAbstractTuple in
	 * what the default values are.
	 * @param tup The tuple to initialize.
	 */
	public static synchronized void peisjava_initTuple(PeisTuple tup) {
		INSTANCE.peisk_initTuple(tup);
	}
	
	/**
	 * Initializes a tuple to reasonable default values for searching in
	 * local tuplespace. Modify remaining fields before calling
	 * insertTuple or findTuple. Differs from peisk_initTuple in what the
	 * default values are.
	 * @param tup The abstract tuple to be initialized.
	 */
	public static synchronized void peisjava_initAbstractTuple(PeisTuple tup) {
		INSTANCE.peisk_initAbstractTuple(tup);
	}
	
	/**
	 * Initializes a meta tuple to reasonable default values.
	 * @param metaOwner The owner of the meta tuple.
	 * @param metaKey The key of the meta tuple.
	 */
	public static synchronized void peisjava_declareMetaTuple(int metaOwner, String metaKey) {
		INSTANCE.peisk_declareMetaTuple(metaOwner, metaKey);
	}	
	
	
	/**
	 * Initializes a meta tuple to reasonable default values (in local tuple space).
	 * @param metaKey The key of the meta tuple.
	 */
	public static void peisjava_declareMetaTuple(String metaKey) {
		peisjava_declareMetaTuple(peisjava_peisid(), metaKey);
	}
	
	//	/**
	//	 * Factory method for creating an abstract {@link PeisTuple} with a given name.
	//	 * @param name The name of the tuple.
	//	 * @return A new abstract tuple.
	//	 */
	//	/*
	//	public static PeisTuple peisjava_createAbstractTuple(String name) {
	//		PeisTuple tuple = new PeisTuple();
	//		synchronized(peiskCriticalSection) {
	//			INSTANCE.peisk_initAbstractTuple(tuple);
	//		}
	//		if (PeisJavaMT.peisjava_setTupleName(tuple, name) != 0) return null;
	//		return tuple;
	//	}
	//	*/
	//	
	//	/**
	//	 * Factory method for creating an abstract {@link PeisTuple} with a given name
	//	 * and owner.
	//	 * @param name The name of the tuple.
	//	 * @param owner The owner of the tuple.
	//	 * @return A new abstract tuple.
	//	 */
	//	/*
	//	public static PeisTuple peisjava_createAbstractPeisTuple(String name, int owner) {
	//		PeisTuple tuple = new PeisTuple();
	//		synchronized(peiskCriticalSection) {
	//			INSTANCE.peisk_initAbstractTuple(tuple);
	//		}
	//		tuple.owner = owner;
	//		if (PeisJavaMT.peisjava_setTupleName(tuple, name) != 0) return null;
	//		return tuple;
	//	}
	//	*/
	
	
	/**
	 * Similar to {@link #peisk_compareTuples(PeisTuple tuple1, PeisTuple tuple2)} but treats wildcards
	 * differently. 
	 * @param tuple1 The first tuple.
	 * @param tuple2 The tuple to compare against.
	 * @return {@code true} if tuple1 is a generalization or equal to
	 * tuple2 (e.g., tuple1 has a wildcard or the same data as tuple2 in
	 * every field).
	 */
	public static synchronized boolean peisjava_isGeneralization(PeisTuple tuple1, PeisTuple tuple2) {
		return INSTANCE.peisk_isGeneralization(tuple1, tuple2);
	}
	
	
	/**
	 * Add to list of hosts that will repeatedly be connected to.
	 * @param url The host to add.
	 */
	public static synchronized void peisjava_autoConnect(String url) {
		INSTANCE.peisk_autoConnect(url);
	}
	
	
	/**
	 * Basic package sending function, broadcast over whole network at
	 * given port number. Send from a specific sender id.
	 * @param from The sender's ID.
	 * @param port The port over which to broadcast.
	 * @param len The length of the data.
	 * @param data The data to broadcast.
	 * @return {@code true} on success.
	 */
	public static synchronized boolean peisjava_broadcastFrom(int from, int port, int len, Pointer data) {
		return INSTANCE.peisk_broadcastFrom(from, port, len, data);
	}
	
	
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
	public static synchronized int peisjava_connect(String url, int flags) {
		return INSTANCE.peisk_connect(url, flags);
	}
	
	
	
	/**
	 * Creates a fresh resultSet that can be used for iterating over
	 * tuples, you must call peisk_resultSetNext before getting first
	 * value.
	 * @return The new result set (to pass to findTuples()).
	 */
	public static synchronized PeisTupleResultSet peisjava_createResultSet() {
		return INSTANCE.peisk_createResultSet();
	}
	
	
	/**
	 * Sets a value to a tuple in our own namespace if it has not
	 * yet been given a value. Useful for providing default values to
	 * tuples which may be configured via the command line --set-tuple option
	 * @param key The key of the tuple o set.
	 * @param value The value to set.
	 */
	public static synchronized void peisjava_setDefaultStringTuple(String key, String value) {
		INSTANCE.peisk_setDefaultStringTuple(key, value);
	}
	
	
	/**
	 * Deletes the given resultSet and frees memory.
	 * @param resultSet The result set to reset.
	 */
	public static synchronized void peisjava_deleteResultSet(PeisTupleResultSet resultSet) {
		INSTANCE.peisk_deleteResultSet(resultSet);
	}
	
	
	/**
	 * Convinience function for quickly deleting a tuple given its name
	 * and owner. This works by intersting a tuple which expires
	 * immediately. A callback functions can be aware of this by noticing
	 * the expiry field of the tuple which receives the value
	 * PEISK_TUPLE_EXPIRE_NOW in field 0.
	 * @param owner The owner of the tuple to delete.
	 * @param key The key of the tuple to delete.
	 */
	public static synchronized void peisjava_deleteTuple(int owner, String key) {
		INSTANCE.peisk_deleteTuple(owner, key);
	}
	
	
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
	public static synchronized PeisTuple peisjava_getTupleIndirectlyByAbstract(PeisTuple M) {
		return INSTANCE.peisk_getTupleIndirectlyByAbstract(M);
	}
	
	
	/**
	 * Looks up a tuple in the local tuplespace and locally cached
	 * tuples using the fully qualified owner and key only (no wildcards
	 * permitted).
	 * @param tuple The tuple to find.
	 * @return {@code null} if the tuple was not found. 
	 */
	public static synchronized PeisTuple peisjava_getTupleByAbstract(PeisTuple tuple) throws InvalidAbstractTupleUseException {
		if (tuple.getOwner() == -1 || tuple.getKey().contains("*")) {
			throw new InvalidAbstractTupleUseException(tuple.getKey(), tuple.getOwner());
		}
		return INSTANCE.peisk_getTupleByAbstract(tuple);
	}
	
	
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
	public static synchronized PeisTupleResultSet peisjava_getTuplesByAbstract(PeisTuple tuple) {
		PeisTupleResultSet result = INSTANCE.peisk_createResultSet();
		if (INSTANCE.peisk_getTuplesByAbstract(tuple, result) > 0) { 
			return result;
		} else {
			return null;
		}
	}
	
	
	/**
	 * Free a tuple (including the data field).
	 * @param tuple
	 */
	public static synchronized void peisjava_freeTuple(PeisTuple tuple) {
		INSTANCE.peisk_freeTuple(tuple);
	}
	
	
	/**
	 * Use the meta tuple given by (metaOwner,metaKey) to find a reference to a
	 * specific tuple. Returns this referenced tuple [metaOwner,metaKey] if found.
	 * Must previously have created a metaSubscription to metaKey and metaOwner.
	 * @param metaKey The key of the meta tuple.
	 * @param metaOwner The owner of the meta tuple.
	 * @param flags Flags indicating whether call should be blocking and/or filter old values, defaults to a non-blocking read. 
	 * @return Tuple [metaOwner,metaKey] if found, {@code null} otherwise.
	 */
	public static synchronized PeisTuple peisjava_getTupleIndirectly(int metaOwner, String metaKey, FLAG... flags) {
		return INSTANCE.peisk_getTupleIndirectly(metaOwner, metaKey, processOptions(flags));
	}
	
	/**
	 * Use the meta tuple given by (metaOwner,metaKey) to find a reference to a
	 * specific tuple. Returns this referenced tuple [metaOwner,metaKey] if found.
	 * Must previously have created a metaSubscription to metaKey and metaOwner.
	 * @param metaKey The key of the meta tuple.
	 * @param metaOwner The owner of the meta tuple. 
	 * @return Tuple [metaOwner,metaKey] if found, {@code null} otherwise.
	 */
	public static PeisTuple peisjava_getTupleIndirectly(int metaOwner, String metaKey) {
		return peisjava_getTupleIndirectly(metaOwner, metaKey, NO_FLAGS);
	}
	
	/**
	 * Gives the current time of the synchronized PEIS vector clock.
	 * @return The current time as a {@link Date}.
	 */
	public static synchronized Date peisjava_getdate() {
		final IntByReference t0 = new IntByReference(0);
		final IntByReference t1 = new IntByReference(0);
		INSTANCE.peisk_getrawtime2(t0, t1);
		
		return PeisJavaUtilities.getJavaDateFromPeisDate(t0.getValue(), t1.getValue());
	}
	
	/**
	 * Gives the current time in seconds since the EPOCH with integer
	 * precision. Uses the synchronized PEIS vector clock.
	 * @return The current time in seconds since the EPOCH.
	 * @see PeisJavaMT#peisjava_getdate()
	 */
	@Deprecated
	public static int peisjava_gettime() {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_gettime();
		}
	}


	/**
	 * Gives the current time in seconds and microseconds since the EPOCH
	 * with two long int pointer arguments where the result is
	 * stored. Uses the synchronized PEIS vector clock.
	 * @param t0 Current time (seconds).
	 * @param t1 Current time (microseconds).
	 * @see PeisJavaMT#peisjava_getdate()
	 */
	@Deprecated
	public static void peisjava_gettime2(IntByReference t0, IntByReference t1) {
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_gettime2(t0, t1);
		}
	}


	/**
	 * Gives the current time in seconds since the EPOCH with
	 * floating-point precision. Uses the synchronized PEIS vector clock.
	 * @return The current time in seconds since the EPOCH.
	 * @see PeisJavaMT#peisjava_getdate()
	 */
	@Deprecated
	public static double peisjava_gettimef() {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_gettimef();
		}
	}
	
	
	/**
	 * Hashes the given string with a simple hash function.
	 * @return The hash key.
	 * @deprecated Perhaps this function can be removed
	 * unless the peis kernel hash function has some special property or use.
	 * Besides, it is available through the {@link PeisJavaMT#INSTANCE.
	 */
	@Deprecated
	public static int peisjava_hashString(String string) {
		//Perhaps this function can be removed unless
		//the peis kernel hash function has some special property or use
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_hashString(string);
		}
	}
	

	
	
	/**
	 * Inserts a tuple into local or remote tuplespace. Tuples must be fully
	 * qualified and contain no wildcards.  It is advisable to create tuples with
	 * the {@link #peisjava_createPeisTuple(String)} factory method before inserting them
	 * with this method.
	 * @param tuple The tuple to insert.
	 * @return {@code true} if successful (NOTE: failure may be delayed
	 * if modifying tuples inside other components, such failures are not reported here).
	 */
	public static synchronized boolean peisjava_insertTuple(PeisTuple tuple) throws InvalidAbstractTupleUseException {
		/*if (tuple.getOwner() == -1 || tuple.getKey().contains("*"))*/
		if (tuple.isAbstract()) { 
			throw new InvalidAbstractTupleUseException(tuple.getKey(), tuple.getOwner());
		}
		return PeisJavaMT.INSTANCE.peisk_insertTuple(tuple);
	}
	
	
	/**
	 * Inserts a tuple into local or remote tuplespace and BLOCKS until completion. Tuples must be fully
	 * qualified and contain no wildcards.  It is advisable to create tuples with
	 * the {@link #peisjava_createPeisTuple(String)} factory method before inserting them
	 * with this method.
	 * @param tuple The tuple to insert.
	 * @return {@code true} if successful (NOTE: failure may be delayed
	 * if modifying tuples inside other components, such failures are not reported here).
	 */
	public static synchronized boolean peisjava_insertTupleBlocking(PeisTuple tuple) throws InvalidAbstractTupleUseException {
		/*if (tuple.getOwner() == -1 || tuple.getKey().contains("*"))*/
		if (tuple.isAbstract()) {
			throw new InvalidAbstractTupleUseException(tuple.getKey(), tuple.getOwner());
		}
		return PeisJavaMT.INSTANCE.peisk_insertTupleBlocking(tuple);
	}
	
	
	/**
	 * Returns true if the given meta tuple is currently pointing to a
	 * valid tuple.
	 * @param metaKey The key of the meta tuple.
	 * @param metaOwner The owner of the meta tuple.
	 * @return {@code true} if the given meta tuple is currently pointing to a
	 * valid tuple.
	 */
	public static synchronized boolean peisjava_isMetaTuple(int metaOwner, String metaKey) {
		return INSTANCE.peisk_isMetaTuple(metaOwner, metaKey);
	}
	
	
	/**
	 * Ascertain if the peiskernel is running.
	 * @return {@code true} after initialization until we have performed a shutdown.
	 */
	public static synchronized boolean peisjava_isRunning() {
		return (peiskThread != null) && INSTANCE.peisk_isRunning();
	}
	
	
	/**
	 * Tests if anyone is currently subscribed to tuples comparable to the
	 * given search string. For instance, if we have a subscription for A.*.C and we test with
	 * partial key A.B.* then the result is _true_.
	 * @param key The search key to employ (admits wildcards).
	 * @return {@code true} iff there is at least one subscriber to a tuple whose
	 * key matches the given key.
	 */
	public static synchronized boolean peisjava_isSubscribed(String key) {
		PeisTuple prototype = new PeisTuple();
		peisjava_initAbstractTuple(prototype);
		peisjava_setTupleName(prototype, key);
//		INSTANCE.peisk_initAbstractTuple(prototype);
//		INSTANCE.peisk_setTupleName(prototype, key);
		return PeisJavaMT.INSTANCE.peisk_hasSubscriberByAbstract(prototype);
	}
	
	/**
	 * Get the id of this peiskernel.
	 * @return The id of this peiskernel.
	 */
	public static synchronized int peisjava_peisid() {
		return INSTANCE.peisk_peisid();
	}
	
	
	/**
	 * Prints all peiskernel specific options.
	 * @param args The command line parameters.
	 */
	public static synchronized void peisjava_printUsage(String[] args) {
		try { throw new Exception("Who called me?"); }
		catch( Exception e ) {
			String callingClass = e.getStackTrace()[1].getClassName();
			int argc = args.length+1;
			String[] newArgs = new String[argc];
			newArgs[0] = callingClass;
			for (int i = 0; i < args.length; i++) {
				newArgs[i+1] = args[i];
			}
			INSTANCE.peisk_printUsage(NativeLibrary.getInstance("c").getFunction("stderr").getPointer(0), (short)argc, newArgs);
		}		
	}
	
	
	
	/**
	 * Register a callback on a tuple.  The callback routine must be implemented in
	 * the method {@link CallbackObject#callback(PeisTuple tuple)} of a class which extends
	 * the abstract class {@link CallbackObject}. 
	 * @param key The key of the tuple to register the callback to.
	 * @param owner The owner of the tuple to register the callback to.
	 * @param co A class extending the {@link CallbackObject} abstract class.
	 * @return The {@link CallbackObject} if the a callback object was successfully instantiated
	 * and queued for registration, {@code null} otherwise. 
	 */
	public static PeisTupleCallback peisjava_registerTupleCallback(int owner, String key, CallbackObject co) {
		return peiskThread.registerCallback(key, owner, co);
	}
	
	
	/**
	 * Unregister a tuple callback given the {@link PeisJavaInterface.PeisTupleCallback} object returned
	 * by {@link PeisJavaMT#peisjava_registerTupleCallback(String, int, String)}.
	 * @param tupleCallback The callback to unregister.
	 */
	//TODO: DO EXCEPTION
	public static void peisjava_unregisterTupleCallback(PeisTupleCallback tupleCallback) {
		peiskThread.unregisterCallback(tupleCallback);
	}
	
	
	/**
	 * Forces a resend of subscription and reload all subscribed
	 * tuples.
	 * @param subscriber The handle of the subscription to reload.
	 * @return Zero on success, error number otherwise.
	 */
	public static synchronized int peisjava_reloadSubscription(PeisSubscriberHandle subscriber) {
		return PeisJavaMT.INSTANCE.peisk_reloadSubscription(subscriber);
	}
	
	/**
	 * Resets an resultSet to point back to _before_ first entry, you
	 * must call peisk_resultSetNext before getting first value.
	 * @param resultSet The result set to reset.
	 */
	public static synchronized void peisjava_resultSetFirst(PeisTupleResultSet resultSet) {
		INSTANCE.peisk_resultSetFirst(resultSet);
	}
	
	
	/**
	 * Inserts value into the tuple referenced by the tuple (metaKey,metaOwner).
	 * Must already be subscribed to (metaKey,metaOwner) using at least a normal
	 * subscribe but also works with a metaSubscription.
	 * @param metaKey The key of the meta tuple.
	 * @param metaOwner The owner of the meta tuple.
	 * @param value The String value to set.
	 * @return Zero on success, error code otherwise.
	 */
	//DO EXCEPTION
	public static synchronized void peisjava_setStringTupleIndirectly(int metaOwner, String metaKey, String value) throws PeisTupleException {
		final int ret = INSTANCE.peisk_setStringTupleIndirectly(metaOwner, metaKey, value);
		if (ret != 0) {
			throw new PeisTupleException(ret, INSTANCE);
		}
	}
	
	
	/**
	 * A wrapper function for setting tuples whose value is a simple
	 * String.  The owner must be the caller. 
	 * @param name The name of the tuple to set.
	 * @param value The value to set.  A String value of "nil" is set if
	 * this parameter is {@code null}.
	 */
	public static synchronized void peisjava_setStringTuple(String name, String value) {
		if (value == null) {
			INSTANCE.peisk_setStringTuple(name,"nil");
		} else {
			INSTANCE.peisk_setStringTuple(name,value);
		}
	}
	
	/**
	 * Simplifying wrapper for creating and setting a tuple in local tuplespace,
	 * propagating it to all subscribers. Provides backwards
	 * compatibility with kernel G3 and earlier.
	 * @param key The key of the tuple to set.
	 * @param len The length of the data to set.
	 * @param value The data to write into the tuple.
	 */
	public static synchronized void peisjava_setTuple(String key, byte[] value, String mimetype, ENCODING encoding) {
		Memory m = new Memory(value.length);
		m.write(0, value, 0, value.length);
		
		final int enc = processEncoding(encoding);
		INSTANCE.peisk_setTuple(key, value.length, m, mimetype, enc);
	}
	
	
	
	/**
	 * Set the name of a tuple.
	 * @param tup The tuple to modify. 
	 * @param fullname The new name for this tuple.
	 * @return 0 iff the operation was successful.
	 */
	public static synchronized boolean peisjava_setTupleName(PeisTuple tup, String fullname) {
		return (INSTANCE.peisk_setTupleName(tup, fullname) == 0);
	}
	
	
	/**
	 * Wrapper for subscribing to tuples with given key from given owner
	 * (or -1 for wildcard on owner).
	 * @param key The key of the tuple to subscribe.
	 * @param owner The owner of the tuple to subscribe (-1 for wildcard).
	 */
	public static synchronized PeisSubscriberHandle peisjava_subscribe(int owner, String key) {
		return PeisJavaMT.INSTANCE.peisk_subscribe(owner, key);
	}
	
	
	
	/**
	 * Subscribe using given abstract tuple as prototype.
	 * @param tuple The abstract tuple to use as prototype. 
	 * @return The handle for this tuple subscription if success, {@code null} otherwise.
	 */
	public static synchronized PeisSubscriberHandle peisjava_subscribe(PeisTuple tuple) {
		return INSTANCE.peisk_subscribeByAbstract(tuple);
	}
	
	/**
	 * Subscribe using given abstract tuple as prototype.
	 * @param tuple The abstract tuple to use as prototype. 
	 * @return The handle for this tuple subscription if success, {@code null} otherwise.
	 */
	//TODO: ASK MATHIAS
	@Deprecated
	public static synchronized PeisSubscriberHandle peisjava_subscribeByAbstract(PeisTuple tuple) {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_subscribeByAbstract(tuple);
		}
	}
	
	
	
	/**
	 * Creates a subscription to the meta tuple (in local tuple space) given by metaKey and metaOwner.
	 * Neither metaKey nor metaOwner may contain wildcard.
	 * @param metaKey The key of the meta tuple to subscribe to.
	 * @param metaOwner The owner of the meta tuple to subscribe to.
	 * @return The subscription handle, {@code null} if the operation fails.
	 */	
	public static synchronized PeisSubscriberHandle peisjava_subscribeIndirectly(String metaKey) {
		return INSTANCE.peisk_subscribeIndirectly(INSTANCE.peisk_peisid(), metaKey);	
	}
	
	
	/**
	 * Creates a subscription to the meta tuple given by metaKey and metaOwner.
	 * Neither metaKey nor metaOwner may contain wildcard.
	 * @param metaKey The key of the meta tuple to subscribe to.
	 * @param metaOwner The owner of the meta tuple to subscribe to.
	 * @return The subscription handle, {@code null} if the operation fails.
	 */	
	public static synchronized PeisSubscriberHandle peisjava_subscribeIndirectly(int metaOwner, String metaKey) {
		return INSTANCE.peisk_subscribeIndirectly(metaOwner, metaKey);	
	}
	
	
	
	/**
	 * Creates a subscription to the tuple M and to the tuple [M]
	 * referenced by the latest content of T continuously. Whenever
	 * T is changed the subscription to [M] changes also. Tuple M must
	 * have a fully qualified key and owner (i.e., only point to exactly
	 * one meta tuple) 
	 * @param M The meta tuple containing a reference to the tuple to subscribe to. 
	 * @return The subscription handle, {@code null} if the operation fails. 
	 */
	public static PeisSubscriberHandle peisjava_subscribeIndirectlyByAbstract(PeisTuple M) {
		return INSTANCE.peisk_subscribeIndirectlyByAbstract(M);
	}
	
	
	/**
	 * Test if the named tuple exists in our own namespace.
	 * @param key The key to search for.
	 * @return {@code true} iff a tuple with the given key exists.
	 */
	public static synchronized boolean peisjava_tupleExists(String key) {
		return INSTANCE.peisk_tupleExists(key);
	}
	
	
	/**
	 * Converts a tuple error code to a printable string.
	 * @param error The error code to translate.
	 * @return The String representation of the error code.
	 */
	public static synchronized String peisjava_tuple_strerror(int error) {
		return INSTANCE.peisk_tuple_strerror(error);
	}
	
	
	/**
	 * Unsubscribe to tuples.
	 * @param handle The handle of the subscription to unsunbscribe.
	 */
	public static synchronized boolean peisjava_unsubscribe(PeisSubscriberHandle handle) {
		return !INSTANCE.peisk_unsubscribe(handle);
	}
	
	
	/**
	 * Sets the meta tuple to point to given real tuple.
	 * @param metaOwner The owner of the meta tuple.
	 * @param metaKey The key of the meta tuple.
	 * @param realOwner The owner of the real tuple.
	 * @param realKey The key of the real tuple.
	 */
	public static synchronized void peisjava_setMetaTuple(int metaOwner, String metaKey, int realOwner, String realKey) {
		INSTANCE.peisk_setMetaTuple(metaOwner, metaKey, realOwner, realKey);
	}
	
	
	/**
	 * Appends the given data to the matching tuple in the tuple space. The argument
	 * tuple is here an ABSTRACT TUPLE that identifies which tuple should be appended to. Thus, you would
	 * normally have an empty data field in the given abstractTuple. If
	 * there are wildcards in the owner and/or key parts of the abstract
	 * tuple, the data will be appended to each such tuple. If there is a
	 * concrete sequence number in the given tuple, then the append will
	 * only be performed if the two tuples have the same sequence number.
	 * @param abstractTuple
	 * @param difflen
	 * @param diff
	 */
	public static synchronized void peisjava_appendTupleByAbstract(PeisTuple abstractTuple,	int difflen, Pointer diff) {
		INSTANCE.peisk_appendTupleByAbstract(abstractTuple,	difflen, diff);
	} 
	
	
	/**
	 * Appends the given data to the matching tuple in the tuple space. The argument
	 * tuple is here an ABSTRACT TUPLE that identifies which tuple should be appended to. Thus, you would
	 * normally have an empty data field in the given abstractTuple. If
	 * there are wildcards in the owner and/or key parts of the abstract
	 * tuple, the data will be appended to each such tuple. If there is a
	 * concrete sequence number in the given tuple, then the append will
	 * only be performed if the two tuples have the same sequence number.
	 * @param abstractTuple
	 * @param difflen
	 * @param diff
	 */
	public static synchronized void peisjava_appendStringTupleByAbstract(PeisTuple abstractTuple, String diff) {
		INSTANCE.peisk_appendStringTupleByAbstract(abstractTuple, diff);
	}
	
	
	/**
	 * Creates a default META tuple in our own tuple space if the tuple does not yet exist.
	 * Useful for providing default values to
	 * configuration parameters.
	 * @param key The key of the tuple o set.
	 * @param data The value to set.
	 */
	public static synchronized void peisjava_setDefaultMetaStringTuple(String key, String data) {
		INSTANCE.peisk_setDefaultMetaStringTuple(key, data);
	}
	
	
	/**
	 * Sets a default value to a tuple in our tuple space it it does not
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
	public static synchronized void peisjava_setDefaultMetaTuple(String key, int datalen, Pointer data, String mimetype, ENCODING encoding) {
		int enc = processEncoding(encoding);
		INSTANCE.peisk_setDefaultMetaTuple(key, datalen, data, mimetype, enc);
	}
	
	/**
	 * Sets a value to a tuple in our own namespace if the tuple does not yet exist.
	 * Useful for providing default values to
	 * tuples which may be configured via the command line --set-tuple option.
	 * @param key The key of the tuple o set.
	 * @param datalen The length of the data to be set.
	 * @param data The value to set.
	 */
	public static synchronized void peisjava_setDefaultTuple(String key, int datalen, Pointer data, String mimetype, ENCODING encoding) {
		final int enc = processEncoding(encoding);
		INSTANCE.peisk_setDefaultTuple(key, datalen, data, mimetype, enc);
	}
	
	
	/**
	 * Wrapper to test if anyone is currently subscribed to the given (full) key in our local tuplespace. Wrapper for backwards compatability.
	 * @param key A (full) key in our local tuplespace.
	 * @return Whether anyone is subscribed to a tuple with the given key.
	 */
	public static synchronized boolean peisjava_hasSubscriber(String key) {
		return INSTANCE.peisk_hasSubscriber(key);
	}
	
	
	/**
	 * Register a callback on a tuple.  The callback routine must be implemented in
	 * the method {@link CallbackObject#callback(PeisTuple tuple)} of a class which extends
	 * the abstract class {@link CallbackObject}. 
	 * @param tuple A tuple prototype describing the tuple to register against.
	 * @param co A class extending the {@link CallbackObject} abstract class.
	 * @return The {@link CallbackObject} if the a callback object was successfully instantiated
	 * and queued for registration, {@code null} otherwise. 
	 */
	public static PeisTupleCallback peisjava_registerTupleCallbackByAbstract(PeisTuple tuple, CallbackObject co) {
		return peiskThread.registerCallback(tuple.getKey(), tuple.owner, co);
	}
	
	
	
	/**
	 * Register a callback on a tuple.  Invoked when matching tuples are deleted. The callback routine must be implemented in
	 * the method {@link CallbackObject#callback(PeisTuple tuple)} of a class which extends
	 * the abstract class {@link CallbackObject}. 
	 * @param key The key of the tuple to register the callback to.
	 * @param owner The owner of the tuple to register the callback to.
	 * @param co A class extending the {@link CallbackObject} abstract class.
	 * @return The {@link CallbackObject} if the a callback object was successfully instantiated
	 * and queued for registration, {@code null} otherwise. 
	 */
	public static PeisTupleCallback peisjava_registerTupleDeletedCallback(int owner, String key, CallbackObject co) {
		return peiskThread.registerDeletedCallback(key, owner, co);
	}
	
	
	
	/**
	 * Register a callback on a tuple.  Invoked when matching tuples are deleted. The callback routine must be implemented in
	 * the method {@link CallbackObject#callback(PeisTuple tuple)} of a class which extends
	 * the abstract class {@link CallbackObject}. 
	 * @param tuple A tuple prototype describing the tuple to register against.
	 * @param co A class extending the {@link CallbackObject} abstract class.
	 * @return The {@link CallbackObject} if the a callback object was successfully instantiated
	 * and queued for registration, {@code null} otherwise. 
	 */
	public static PeisTupleCallback peisjava_registerTupleDeletedCallbackByAbstract(PeisTuple tuple, CallbackObject co) {
		return peiskThread.registerDeletedCallback(tuple.getKey(), tuple.owner, co);
	}
	
	
	/**
	 * Blocks until and owner for the given key (in any possible owner tuplespace has been found).
	 * Returns the ID of the found owner. 
	 * @param key The key of a tuple in the owner's tuplespace.
	 * @return the ID of the found owner.
	 */
	public static synchronized int peisjava_findOwner(String key) {
		return INSTANCE.peisk_findOwner(key);
	}
	
	
	/**
	 * Wrapper for creating and setting a tuple in a tuplespace belonging
	 * to some other peis. Propagation is done by the other peis if
	 * successful. Provides backwards compatibility with kernel G3 and
	 * earlier.
	 * @param key The key of the tuple to set.
	 * @param owner The owner of the tuple to set.
	 * @param value The data to set.
	 */
	public static synchronized void peisjava_setRemoteStringTuple(int owner, String key, String value) {
		INSTANCE.peisk_setRemoteStringTuple(owner, key, value);
	}
	
	
	/**
	 * Wrapper for creating and setting a tuple in a tuplespace belonging
	 * to some other peis. Blocks until set operation have been acknowledged. Propagation is done by the other peis if
	 * successful. Provides backwards compatibility with kernel G3 and
	 * earlier.
	 * @param key The key of the tuple to set.
	 * @param owner The owner of the tuple to set.
	 * @param value The data to set.
	 */
	public static synchronized void peisjava_setRemoteStringTupleBlocking(int owner, String key, String value) {
		INSTANCE.peisk_setRemoteStringTupleBlocking(owner, key, value);
	}
	

	
	/**
	 * Inserts value into the tuple referenced by the tuple (metaKey,metaOwner).
	 * Must already be subscribed to (metaKey,metaOwner) using at least a normal
	 * subscribe but also works with a metaSubscription.
	 * @param metaKey The key of the meta tuple.
	 * @param metaOwner The owner of the meta tuple.
	 * @param value The value to set.
	 * @param mimetype The mimetype of the tuple. 
	 * @return {@code true} iff success.
	 */
	//ADDED
	//DO EXCEPTION
	public static synchronized void peisjava_setTupleIndirectly(int metaOwner, String metaKey, byte[] value, String mimetype, ENCODING encoding) throws PeisTupleException {
		Memory m = new Memory(value.length);
		m.write(0, value, 0, value.length);
		
		int enc = processEncoding(encoding);

		int ret = INSTANCE.peisk_setTupleIndirectly(metaOwner, metaKey, value.length, m, mimetype, enc);
		if (ret != 0) throw new PeisTupleException(ret, INSTANCE);
	}
	
	
	
	/**
	 * Get all tuples matching the given owner/key pair.
	 * @param owner The owner of the sought tuples.
	 * @param key The keyname of the tuples.
	 * @return A result set containing the found tuples (null if none are found).
	 */
	public static synchronized PeisTupleResultSet peisjava_getTuples(int owner, String key) {
		PeisTupleResultSet result = INSTANCE.peisk_createResultSet();
		if (INSTANCE.peisk_getTuples(owner, key, result) > 0) {
			return result;
		}
		return null;
	}
	
	
	/**
	 * Get all tuples matching the given owner/key pair.
	 * @param owner The owner of the sought tuples.
	 * @param key The keyname of the tuples.
	 * @return A result set containing the found tuples (null if none are found).
	 */
	public static PeisTupleResultSet peisjava_getTuples(int owner, String key, FLAG flag) {
		
		for(;;) {
			PeisTupleResultSet result = PeisJavaMT.peisjava_getTuples(owner, key);
			
			if(result == null && flag.equals(FLAG.BLOCKING)) {
				//Retry if the result is null and the call is blocking
				
				synchronized (peiskCriticalSection) {
					//TODO: I don't think a lock is necessary here
					//(This also prevents the kernel from being stepped)
					INSTANCE.peisk_waitOneCycle(10000);
				}
				continue;
			}
			
			return result;
		}
	}
	
	
	/**
	 * Wrapper for creating and setting a tuple in a tuplespace belonging
	 * to some other peis. Propagation is done by the other peis if
	 * successful. Provides backwards compatibility with kernel G3 and
	 * earlier.
	 * @param owner The owner of the tuple to set.
	 * @param key The key of the tuple to set.
	 * @param data The data to set.
	 * @param mimetype The mimetype of the tuple to set.
	 * @param encoding Whether the data is ASCII or binary.
	 */ //ADDED
	public static void peisjava_setRemoteTuple(int owner, String key, byte[] data, String mimetype, ENCODING encoding) {
		Memory m = new Memory(data.length);
		m.write(0, data, 0, data.length);
		
		final int enc = processEncoding(encoding);
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_setRemoteTuple(owner, key, data.length, m, mimetype, enc);
		}
	}
	
	
	/**
	 * Wrapper for creating and setting a tuple in a tuplespace belonging
	 * to some other peis. Blocks until set operation has been acknowledged. Propagation is done by the other peis if
	 * successful. Provides backwards compatibility with kernel G3 and
	 * earlier.
	 * @param owner The owner of the tuple to set.
	 * @param key The key of the tuple to set.
	 * @param data The data to set.
	 * @param mimetype The mimetype of the tuple to set.
	 * @param encoding Whether the data is ASCII or binary.
	 */
	public static void peisjava_setRemoteTupleBlocking(int owner, String key, byte[] data, String mimetype, ENCODING encoding) {
		Memory m = new Memory(data.length);
		m.write(0, data, 0, data.length);
		
		final int enc = processEncoding(encoding);
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_setRemoteTupleBlocking(owner, key, data.length, m, mimetype, enc);
		}
	}
	
	/**
	 * Sets the user defined timestamp (ts_user) for all comming tuples
	 * that are created. This is for backwards compatability but also
	 * convinience when many tuples are created.
	 * @param Date the time
	 */
	public static synchronized void peisjava_tsUser(Date tsUser) {
		int[] ts = new int[2];
		PeisJavaUtilities.setPeisDateFromJavaDate(tsUser, ts);
		INSTANCE.peisk_tsUser(ts[0], ts[1]);
	}
	
	/**
	 * Clone a tuple (including the data field). The user of this
	 * function is responsible for freeing the memory eventually, see
	 * peisk_freeTuple.
	 * @param tuple The tuple to clone.
	 * @return The new tuple.
	 */
	public static synchronized PeisTuple peisjava_cloneTuple(PeisTuple tuple) {
		return INSTANCE.peisk_cloneTuple(tuple);
	}
	
}
