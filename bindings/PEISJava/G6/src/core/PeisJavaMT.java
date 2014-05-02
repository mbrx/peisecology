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
import java.util.HashMap;
import java.util.Vector;

import com.sun.jna.Memory;
import com.sun.jna.Native;
import com.sun.jna.NativeLibrary;
import com.sun.jna.Pointer;
import com.sun.jna.ptr.IntByReference;
import com.sun.jna.ptr.LongByReference;

import core.PeisJavaInterface.PeisCallbackHandle;
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

	
	/**
	 * Flags determining whether tuple access should be blocking and/or filter old values.
	 */
	//0 = filter old values, 1 = blocking
	public static enum FLAG { FILTER_OLD, BLOCKING };
	
	/**
	 * Predefined encoding types for tuples.
	 */
	//-1 = unspecified, 0 = ASCII, 1 = binary
	public static enum ENCODING { ASCII, BINARY };
	
	/**
	 * Reference to PEISKernel library instance.  Use if you need to access the
	 * peisk_xxx() calls directly (otherwise just invoke the static methods
	 * of PEISJava). 
	 */
	public static PeisJavaInterface INSTANCE = (PeisJavaInterface)Native.loadLibrary("peiskernel",PeisJavaInterface.class);

	private static int peisjava_tuple_errno = 0;
	
	//private static boolean MULTIPLE_INSTANCE = true;
	
	/**
	 * Object to synchronize against when using the peisk_XXX methods directly.
	 */
	public static Object peiskCriticalSection = new Object();
	
	/**
	 * Shuts down the peiskernel upon deallocation.
	 */
	public void finalize() {
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_shutdown();
		}
		
	}

	private static class PeiskThread extends Thread {
		
		//Callback queue
		private Vector<Callback> pendingCallbacks = null;
		private Vector<Callback> pendingDeletedCallbacks = null;
		private HashMap<PeisTupleCallback,PeisCallbackHandle> callbacks = null;
		private Vector<PeisTupleCallback> toRemoveCallbacks = null;
		
		private int frequencyHz = 100;
		
		private Object callbackQueueCriticalSection = new Object();
		private Object deletedCallbackQueueCriticalSection = new Object();
		
		private class Callback {
			public int owner;
			public String key;
			public PeisTupleCallback tupleCallback;
			public Callback(int o, String k, PeisTupleCallback tcb) {
				owner = o;
				key = k;
				tupleCallback = tcb;
			}
			public String toString() {
				return ("Callback <" + key + "," + owner + "," + tupleCallback.getClass().getName() + ">");
			}
		}
		
		@SuppressWarnings("unchecked")
		private PeisTupleCallback registerCallbackOld(String key, int owner, String callbackObjectClass) {
			Class<PeisTupleCallback> co = null;
			try {
			    //co = (Class<PeisTupleCallback>)Class.forName(callbackObjectClass);
			    //The above does not work if PeisJava is instantiated as an extension...
			    //below is the solution (see http://www.javageeks.com/Papers/ClassForName/ClassForName.pdf)
			    ClassLoader cl = this.getContextClassLoader();
			    co = (Class<PeisTupleCallback>)cl.loadClass(callbackObjectClass);
			} catch (ClassNotFoundException e) {
				e.printStackTrace();
			}
			synchronized(callbackQueueCriticalSection) {
				if (co != null) {
					if (pendingCallbacks == null) pendingCallbacks = new Vector<Callback>();
					try {
						final PeisTupleCallback coi = co.newInstance();
						if (coi != null) {
							/*
							PeisTupleCallback tupleCallback = new PeisTupleCallback() {
								public void callback(PeisTuple tuple, Pointer userdata) {
									coi.callbackMethod();
								}
							};*/
							//Callback cb = new Callback(owner,key,coi,tupleCallback);
							Callback cb = new Callback(owner,key,coi);
							pendingCallbacks.add(cb);
							return coi;
						}
					} catch (InstantiationException e) {
						e.printStackTrace();
					} catch (IllegalAccessException e) {
						e.printStackTrace();
					}
				}
			}
			return null;
		}
		
		private PeisTupleCallback registerCallback(String key, int owner, PeisTupleCallback coi) {
			if (coi != null) {
				synchronized(callbackQueueCriticalSection) {
					if (pendingCallbacks == null) pendingCallbacks = new Vector<Callback>();
					Callback cb = new Callback(owner,key,coi);
					pendingCallbacks.add(cb);
					return coi;
				}	
			}
			return null;
		}
		
		private PeisTupleCallback registerDeletedCallback(String key, int owner, PeisTupleCallback coi) {
			if (coi != null) {
				synchronized(deletedCallbackQueueCriticalSection) {
					if (pendingDeletedCallbacks == null) pendingDeletedCallbacks = new Vector<Callback>();
					Callback cb = new Callback(owner,key,coi);
					pendingDeletedCallbacks.add(cb);
					return coi;
				}	
			}
			return null;
		}
		
		@Override
		public void run() {
			super.run();
			while(PeisJavaMT.INSTANCE.peisk_isRunning()) {
				//INSTANCE.peisk_wait(1/frequencyHz*10000);
				//INSTANCE.peisk_waitOneCycle(1/frequencyHz*1000000);
				try { Thread.sleep(1000/frequencyHz); } catch (InterruptedException e) {}
				//See if there are any callbacks to register...
				synchronized(callbackQueueCriticalSection) {
					if (pendingCallbacks != null) {
						Vector<Callback> added = new Vector<Callback>();
						for (Callback cb : pendingCallbacks) {
							PeisCallbackHandle hndl = PeisJavaMT.INSTANCE.peisk_registerTupleCallback(cb.owner, cb.key, null, cb.tupleCallback);
							System.out.println("REGISTERED (M) " + cb);
							added.add(cb);
							if (callbacks == null)
								callbacks = new HashMap<PeisTupleCallback,PeisCallbackHandle>();
							callbacks.put(cb.tupleCallback,hndl);
						}
						for (Callback cb : added) {
							pendingCallbacks.remove(cb);
						}
					}
				}
				
				synchronized(deletedCallbackQueueCriticalSection) {
					if (pendingDeletedCallbacks != null) {
						Vector<Callback> added = new Vector<Callback>();
						for (Callback cb : pendingDeletedCallbacks) {
							PeisCallbackHandle hndl = PeisJavaMT.INSTANCE.peisk_registerTupleDeletedCallback(cb.owner, cb.key, null, cb.tupleCallback);
							System.out.println("REGISTERED (D) " + cb);
							added.add(cb);
							if (callbacks == null)
								callbacks = new HashMap<PeisTupleCallback,PeisCallbackHandle>();
							callbacks.put(cb.tupleCallback,hndl);
						}
						for (Callback cb : added) {
							pendingDeletedCallbacks.remove(cb);
						}
					}
				}

				//See if there are any callbacks to unregister...
				if (toRemoveCallbacks != null) {
					for (PeisTupleCallback tupleCallback : toRemoveCallbacks) {
						PeisCallbackHandle hndl = callbacks.get(tupleCallback);
						synchronized(peiskCriticalSection) {
							int ret = INSTANCE.peisk_unregisterTupleCallback(hndl);
							if (ret != 0)
								try {
									throw new PeisTupleException(ret, INSTANCE);
								} catch (PeisTupleException e) {
									// TODO Auto-generated catch block
									e.printStackTrace();
									System.exit(ret);
								}
						}
						callbacks.remove(tupleCallback);
					}
				}
				synchronized(peiskCriticalSection) {
					INSTANCE.peisk_step();
					peisjava_tuple_errno = NativeLibrary.getInstance("peiskernel").getGlobalVariableAddress("peisk_tuples_errno").getInt(0);
				}
			}
			PeisJavaMT.INSTANCE.peisk_shutdown();
		}

		private void unregisterCallback(PeisTupleCallback tupleCallback) {
			if (toRemoveCallbacks == null) toRemoveCallbacks = new Vector<PeisTupleCallback>();
			toRemoveCallbacks.add(tupleCallback);
		}

	}
	
	private static PeiskThread peiskThread = null;
	
	/**
	 * Get the error code for the last user called function.
	 * @return The error code for the last user called function.
	 */
	public static int peisjava_getTupleErrno() { return peisjava_tuple_errno; }
	
	
	/**
	 * Get a tuple from the local tuplespace. The owner of the tuple must be the caller.  Note: this is a non-blocking read, and the same value can be read more than once.
	 * @param owner The owner of the tuple.
	 * @param key The fully qualified key of the tuple to get.
	 * @return The tuple matching the given key.
	 */
	public static PeisTuple peisjava_getTuple(int owner, String key) {
		synchronized(peiskCriticalSection) {
			PeisTuple ret = INSTANCE.peisk_getTuple(owner, key, 1);
			return ret;
		}
	}
	
	/**
	 * Get a tuple from the local tuplespace. The owner of the tuple must be the caller.  Note: this is a non-blocking read, and the same value can be read more than once.
	 * @param owner The owner of the tuple.
	 * @param key The fully qualified key of the tuple to get.
	 * @param flags Flags indicating whether call should be blocking and/or filter old values.
	 * @return The tuple matching the given key.
	 */
	public static PeisTuple peisjava_getTuple(int owner, String key, FLAG... flags) {
		synchronized(peiskCriticalSection) {
			PeisTuple ret = INSTANCE.peisk_getTuple(owner, key, processOptions(flags));
			return ret;
		}
	}
	
	/**
	 * Get the data in a tuple in the form of a String, given the name of the tuple (which should be in local tuplespace).
	 * Note: this is a non-blocking read, and the same value can be read more than once.
	 * @param name The name of the tuple.
	 * @return A String containing the tuple data if the tuple was found, {@code null} otherwise. 
	 */
	public static String peisjava_getStringTuple(String name) {
    	PeisTuple tup = null;
    	synchronized(peiskCriticalSection) {
    		if (INSTANCE.peisk_tupleExists(name))
    			tup = INSTANCE.peisk_getTuple(INSTANCE.peisk_peisid(), name, 1);
    	}
    	if (tup != null && tup.getStringData() != null)
    		return tup.getStringData();
		return null;
	}
	
	/**
	 * Get the data in a tuple in the form of a String, given the name of the tuple (which should be in local tuplespace).
	 * @param name The name of the tuple.
	 * @param flags Flags indicating whether call should be blocking and/or filter old values.
	 * @return A String containing the tuple data if the tuple was found, {@code null} otherwise. 
	 */
	public static String peisjava_getStringTuple(String name, FLAG... flags) {
    	PeisTuple tup = null;
    	synchronized(peiskCriticalSection) {
    		if (INSTANCE.peisk_tupleExists(name)) {
    			tup = INSTANCE.peisk_getTuple(INSTANCE.peisk_peisid(), name, processOptions(flags));
    		}
    	}
    	if (tup != null && tup.getStringData() != null)
    		return tup.getStringData();
		return null;
	}

		
	/**
	 * Get the data in a tuple in the form of a String.  Note: this is a non-blocking read, and the same value can be read more than once.
	 * @param name The name of the tuple.
	 * @param owner The owner of the tuple.
	 * @return A String containing the tuple data if the tuple was found, {@code null} otherwise. 
	 */
	public static String peisjava_getStringTuple(int owner, String name) {
    	PeisTuple tup = null;
    	synchronized(peiskCriticalSection) {
    		tup = INSTANCE.peisk_getTuple(owner, name, 1);
    	}
    	if (tup != null && tup.getStringData() != null)
    		return tup.getStringData();
		return null;
	}
	
	/**
	 * Get the data in a tuple in the form of a String.
	 * @param name The name of the tuple.
	 * @param owner The owner of the tuple. 
	 * @param flags Flags indicating whether call should be blocking and/or filter old values. 
	 * @return A String containing the tuple data if the tuple was found, {@code null} otherwise.
	 */
	public static String peisjava_getStringTuple(int owner, String name, FLAG... flags) {
    	PeisTuple tup = null;
    	synchronized(peiskCriticalSection) {
   			tup = INSTANCE.peisk_getTuple(owner, name, processOptions(flags));
    	}
    	if (tup != null && tup.getStringData() != null)
    		return tup.getStringData();
		return null;
	}

	/**
	 * Get the data in a tuple in the form of a byte array.  The owner of the tuple must
	 * be the caller component.  Note: this is a non-blocking read, and the same value can be read more than once.
	 * @param name The name of the tuple.
	 * @return A byte array containing the tuple data if the tuple was found, {@code null} otherwise. 
	 */
	public static byte[] peisjava_getByteTuple(String name) {
    	PeisTuple tup = null;
    	synchronized(peiskCriticalSection) {
      		if (INSTANCE.peisk_tupleExists(name))
      			tup = INSTANCE.peisk_getTuple(INSTANCE.peisk_peisid(), name, 1);
    	}
    	if (tup != null && tup.getByteData() != null)
    		return tup.getByteData();
		return null;
	}
	
	/**
	 * Get the data in a tuple in the form of a byte array.  The owner of the tuple must
	 * be the caller component.
	 * @param name The name of the tuple.
	 * @param flags Flags indicating whether call should be blocking and/or filter old values.
	 * @return A byte array containing the tuple data if the tuple was found, {@code null} otherwise. 
	 */
	public static byte[] peisjava_getByteTuple(String name, FLAG... flags) {
    	PeisTuple tup = null;
    	synchronized(peiskCriticalSection) {
      		if (INSTANCE.peisk_tupleExists(name)) {
      			tup = INSTANCE.peisk_getTuple(INSTANCE.peisk_peisid(), name, processOptions(flags));
      		}
    	}
    	if (tup != null && tup.getByteData() != null)
    		return tup.getByteData();
		return null;
	}
	
	/**
	 * Get the data in a tuple in the form of a byte array.  Note: this is a non-blocking read, and the same value can be read more than once.
	 * @param name The name of the tuple.
	 * @param owner The owner of the tuple.
	 * @return A byte array containing the tuple data if the tuple was found, {@code null} otherwise. 
	 */
	public static byte[] peisjava_getByteTuple(int owner, String name) {
    	PeisTuple tup = null;
    	synchronized(peiskCriticalSection) {
    		tup = INSTANCE.peisk_getTuple(owner, name, 1);
    	}
    	if (tup != null && tup.getByteData() != null)
    		return tup.getByteData();
		return null;
	}
	
	/**
	 * Get the data in a tuple in the form of a byte array.
	 * @param name The name of the tuple.
	 * @param owner The owner of the tuple.
	 * @param flags Flags indicating whether call should be blocking and/or filter old values.
	 * @return A byte array containing the tuple data if the tuple was found, {@code null} otherwise. 
	 */
	public static byte[] peisjava_getByteTuple(int owner, String name, FLAG... flags) {
    	PeisTuple tup = null;
    	synchronized(peiskCriticalSection) {
    		tup = INSTANCE.peisk_getTuple(owner, name, processOptions(flags));
    	}
    	if (tup != null && tup.getByteData() != null)
    		return tup.getByteData();
		return null;
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
	public static void peisjava_initTuple(PeisTuple tup) {
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_initTuple(tup);
		}
	}
	
	/**
	 * Initializes a tuple to reasonable default values for searching in
	 * local tuplespace. Modify remaining fields before calling
	 * insertTuple or findTuple. Differs from peisk_initTuple in what the
	 * default values are.
	 * @param tup The abstract tuple to be initialized.
	 */
	public static void peisjava_initAbstractTuple(PeisTuple tup) {
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_initAbstractTuple(tup);
		}
	}
	
	/**
	 * Initializes a meta tuple to reasonable default values.
	 * @param metaOwner The owner of the meta tuple.
	 * @param metaKey The key of the meta tuple.
	 */
	public static void peisjava_declareMetaTuple(int metaOwner, String metaKey) {
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_declareMetaTuple(metaOwner, metaKey);
		}
	}	

	
	/**
	 * Initializes a meta tuple to reasonable default values (in local tuple space).
	 * @param metaKey The key of the meta tuple.
	 */
	public static void peisjava_declareMetaTuple(String metaKey) {
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_declareMetaTuple(INSTANCE.peisk_peisid(), metaKey);
		}
	}
	
	/**
	 * Factory method for creating an abstract {@link PeisTuple} with a given name.
	 * @param name The name of the tuple.
	 * @return A new abstract tuple.
	 */
	/*
	public static PeisTuple peisjava_createAbstractTuple(String name) {
		PeisTuple tuple = new PeisTuple();
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_initAbstractTuple(tuple);
		}
		if (PeisJavaMT.peisjava_setTupleName(tuple, name) != 0) return null;
		return tuple;
	}
	*/
	
	/**
	 * Factory method for creating an abstract {@link PeisTuple} with a given name
	 * and owner.
	 * @param name The name of the tuple.
	 * @param owner The owner of the tuple.
	 * @return A new abstract tuple.
	 */
	/*
	public static PeisTuple peisjava_createAbstractPeisTuple(String name, int owner) {
		PeisTuple tuple = new PeisTuple();
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_initAbstractTuple(tuple);
		}
		tuple.owner = owner;
		if (PeisJavaMT.peisjava_setTupleName(tuple, name) != 0) return null;
		return tuple;
	}
	*/

	
	/**
	 * Similar to {@link #peisk_compareTuples(PeisTuple tuple1, PeisTuple tuple2)} but treats wildcards
	 * differently. 
	 * @param tuple1 The first tuple.
	 * @param tuple2 The tuple to compare against.
	 * @return {@code true} if tuple1 is a generalization or equal to
	 * tuple2 (e.g., tuple1 has a wildcard or the same data as tuple2 in
	 * every field).
	 */
	public static boolean peisjava_isGeneralization(PeisTuple tuple1, PeisTuple tuple2) {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_isGeneralization(tuple1, tuple2);
		}
	}
	

	/**
	 * Add to list of hosts that will repeatedly be connected to.
	 * @param url The host to add.
	 */
	public static void peisjava_autoConnect(String url) {
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_autoConnect(url);
		}
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
	public static boolean peisjava_broadcastFrom(int from, int port, int len, Pointer data) {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_broadcastFrom(from, port, len, data);
		}
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
	public static int peisjava_connect(String url, int flags) {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_connect(url, flags);
		}
	}



	/**
	 * Creates a fresh resultSet that can be used for iterating over
	 * tuples, you must call peisk_resultSetNext before getting first
	 * value.
	 * @return The new result set (to pass to findTuples()).
	 */
	public static PeisTupleResultSet peisjava_createResultSet() {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_createResultSet();
		}
	}


	/**
	 * Sets a value to a tuple in our own namespace if it has not
	 * yet been given a value. Useful for providing default values to
	 * tuples which may be configured via the command line --set-tuple option
	 * @param key The key of the tuple o set.
	 * @param value The value to set.
	 */
	public static void peisjava_setDefaultStringTuple(String key, String value) {
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_setDefaultStringTuple(key, value);
		}
		
	}


	/**
	 * Deletes the given resultSet and frees memory.
	 * @param resultSet The result set to reset.
	 */
	public static void peisjava_deleteResultSet(PeisTupleResultSet resultSet) {
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_deleteResultSet(resultSet);
		}
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
	public static void peisjava_deleteTuple(int owner, String key) {
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_deleteTuple(owner, key);
		}
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
	public static PeisTuple peisjava_getTupleIndirectlyByAbstract(PeisTuple M) {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_getTupleIndirectlyByAbstract(M);
		}
	}


	/**
	 * Looks up a tuple in the local tuplespace and locally cached
	 * tuples using the fully qualified owner and key only (no wildcards
	 * permitted).
	 * @param tuple The tuple to find.
	 * @return {@code null} if the tuple was not found. 
	 */
	public static PeisTuple peisjava_getTupleByAbstract(PeisTuple tuple) throws InvalidAbstractTupleUseException {
		if (tuple.getOwner() == -1 || tuple.getKey().contains("*")) {
			throw new InvalidAbstractTupleUseException(tuple.getKey(), tuple.getOwner());
		}
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_getTupleByAbstract(tuple);
		}
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
	public static PeisTupleResultSet peisjava_getTuplesByAbstract(PeisTuple tuple) {
		synchronized(peiskCriticalSection) {
			PeisTupleResultSet result = INSTANCE.peisk_createResultSet();
			if (INSTANCE.peisk_getTuplesByAbstract(tuple, result) > 0) return result;
			return null;
		}
	}


	/**
	 * Free a tuple (including the data field).
	 * @param tuple
	 */
	public static void peisjava_freeTuple(PeisTuple tuple) {
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_freeTuple(tuple);
		}
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
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_getTupleIndirectly(metaOwner, metaKey, 1);
		}
	}


	/**
	 * Use the meta tuple given by (metaOwner,metaKey) to find a reference to a
	 * specific tuple. Returns this referenced tuple [metaOwner,metaKey] if found.
	 * Must previously have created a metaSubscription to metaKey and metaOwner.
	 * @param metaKey The key of the meta tuple.
	 * @param metaOwner The owner of the meta tuple.
	 * @param flags Flags indicating whether call should be blocking and/or filter old values. 
	 * @return Tuple [metaOwner,metaKey] if found, {@code null} otherwise.
	 */
	public static PeisTuple peisjava_getTupleIndirectly(int metaOwner, String metaKey, FLAG... flags) {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_getTupleIndirectly(metaOwner, metaKey, processOptions(flags));
		}
	}

	
	/**
	 * Gives the current time in seconds since the EPOCH with integer
	 * precision. Uses the synchronized PEIS vector clock.
	 * @return The current time in seconds since the EPOCH.
	 */
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
	 */
	public static void peisjava_gettime2(IntByReference t0, IntByReference t1) {
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_gettime2(t0, t1);
		}
	}


	/**
	 * Gives the current time in seconds since the EPOCH with
	 * floating-point precision. Uses the synchronized PEIS vector clock.
	 * @return The current time in seconds since the EPOCH.
	 */
	public static double peisjava_gettimef() {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_gettimef();
		}
	}
	


	/**
	 * Hashes the given string with a simple hash function.
	 * @return The hash key.
	 */
	public static int peisjava_hashString(String string) {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_hashString(string);
		}
	}
	

	/**
	 * Initializes peiskernel using any appropriate command line options.
	 * @param args The command line arguments to pass to the underlying peisk_initialize() call.
	 */
	public static void peisjava_initialize(String[] args) throws WrongPeisKernelVersionException {
		boolean shareThread = false;
		//System.out.println(">>>>>>>>>>>>>>>> " + Thread.currentThread().getName() + ": " + Thread.currentThread().getId());
		if (args == null) args = new String[0];
		for (String arg : args)
			if (arg.equals("--help") || arg.equals("-h")) {
				peisjava_printUsage(args);
				System.exit(0);
			}

		/*
		if (peiskThread != null) try { throw new Exception("peiskmt: Error, attempting to start multiple peiskernels in the same process."); }
		catch ( Exception e ) {
			System.out.println(e.getMessage());
		}
		*/
				
		if (peiskThread != null) shareThread = true;

		try { throw new Exception("Who called me?"); }
	      catch( Exception e ) {
	    	  String callingClass = e.getStackTrace()[1].getClassName();
	    	  IntByReference argc = new IntByReference(args.length+1);
	    	  String[] newArgs = new String[argc.getValue()];
	    	  newArgs[0] = callingClass;
	    	  for (int i = 0; i < args.length; i++) {
	    		  newArgs[i+1] = args[i];
	    	  }
	    	  INSTANCE.peisk_initialize(argc,newArgs);
	    	  INSTANCE.peisk_setStringTuple("kernel.PeisJava.version", INSTANCE.version);
	    	  System.out.println("PeisJava version " + INSTANCE.version);
	    	  String peiskVersion = peisjava_getStringTuple("kernel.version", FLAG.BLOCKING);
	    	  if (!peiskVersion.substring(0, peiskVersion.lastIndexOf('.')).equals(INSTANCE.version.substring(0, peiskVersion.lastIndexOf('.'))))
	    		  throw new WrongPeisKernelVersionException(INSTANCE.version, peiskVersion);
	    		  
	    	  //Spawn the peiskmt thread which steps @100Hz
	    	  if (!shareThread) {
	    		  peiskThread = new PeiskThread();
	    		  peiskThread.start();
	    	  }
	          //Write the do-quit tuple (deprecated because it is in kernel.do-quit exists by default)
	    	  //PeisJavaMT.peisjava_setStringTuple("do-quit",null);
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
	public static boolean peisjava_insertTuple(PeisTuple tuple) throws InvalidAbstractTupleUseException {
		synchronized(peiskCriticalSection) {
			/*if (tuple.getOwner() == -1 || tuple.getKey().contains("*"))*/
			if (tuple.isAbstract()) throw new InvalidAbstractTupleUseException(tuple.getKey(), tuple.getOwner());
			return ((PeisJavaInterface)PeisJavaMT.INSTANCE).peisk_insertTuple(tuple);
		}
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
	public static boolean peisjava_insertTupleBlocking(PeisTuple tuple) throws InvalidAbstractTupleUseException {
		synchronized(peiskCriticalSection) {
			/*if (tuple.getOwner() == -1 || tuple.getKey().contains("*"))*/
			if (tuple.isAbstract()) throw new InvalidAbstractTupleUseException(tuple.getKey(), tuple.getOwner());
			return ((PeisJavaInterface)PeisJavaMT.INSTANCE).peisk_insertTupleBlocking(tuple);
		}
	}


	/**
	 * Returns true if the given meta tuple is currently pointing to a
	 * valid tuple.
	 * @param metaKey The key of the meta tuple.
	 * @param metaOwner The owner of the meta tuple.
	 * @return {@code true} if the given meta tuple is currently pointing to a
	 * valid tuple.
	 */
	public static boolean peisjava_isMetaTuple(int metaOwner, String metaKey) {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_isMetaTuple(metaOwner, metaKey);
		}
	}


	/**
	 * Ascertain if the peiskernel is running.
	 * @return {@code true} after initialization until we have performed a shutdown.
	 */
	public static boolean peisjava_isRunning() {
		return INSTANCE.peisk_isRunning();
	}
	

	/**
	 * Tests if anyone is currently subscribed to tuples comparable to the
	 * given search string. For instance, if we have a subscription for A.*.C and we test with
	 * partial key A.B.* then the result is _true_.
	 * @param key The search key to employ (admits wildcards).
	 * @return {@code true} iff there is at least one subscriber to a tuple whose
	 * key matches the given key.
	 */
	public static boolean peisjava_isSubscribed(String key) {
		PeisTuple prototype = new PeisTuple();
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_initAbstractTuple(prototype);
			INSTANCE.peisk_setTupleName(prototype, key);
			return ((PeisJavaInterface)PeisJavaMT.INSTANCE).peisk_hasSubscriberByAbstract(prototype);
		}
	}
	


	/**
	 * Get the id of this peiskernel.
	 * @return The id of this peiskernel.
	 */
	public static int peisjava_peisid() {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_peisid();
		}
	}


	/**
	 * Prints all peiskernel specific options.
	 * @param args The command line parameters.
	 */
	public static void peisjava_printUsage(String[] args) {
		try { throw new Exception("Who called me?"); }
	      catch( Exception e ) {
	    	  String callingClass = e.getStackTrace()[1].getClassName();
	    	  int argc = args.length+1;
	    	  String[] newArgs = new String[argc];
	    	  newArgs[0] = callingClass;
	    	  for (int i = 0; i < args.length; i++) {
	    		  newArgs[i+1] = args[i];
	    	  }
	    	  synchronized(peiskCriticalSection) {
	    		  INSTANCE.peisk_printUsage(NativeLibrary.getInstance("c").getFunction("stderr").getPointer(0), (short)argc, newArgs);
	    	  }
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
	//DO EXCEPTION
	public static void peisjava_unregisterTupleCallback(PeisTupleCallback tupleCallback) {
		peiskThread.unregisterCallback(tupleCallback);
	}


	/**
	 * Forces a resend of subscription and reload all subscribed
	 * tuples.
	 * @param subscriber The handle of the subscription to reload.
	 * @return Zero on success, error number otherwise.
	 */
	public static int peisjava_reloadSubscription(PeisSubscriberHandle subscriber) {
		synchronized(peiskCriticalSection) {
			return ((PeisJavaInterface)PeisJavaMT.INSTANCE).peisk_reloadSubscription(subscriber);
		}
	}



	/**
	 * Resets an resultSet to point back to _before_ first entry, you
	 * must call peisk_resultSetNext before getting first value.
	 * @param resultSet The result set to reset.
	 */
	public static void peisjava_resultSetFirst(PeisTupleResultSet resultSet) {
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_resultSetFirst(resultSet);
		}
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
	public static void peisjava_setStringTupleIndirectly(int metaOwner, String metaKey, String value) throws PeisTupleException {
		synchronized(peiskCriticalSection) {
			int ret = INSTANCE.peisk_setStringTupleIndirectly(metaOwner, metaKey, value);
			if (ret != 0) throw new PeisTupleException(ret, INSTANCE); 
		}
	}


	/**
	 * A wrapper function for setting tuples whose value is a simple
	 * String.  The owner must be the caller. 
	 * @param name The name of the tuple to set.
	 * @param value The value to set.  A String value of "nil" is set if
	 * this parameter is {@code null}.
	 */
	public static void peisjava_setStringTuple(String name, String value) {
		synchronized(peiskCriticalSection) {
			if (value == null) INSTANCE.peisk_setStringTuple(name,"nil");
			else INSTANCE.peisk_setStringTuple(name,value);
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
	public static void peisjava_setTuple(String key, byte[] value, String mimetype, ENCODING encoding) {
		Memory m = new Memory(value.length);
		m.write(0, value, 0, value.length);
		synchronized(peiskCriticalSection) {
			int enc = -1;
			if (encoding.equals(ENCODING.ASCII)) enc = 0;
			else if (encoding.equals(ENCODING.BINARY)) enc = 1;
			INSTANCE.peisk_setTuple(key, value.length, m, mimetype, enc);
		}
	}



	/**
	 * Set the nameof a tuple.
	 * @param tup The tuple to modify. 
	 * @param fullname The new name for this tuple.
	 * @return 0 iff the operation was successful.
	 */
	public static boolean peisjava_setTupleName(PeisTuple tup, String fullname) {
		synchronized(peiskCriticalSection) {
			return (INSTANCE.peisk_setTupleName(tup, fullname) == 0);
		}
	}
	


	/**
	 * Stops the running peis kernel.
	 */
	public static void peisjava_shutdown() {
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_shutdown();
		}
	}



	/**
	 * Wrapper for subscribing to tuples with given key from given owner
	 * (or -1 for wildcard on owner).
	 * @param key The key of the tuple to subscribe.
	 * @param owner The owner of the tuple to subscribe (-1 for wildcard).
	 */
	public static PeisSubscriberHandle peisjava_subscribe(int owner, String key) {
		synchronized(peiskCriticalSection) {
			return ((PeisJavaInterface)PeisJavaMT.INSTANCE).peisk_subscribe(owner, key);
		}
		
	}
	

	
	/**
	 * Subscribe using given abstract tuple as prototype.
	 * @param tuple The abstract tuple to use as prototype. 
	 * @return The handle for this tuple subscription if success, {@code null} otherwise.
	 */
	public static PeisSubscriberHandle peisjava_subscribe(PeisTuple tuple) {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_subscribeByAbstract(tuple);
		}
	}

	/**
	 * Subscribe using given abstract tuple as prototype.
	 * @param tuple The abstract tuple to use as prototype. 
	 * @return The handle for this tuple subscription if success, {@code null} otherwise.
	 */
	//ASK MATHIAS
	@Deprecated
	public static PeisSubscriberHandle peisjava_subscribeByAbstract(PeisTuple tuple) {
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
	public static PeisSubscriberHandle peisjava_subscribeIndirectly(String metaKey) {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_subscribeIndirectly(INSTANCE.peisk_peisid(), metaKey);	
		}
	}
	
	
	/**
	 * Creates a subscription to the meta tuple given by metaKey and metaOwner.
	 * Neither metaKey nor metaOwner may contain wildcard.
	 * @param metaKey The key of the meta tuple to subscribe to.
	 * @param metaOwner The owner of the meta tuple to subscribe to.
	 * @return The subscription handle, {@code null} if the operation fails.
	 */	
	public static PeisSubscriberHandle peisjava_subscribeIndirectly(int metaOwner, String metaKey) {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_subscribeIndirectly(metaOwner, metaKey);	
		}
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
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_subscribeIndirectlyByAbstract(M);
		}
	}
	

	/**
	 * Test if the named tuple exists in our own namespace.
	 * @param key The key to search for.
	 * @return {@code true} iff a tuple with the given key exists.
	 */
	public static boolean peisjava_tupleExists(String key) {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_tupleExists(key);
		}
	}
	

	/**
	 * Converts a tuple error code to a printable string.
	 * @param error The error code to translate.
	 * @return The String representation of the error code.
	 */
	public static String peisjava_tuple_strerror(int error) {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_tuple_strerror(error);
		}
	}


	/**
	 * Unsubscribe to tuples.
	 * @param handle The handle of the subscription to unsunbscribe.
	 */
	public static boolean peisjava_unsubscribe(PeisSubscriberHandle handle) {
		synchronized(peiskCriticalSection) {
			return !INSTANCE.peisk_unsubscribe(handle);
		}
	}


	/**
	 * Sets the meta tuple to point to given real tuple.
	 * @param metaOwner The owner of the meta tuple.
	 * @param metaKey The key of the meta tuple.
	 * @param realOwner The owner of the real tuple.
	 * @param realKey The key of the real tuple.
	 */
	public static void peisjava_setMetaTuple(int metaOwner, String metaKey, int realOwner, String realKey) {
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_setMetaTuple(metaOwner, metaKey, realOwner, realKey);
		}
	}

	
	private static int processOptions(FLAG... flags) {
		int bitwise = 1;
		if (Arrays.asList(flags).contains(FLAG.BLOCKING)) bitwise = bitwise + 2;
		if (Arrays.asList(flags).contains(FLAG.FILTER_OLD)) bitwise = bitwise - 1;
		return bitwise;
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
	public static void peisjava_appendTupleByAbstract(PeisTuple abstractTuple,	int difflen, Pointer diff) {
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_appendTupleByAbstract(abstractTuple,	difflen, diff);
		}
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
	public static void peisjava_appendStringTupleByAbstract(PeisTuple abstractTuple, String diff) {
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_appendStringTupleByAbstract(abstractTuple, diff);
		}
	}
	
	
	/**
	 * Creates a default META tuple in our own tuple space if the tuple does not yet exist.
	 * Useful for providing default values to
	 * configuration parameters.
	 * @param key The key of the tuple o set.
	 * @param data The value to set.
	 */
	public static void peisjava_setDefaultMetaStringTuple(String key, String data) {
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_setDefaultMetaStringTuple(key, data);
		}
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
	public static void peisjava_setDefaultMetaTuple(String key, int datalen, Pointer data, String mimetype, ENCODING encoding) {
		synchronized(peiskCriticalSection) {
			int enc = -1;
			if (encoding.equals(ENCODING.ASCII)) enc = 0;
			else if (encoding.equals(ENCODING.BINARY)) enc = 1;
			INSTANCE.peisk_setDefaultMetaTuple(key, datalen, data, mimetype, enc);
		}
	}
	

	
	/**
	 * Sets a value to a tuple in our own namespace if the tuple does not yet exist.
	 * Useful for providing default values to
	 * tuples which may be configured via the command line --set-tuple option.
	 * @param key The key of the tuple o set.
	 * @param datalen The length of the data to be set.
	 * @param data The value to set.
	 */
	public static void peisjava_setDefaultTuple(String key, int datalen, Pointer data, String mimetype, ENCODING encoding) {
		synchronized(peiskCriticalSection) {
			int enc = -1;
			if (encoding.equals(ENCODING.ASCII)) enc = 0;
			else if (encoding.equals(ENCODING.BINARY)) enc = 1;
			INSTANCE.peisk_setDefaultTuple(key, datalen, data, mimetype, enc);
		}		
	}


	/**
	 * Wrapper to test if anyone is currently subscribed to the given (full) key in our local tuplespace. Wrapper for backwards compatability.
	 * @param key A (full) key in our local tuplespace.
	 * @return Whether anyone is subscribed to a tuple with the given key.
	 */
	public static boolean peisjava_hasSubscriber(String key) {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_hasSubscriber(key);
		}
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
	public static int peisjava_findOwner(String key) {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_findOwner(key);
		}
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
	public static void peisjava_setRemoteStringTuple(int owner, String key, String value) {
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_setRemoteStringTuple(owner, key, value);
		}
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
	public static void peisjava_setRemoteStringTupleBlocking(int owner, String key, String value) {
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_setRemoteStringTupleBlocking(owner, key, value);
		}
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
	public static void peisjava_setTupleIndirectly(int metaOwner, String metaKey, byte[] value, String mimetype, ENCODING encoding) throws PeisTupleException {
		Memory m = new Memory(value.length);
		m.write(0, value, 0, value.length);
		synchronized(peiskCriticalSection) {
			int enc = -1;
			if (encoding.equals(ENCODING.ASCII)) enc = 0;
			else if (encoding.equals(ENCODING.BINARY)) enc = 1;
			int ret = INSTANCE.peisk_setTupleIndirectly(metaOwner, metaKey, value.length, m, mimetype, enc);
			if (ret != 0) throw new PeisTupleException(ret, INSTANCE);
		}
	}
	


	/**
	 * Get all tuples matching the given owner/key pair.
	 * @param owner The owner of the sought tuples.
	 * @param key The keyname of the tuples.
	 * @return A result set containing the found tuples (null if none are found).
	 */
	public static PeisTupleResultSet peisjava_getTuples(int owner, String key) {
		synchronized(peiskCriticalSection) {
			PeisTupleResultSet result = INSTANCE.peisk_createResultSet();
			if (INSTANCE.peisk_getTuples(owner, key, result) > 0) return result;
		return null;
		}
	}

	
	/**
	 * Get all tuples matching the given owner/key pair.
	 * @param owner The owner of the sought tuples.
	 * @param key The keyname of the tuples.
	 * @return A result set containing the found tuples (null if none are found).
	 */
	public static PeisTupleResultSet peisjava_getTuples(int owner, String key, FLAG flag) {
		PeisTupleResultSet result = null;
		if (flag.equals(FLAG.BLOCKING)) {
			result = PeisJavaMT.peisjava_getTuples(owner, key);
			while (result == null) {
				synchronized(peiskCriticalSection) { INSTANCE.peisk_waitOneCycle(10000); }
				result = PeisJavaMT.peisjava_getTuples(owner, key);
			}
			return result;
		}
		return peisjava_getTuples(owner, key);
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
		synchronized(peiskCriticalSection) {
			int enc = -1;
			if (encoding.equals(ENCODING.ASCII)) enc = 0;
			else if (encoding.equals(ENCODING.BINARY)) enc = 1;
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
		synchronized(peiskCriticalSection) {
			int enc = -1;
			if (encoding.equals(ENCODING.ASCII)) enc = 0;
			else if (encoding.equals(ENCODING.BINARY)) enc = 1;
			INSTANCE.peisk_setRemoteTupleBlocking(owner, key, data.length, m, mimetype, enc);
		}
	}


	/**
	 * Sets the user defined timestamp (ts_user) for all comming tuples
	 * that are created. This is for backwards compatability but also
	 * convinience when many tuples are created.
	 * @param ts_user_sec Time (seconds)
	 * @param ts_user_usec Time (milliseconds)
	 */
	public static void peisjava_tsUser(int tsUserSec, int tsUserUsec) {
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_tsUser(tsUserSec, tsUserUsec);
		}
	}
	
	/**
	 * Clone a tuple (including the data field). The user of this
	 * function is responsible for freeing the memory eventually, see
	 * peisk_freeTuple.
	 * @param tuple The tuple to clone.
	 * @return The new tuple.
	 */
	public static PeisTuple peisjava_cloneTuple(PeisTuple tuple) {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_cloneTuple(tuple);
		}		
	}



}
