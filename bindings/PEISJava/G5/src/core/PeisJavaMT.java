/*
    Copyright (C) 2008  Federico Pecora
    
    Based on libpeiskernel (Copyright (C) 2005 - 2006  Mathias Broxvall).
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
import com.sun.jna.ptr.PointerByReference;

import java.net.URLClassLoader;
import java.lang.SecurityManager;

/**
 * This class implements the {@link core.PeisJavaInterface} interface, providing static access to
 * the peiskernel functionality.  The class provides convenient multithread wrappers for the
 * peiskernel library.  It allows the programmer to access core peiskernel functionality at three
 * levels:
 * <ul>
 * <li><i>high-level access to most common features</i>, e.g., initializing,
 * reading and writing tuples, and various convenience methods;</li>
 * <li><i>thread-safe access to lower-level peiskernel functionality</i>, e.g.,
 * manually creating result sets for tuple searches etc;</li>
 * <li><i>non-thread-safe access to low level peiskernel functionalities</i>, e.g.,
 * accessing the underlying peer-to-peer network or broadcasting messages to
 * known hosts.</i></li>
 * </ul>
 * The first level of functionality is provided through static methods of this class
 * of the form {@code peisjava_XXX(...)}.  The second level of access is provided through
 * static methods of this class of the form {@code peisk_XXX(...)}. Finally, non-thread-safe
 * access to other peiskernel functions is possible through the {@link #INSTANCE} object of
 * type {@link core.PeisJavaInterface}.  If these latter non-thread-safe methods are invoked,
 * they must be synchronized on the {@link #peiskCriticalSection} object.
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

public class PeisJavaMT implements PeisJavaInterface {

	public enum FLAG { FILTER_OLD, BLOCKING };
	
	/**
	 * Reference to PEISKernel library instance.  Use if you need to access the
	 * peisk_xxx() calls directly (otherwise just invoke the static methods
	 * of PEISJava). 
	 */
	public static PeisJavaInterface INSTANCE = (PeisJavaInterface)Native.loadLibrary("peiskernel",PeisJavaInterface.class);
	//public static PeisJavaInterface INSTANCE = (PeisJavaInterface)Native.synchronizedLibrary((Library)Native.loadLibrary("peiskernel",PeisJavaInterface.class));
	//public static PeisJavaInterface INSTANCE = (PeisJavaInterface)Native.loadLibrary("peiskernel_mt",PeisJavaInterface.class);

	private static int peisjava_tuple_errno = 0;
	
	//private static boolean MULTIPLE_INSTANCE = true;
	
	/**
	 * Object to synchronize against when using the peisk_XXX methods directly.
	 */
	public static Object peiskCriticalSection = new Object();

	private static class PeiskThread extends Thread {
		
		//Callback queue
		private Vector<Callback> pendingCallbacks = null;
		private HashMap<PeisTupleCallback,PeisCallbackHandle> callbacks = null;
		private Vector<PeisTupleCallback> toRemoveCallbacks = null;
		
		private int frequencyHz = 100;
		
		private Object callbackQueueCriticalSection = new Object();
		
		public class Callback {
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
							PeisCallbackHandle hndl = PeisJavaMT.INSTANCE.peisk_registerTupleCallback(cb.key, cb.owner, null, cb.tupleCallback);
							System.out.println("REGISTERED " + cb);
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
				
				//See if there are any callbacks to unregister...
				if (toRemoveCallbacks != null) {
					for (PeisTupleCallback tupleCallback : toRemoveCallbacks) {
						PeisCallbackHandle hndl = callbacks.get(tupleCallback);
						synchronized(peiskCriticalSection) {
							INSTANCE.peisk_unregisterTupleCallback(hndl);							
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
	 * A wrapper function for setting tuples whose value is a simple
	 * String.
	 * @param owner The owner of the tuple.
	 * @param name The name of the tuple to set.
	 * @param value The value to set.  A String value of "nil" is set if
	 * this parameter is {@code null}.
	 */
	public static void peisjava_setStringTuple(int owner, String name, String value) {
		/*
		byte[] bytes = null;
		if (value != null) bytes = value.getBytes();
		else bytes = (new String("nil")).getBytes();
		Memory m = new Memory(bytes.length);
		m.write(0, bytes, 0, bytes.length);
		*/

		String nil = "nil";
		byte[] bytes = null;
		byte[] valueBytes = null;
		if (value != null) {
			bytes = new byte[value.length()+1];
			valueBytes = value.getBytes();
		}
		else {
			bytes = new byte[nil.length()+1];
			valueBytes = nil.getBytes();
		}
		for (int i = 0; i < valueBytes.length; i++)
			bytes[i] = valueBytes[i];
		bytes[valueBytes.length] = '\0';
		
		Memory m = new Memory(bytes.length);
		m.write(0, bytes, 0, bytes.length);
		
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_setRemoteTuple(name, owner, bytes.length, m);
		}
	}

	/**
	 * Get a tuple from the local tuplespace. The owner of the tuple must be the caller.  Note: this is a non-blocking read, and the same value can be read more than once.
	 * @param key The fully qualified key of the tuple to get.
	 * @return The tuple matching the given key.
	 */
	public static PeisTuple peisjava_getTuple(String key) {
		synchronized(peiskCriticalSection) {
			PeisTuple ret = INSTANCE.peisk_getTuple(key, INSTANCE.peisk_peisid(), new IntByReference(), new PointerByReference(), 1);
			return ret;
		}
	}
	
	/**
	 * Get a tuple from the local tuplespace. The owner of the tuple must be the caller.
	 * @param key The fully qualified key of the tuple to get.
	 * @param flags Flags indicating whether call should be blocking and/or filter old values.
	 * @return The tuple matching the given key.
	 */
	public static PeisTuple peisjava_getTuple(String key, FLAG... flags) {
		synchronized(peiskCriticalSection) {
			PeisTuple ret = INSTANCE.peisk_getTuple(key, INSTANCE.peisk_peisid(), new IntByReference(), new PointerByReference(), processOptions(flags));
			return ret;
		}
	}
	
	/**
	 * Get a tuple from the local tuplespace. The owner of the tuple must be the caller.  Note: this is a non-blocking read, and the same value can be read more than once.
	 * @param owner The owner of the tuple.
	 * @param key The fully qualified key of the tuple to get.
	 * @return The tuple matching the given key.
	 */
	public static PeisTuple peisjava_getTuple(int owner, String key) {
		synchronized(peiskCriticalSection) {
			PeisTuple ret = INSTANCE.peisk_getTuple(key, owner, new IntByReference(), new PointerByReference(), 1);
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
			PeisTuple ret = INSTANCE.peisk_getTuple(key, owner, new IntByReference(), new PointerByReference(), processOptions(flags));
			return ret;
		}
	}
	
	/**
	 * Get the data in a tuple in the form of a String.  Note: this is a non-blocking read, and the same value can be read more than once.
	 * be the caller component.  Note: this is a non-blocking read.
	 * @param name The name of the tuple.
	 * @return A String containing the tuple data if the tuple was found, {@code null} otherwise. 
	 */
	public static String peisjava_getStringTuple(String name) {
    	PointerByReference value = new PointerByReference();
    	PeisTuple tup = null;
    	synchronized(peiskCriticalSection) {
    		if (INSTANCE.peisk_tupleExists(name))
    			tup = INSTANCE.peisk_getTuple(name, INSTANCE.peisk_peisid(), new IntByReference(), value, 1);
    	}
    	if (tup != null && value.getValue() != null)
    		return value.getValue().getString(0);
		return null;
	}
	
	/**
	 * Get the data in a tuple in the form of a String.
	 * be the caller component.  Note: this is a non-blocking read.
	 * @param name The name of the tuple.
	 * @param flags Flags indicating whether call should be blocking and/or filter old values.
	 * @return A String containing the tuple data if the tuple was found, {@code null} otherwise. 
	 */
	public static String peisjava_getStringTuple(String name, FLAG... flags) {
    	PointerByReference value = new PointerByReference();
    	PeisTuple tup = null;
    	synchronized(peiskCriticalSection) {
    		if (INSTANCE.peisk_tupleExists(name)) {
    			tup = INSTANCE.peisk_getTuple(name, INSTANCE.peisk_peisid(), new IntByReference(), value, processOptions(flags));
    		}
    	}
    	if (tup != null && value.getValue() != null)
    		return value.getValue().getString(0);
		return null;
	}

		
	/**
	 * Get the data in a tuple in the form of a String.  Note: this is a non-blocking read, and the same value can be read more than once.
	 * @param name The name of the tuple.
	 * @param owner The owner of the tuple.
	 * @return A String containing the tuple data if the tuple was found, {@code null} otherwise. 
	 */
	public static String peisjava_getStringTuple(int owner, String name) {
    	PointerByReference value = new PointerByReference();
    	PeisTuple tup = null;
    	synchronized(peiskCriticalSection) {
    		tup = INSTANCE.peisk_getTuple(name, owner, new IntByReference(), value, 1);
    	}
    	if (tup != null && value.getValue() != null)
    		return value.getValue().getString(0);
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
    	PointerByReference value = new PointerByReference();
    	PeisTuple tup = null;
    	synchronized(peiskCriticalSection) {
   			tup = INSTANCE.peisk_getTuple(name, owner, new IntByReference(), value, processOptions(flags));
    	}
    	if (tup != null && value.getValue() != null)
    		return value.getValue().getString(0);
		return null;
	}

	/**
	 * Get the data in a tuple in the form of a byte array.  The owner of the tuple must
	 * be the caller component.  Note: this is a non-blocking read, and the same value can be read more than once.
	 * @param name The name of the tuple.
	 * @return A byte array containing the tuple data if the tuple was found, {@code null} otherwise. 
	 */
	public static byte[] peisjava_getByteTuple(String name) {
    	PointerByReference value = new PointerByReference();
    	PeisTuple tup = null;
    	synchronized(peiskCriticalSection) {
      		if (INSTANCE.peisk_tupleExists(name))
      			tup = INSTANCE.peisk_getTuple(name, INSTANCE.peisk_peisid(), new IntByReference(), value, 1);
    	}
    	if (tup != null && value.getValue() != null)
    		return value.getValue().getByteArray(0, tup.datalen);
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
    	PointerByReference value = new PointerByReference();
    	PeisTuple tup = null;
    	synchronized(peiskCriticalSection) {
      		if (INSTANCE.peisk_tupleExists(name)) {
      			tup = INSTANCE.peisk_getTuple(name, INSTANCE.peisk_peisid(), new IntByReference(), value, processOptions(flags));
      		}
    	}
    	if (tup != null && value.getValue() != null)
    		return value.getValue().getByteArray(0, tup.datalen);
		return null;
	}
	
	/**
	 * Get the data in a tuple in the form of a byte array.  Note: this is a non-blocking read, and the same value can be read more than once.
	 * @param name The name of the tuple.
	 * @param owner The owner of the tuple.
	 * @return A byte array containing the tuple data if the tuple was found, {@code null} otherwise. 
	 */
	public static byte[] peisjava_getByteTuple(int owner, String name) {
    	PointerByReference value = new PointerByReference();
    	PeisTuple tup = null;
    	synchronized(peiskCriticalSection) {
    		tup = INSTANCE.peisk_getTuple(name, owner, new IntByReference(), value, 1);
    	}
    	if (tup != null && value.getValue() != null)
    		return value.getValue().getByteArray(0, tup.datalen);
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
    	PointerByReference value = new PointerByReference();
    	PeisTuple tup = null;
    	synchronized(peiskCriticalSection) {
    		tup = INSTANCE.peisk_getTuple(name, owner, new IntByReference(), value, processOptions(flags));
    	}
    	if (tup != null && value.getValue() != null)
    		return value.getValue().getByteArray(0, tup.datalen);
		return null;
	}
	
	/**
	 * Gets all values from local tuplespace for a given fully qualified key.
	 * @param key The (partial) name of the tuples to get.
	 * @return A hashmap of owner-(vector-of-tuple-values) pairs.
	 */
	/*
	public static HashMap<Integer,Vector<byte[]>> peisjava_getAllTuples(String key) {
		HashMap<Integer,Vector<byte[]>> map = null;
		IntByReference owner = new IntByReference(0);
		IntByReference len = new IntByReference(0);
		PointerByReference ptr = new PointerByReference();
		IntByReference handle = new IntByReference(0);
		PeisTuple ret = null;
		synchronized(peiskCriticalSection) {
			ret = INSTANCE.peisk_getTuples(key, owner, len, ptr, handle, false);
		}
		while (ret != null) {
			if (map == null)
				map = new HashMap<Integer,Vector<byte[]>>();
			Vector<byte[]> vec = null;
			if (!map.containsKey(new Integer("" + owner.getValue()))) {
				vec = new Vector<byte[]>();
				map.put(new Integer("" + owner.getValue()), vec);
			}
			else
				vec = map.get(new Integer("" + owner.getValue()));
			vec.add(ptr.getPointer().getPointer(0).getByteArray(0, ret.datalen));
			synchronized(peiskCriticalSection) {
				ret = INSTANCE.peisk_getTuples(key, owner, len, ptr, handle, false);
			}
		}
		return map;
	}
	*/
	
	/**
	 * Gets all values from local tuplespace for a given fully qualified key.
	 * Values are returned as Strings.
	 * @param key The (partial) name of the tuples to get.
	 * @return A hashmap of owner-(vector-of-tuple-values) pairs.
	 */
	/*
	public static HashMap<Integer,Vector<String>> peisjava_getAllStringTuples(String key) {
		HashMap<Integer,Vector<String>> map = null;
		IntByReference owner = new IntByReference(0);
		IntByReference len = new IntByReference(0);
		PointerByReference ptr = new PointerByReference();
		IntByReference handle = new IntByReference(0);
		PeisTuple ret = null;
		synchronized(peiskCriticalSection) {
			ret = INSTANCE.peisk_getTuples(key, owner, len, ptr, handle, false);
		}
		while (ret != null) {
			Vector<String> vec = null;
			if (map == null)
				map = new HashMap<Integer,Vector<String>>();
			if (!map.containsKey(new Integer("" + owner.getValue()))) {
				vec = new Vector<String>();
				map.put(new Integer("" + owner.getValue()), vec);
			}
			else
				vec = map.get(new Integer("" + owner.getValue()));
			vec.add(new String(ptr.getPointer().getPointer(0).getByteArray(0, ret.datalen)));
			synchronized(peiskCriticalSection) {
				ret = INSTANCE.peisk_getTuples(key, owner, len, ptr, handle, false);
			}
		}
		return map;
	}
	*/
	
	/**
	 * Factory method for creating a {@link PeisTuple} with a given fully qualified name.
	 * @param name The fully qualified name of the tuple.
	 * @return A new tuple.
	 */
	public static PeisTuple peisjava_createPeisTuple(String name) {
		PeisTuple tuple = new PeisTuple();
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_initTuple(tuple);
		}
		if (PeisJavaMT.peisjava_setTupleName(tuple, name) != 0) return null;
		return tuple;
	}
	
	/**
	 * Factory method for creating a meta {@link PeisTuple} with a given fully qualified name
	 * which points to a given real tuple.
	 * @param metaKey The fully qualified name of the meta tuple to create.
	 * @param realKey The fully qualified name of the real tuple.
	 * @param realOwner The owner of the real tuple.
	 * @return A new meta tuple referencing the given real tuple.
	 */
	public static PeisTuple peisjava_createMetaPeisTuple(String metaKey, String realKey, int realOwner) {
		PeisTuple tuple = new PeisTuple();
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_initMetaTuple(INSTANCE.peisk_peisid(), metaKey);
			INSTANCE.peisk_writeMetaTuple(INSTANCE.peisk_peisid(), metaKey, realOwner, realKey);
		}
		return tuple;
	}	

	/**
	 * Factory method for creating an abstract {@link PeisTuple} with a given name.
	 * @param name The name of the tuple.
	 * @return A new abstract tuple.
	 */
	public static PeisTuple peisjava_createAbstractPeisTuple(String name) {
		PeisTuple tuple = new PeisTuple();
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_initAbstractTuple(tuple);
		}
		if (PeisJavaMT.peisjava_setTupleName(tuple, name) != 0) return null;
		return tuple;
	}

	/**
	 * Write raw bytes to a tuple.
	 * @param mimeType The mime type of the bytes.  If {@code null} then
	 * the default type "example/peisdata" is set.
	 * @param key The name of the tuple.
	 * @param data The data to be written.
	 */
	public static void peisjava_setByteArrayTuple(String mimeType, String key, byte[] data) {
		if (data != null) {
			String mimeT = mimeType;
			if (mimeT == null) {
				mimeT = "example/peisdata";
			}
			Memory m = new Memory(data.length+mimeT.length()+1);
			m.write(0, mimeT.getBytes(), 0, mimeT.length());
			byte[] terminator = {0x0};
			m.write(mimeT.length(), terminator, 0, 1);
			m.write(mimeT.length()+1, data, 0, data.length);
			synchronized(peiskCriticalSection) {
				INSTANCE.peisk_setTuple(key, data.length+mimeT.length()+1, m);
			}
		}
	}
	
	/**
	 * Looks up a tuple in the local tuplespace and locally cached tuples.
	 * The search string may contain wildcards and may give multiple results.
	 * The returned resultSet must be accessed with the {@link PeisTupleResultSet#next()} method.
	 * The resultSet is freed automatically by the Java garbage collector.
	 * @param key The search string (e.g., A.*.B).
	 * @return A result set (to be accessed with {@link PeisTupleResultSet#next()})
	 * or {@code null} if no results were found.
	 */
	public static PeisTupleResultSet peisjava_findTuples(String key) {
		PeisTuple tup = new PeisTuple();
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_initAbstractTuple(tup);
			INSTANCE.peisk_setTupleName(tup, key);
			PeisTupleResultSet result = INSTANCE.peisk_createResultSet();
			if (INSTANCE.peisk_findTuples(tup, result) > 0) return result;
			return null;
		}
	}
		
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
	 * Makes a lookup for latest value of meta tuple M and provides
	 * an abstract tuple to point to the tuple referenced by M. 
	 * @param M The reference meta tuple.
	 * @return The abstract tuple built by referencing the meta tuple M, {@code null} in case of failure.
	 */
	public static PeisTuple peisjava_getGeneralizationFromIndirectTuple(PeisTuple M) {
		PeisTuple A = new PeisTuple();
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_initAbstractTuple(A);
			if (INSTANCE.peisk_initIndirectTuple(M, A) != 0) return null;
			return A;
		}
	}

	/*
	 * Thread safe functions
	 */

	/**
	 * Add to list of hosts that will repeatedly be connected to.
	 * @param url The host to add.
	 */
	public static void peisjava_autoConnect(String url) {
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_autoConnect(url);
		}
	}
	
	@Override
	public void peisk_autoConnect(String url) {
		// TODO Auto-generated method stub		
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

	@Override
	public boolean peisk_broadcastFrom(int from, int port, int len, Pointer data) {
		// TODO Auto-generated method stub
		return false;
	}

	/**
	 * @deprecated Functionality provided by {@link PeisTuple#clone()}.
	 */
	@Deprecated
	@Override
	public PeisTuple peisk_cloneTuple(PeisTuple tuple) {
		// TODO Auto-generated method stub
		return null;
	}
 
	/**
	 * @deprecated Functionality provided by {@link PeisTuple#compareTo(PeisTuple)}.
	 */
	@Deprecated
	@Override
	public int peisk_compareTuples(PeisTuple tuple1, PeisTuple tuple2) {
		// TODO Auto-generated method stub
		return 0;
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

	@Override
	public int peisk_connect(String url, int flags) {
		// TODO Auto-generated method stub
		return 0;
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

	@Override
	public PeisTupleResultSet peisk_createResultSet() {
		// TODO Auto-generated method stub
		return null;
	}

	/**
	 * Sets a value to a tuple in our own namespace if it has not
	 * yet been given a value. Useful for providing default values to
	 * tuples which may be configured via the command line --set-tuple option
	 * @param key The key of the tuple o set.
	 * @param value The value to set.
	 */
	public static void peisjava_defaultStringTuple(String key, String value) {
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_defaultStringTuple(key, value);
		}
		
	}

	@Override
	public void peisk_defaultStringTuple(String key, String value) {
		// TODO Auto-generated method stub
		
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

	@Override
	public void peisk_deleteResultSet(PeisTupleResultSet resultSet) {
		// TODO Auto-generated method stub
		
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
	
	@Override
	public void peisk_deleteTuple(int owner, String key) {
		// TODO Auto-generated method stub
		
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
	public static PeisTuple peisjava_findIndirectTuple(PeisTuple M) {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_findIndirectTuple(M);
		}
	}
	
	@Override
	public PeisTuple peisk_findIndirectTuple(PeisTuple M) {
		// TODO Auto-generated method stub
		return null;
	}

	/**
	 * Looks up a tuple in the local tuplespace and locally cached
	 * tuples using the fully qualified owner and key only (no wildcards
	 * permitted).
	 * @param tuple The tuple to find.
	 * @return {@code null} if the tuple was not found. 
	 */
	public static PeisTuple peisjava_findSimpleTuple(PeisTuple tuple) {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_findSimpleTuple(tuple);
		}
	}

	@Override
	public PeisTuple peisk_findSimpleTuple(PeisTuple tuple) {
		// TODO Auto-generated method stub
		return null;
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
	public static int peisjava_findTuples(PeisTuple tuple, PeisTupleResultSet resultset) {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_findTuples(tuple, resultset);
		}
	}

	@Override
	public int peisk_findTuples(PeisTuple tuple, PeisTupleResultSet resultset) {
		// TODO Auto-generated method stub
		return 0;
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
	
	@Override
	public void peisk_freeTuple(PeisTuple tuple) {
		// TODO Auto-generated method stub
		
	}


	
	/**
	 * Use the meta tuple given by (metaOwner,metaKey) to find a reference to a
	 * specific tuple. Returns this referenced tuple [metaOwner,metaKey] if found.
	 * Must previously have created a metaSubscription to metaKey and metaOwner.
	 * @param metaKey The key of the meta tuple.
	 * @param metaOwner The owner of the meta tuple. 
	 * @return Tuple [metaOwner,metaKey] if found, {@code null} otherwise.
	 */
	public static PeisTuple peisjava_getIndirectTuple(String metaKey, int metaOwner) {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_getIndirectTuple(metaKey, metaOwner, new IntByReference(), new PointerByReference(), 1);
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
	public static PeisTuple peisjava_getIndirectTuple(String metaKey, int metaOwner, FLAG... flags) {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_getIndirectTuple(metaKey, metaOwner, new IntByReference(), new PointerByReference(), processOptions(flags));
		}
	}

	
	/**
	 * Use the meta tuple given by (metaOwner,metaKey) to find a reference to a
	 * specific tuple. Returns this referenced tuple [metaOwner,metaKey] if found.
	 * Must previously have created a metaSubscription to metaKey and metaOwner.
	 * @param metaKey The key of the meta tuple.
	 * @param metaOwner The owner of the meta tuple. 
	 * @param len The length of the data field of the returned tuple.
	 * @param ptr The data of the returned tuple.
	 * @param flags Flags modifying the behaviour of internal getTuple call.
	 * @return Tuple [metaOwner,metaKey] if found, {@code null} otherwise.
	 */
	/*
	public static PeisTuple peisjava_getIndirectTuple(String metaKey, int metaOwner,
			IntByReference len, PointerByReference ptr, boolean flags) {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_getIndirectTuple(metaKey, metaOwner, len, ptr, 1);
		}
	}
	*/

	@Override
	public PeisTuple peisk_getIndirectTuple(String metaKey, int metaOwner,
			IntByReference len, PointerByReference ptr, int flags) {
		// TODO Auto-generated method stub
		return null;
	}
	

	/**
	 * Wrapper for getting a value from the local tuplespace for the fully qualified given key and owner. 
	 * Provides backwards compatibility with kernel G3 and earlier.
	 * @param key The keyname of the tuple.
	 * @param owner The owner of the tuple.
	 * @param len Length of returned data. If {@code null} then ignored.
	 * @param ptr The returned data. If {@code null} then ignored.
	 * @param flags Flags modifying the behaviour of getTuple.
	 * Logical or of flags: PEISK_TFLAG_OLDVAL,  PEISK_TFLAG_BLOCKING.
	 * @return The tuple if successful {@code null} otherwise.
	 */
	/*
	public static PeisTuple peisjava_getTuple(String key, int owner, IntByReference len,
			PointerByReference ptr, int flags) {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_getTuple(key, owner, len, ptr, flags);
		}
	}
	*/

	@Override
	public PeisTuple peisk_getTuple(String key, int owner, IntByReference len,
			PointerByReference ptr, int flags) {
		// TODO Auto-generated method stub
		return null;
	}

	/**
	 * Wrapper for getting values, one at a a time, from local tuplespace
	 * for the given fully qualified key and any producer. Uses an
	 * integer "handle" to iterate over all values. Returns non-{@code null} if a
	 * value existed and if so modifies len, ptr to the data for this
	 * tuple and sets owner to the owner of this tuple.
	 * @param key The keyname of the tuple.
	 * @param owner Is modified to be the owner of the returned tuple.
	 * @param len Modified to be length of the returned tuple. If {@code null} then ignored.
	 * @param ptr Set to the actual data of the tuple. If {@code null} then
	 * ignored.
	 * @param handle Used to iterate over the tuples. Must be zero on first call.
	 * @param flags Flags modifying the behaviour of getTuple.
	 * Logical or of flags: PEISK_TFLAG_OLDVAL,  PEISK_TFLAG_BLOCKING.
	 * @return The tuples if successful {@code null} otherwise.
	 */
	/*
	public static PeisTuple peisjava_getTuples(String key, IntByReference owner,
			IntByReference len, PointerByReference ptr, IntByReference handle,
			boolean flags) {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_getTuples(key, owner, len, ptr, handle, flags);
		}
	}
	*/

	/**
	 * @deprecated Functionality provided by {@link #peisk_findTuples(PeisTuple, PeisTupleResultSet)}.
	 */
	@Deprecated
	@Override
	public PeisTuple peisk_getTuples(String key, IntByReference owner,
			IntByReference len, PointerByReference ptr, IntByReference handle,
			int flags) {
		// TODO Auto-generated method stub
		return null;
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

	@Override
	public int peisk_gettime() {
		// TODO Auto-generated method stub
		return 0;
	}

	/**
	 * Gives the current time in seconds and microseconds since the EPOCH
	 * with two long int pointer arguments where the result is
	 * stored. Uses the synchronized PEIS vector clock.
	 * @param t0 Current time (seconds).
	 * @param t1 Current time (microseconds).
	 */
	public static void peisjava_gettime2(LongByReference t0, LongByReference t1) {
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_gettime2(t0, t1);
		}
	}

	@Override
	public void peisk_gettime2(LongByReference t0, LongByReference t1) {
		// TODO Auto-generated method stub
		
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
	
	@Override
	public double peisk_gettimef() {
		// TODO Auto-generated method stub
		return 0;
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

	@Override
	public int peisk_hashString(String string) {
		// TODO Auto-generated method stub
		return 0;
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

	@Override
	public void peisk_initAbstractTuple(PeisTuple tup) {
		// TODO Auto-generated method stub
		
	}

	/**
	 * Makes a lookup for latest value of meta tuple M and initializes
	 * abstract tuple A to point to tuple referenced by M. 
	 * @param M The reference meta tuple.
	 * @param A The abstract tuple to point to the tuple referenced by the meta tuple.
	 * @return Zero on success, error code otherwise
	 */
	public static int peisjava_initIndirectTuple(PeisTuple M, PeisTuple A) {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_initIndirectTuple(M, A);
		}
	}

	@Override
	public int peisk_initIndirectTuple(PeisTuple M, PeisTuple A) {
		// TODO Auto-generated method stub
		return 0;
	}

	/**
	 * Initializes a meta tuple to reasonable default values.
	 * @param metaOwner The owner of the meta tuple.
	 * @param metaKey The key of the meta tuple.
	 */
	public static void peisjava_initMetaTuple(int metaOwner, String metaKey) {
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_initMetaTuple(metaOwner, metaKey);
		}
	}
	
	@Override
	public void peisk_initMetaTuple(int metaOwner, String metaKey) {
		// TODO Auto-generated method stub
		
	}

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

	@Override
	public void peisk_initTuple(PeisTuple tup) {
		// TODO Auto-generated method stub
		
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
	    	  String peiskVersion = peisjava_getStringTuple("kernel.version");
	    	  if (!peiskVersion.substring(0, peiskVersion.lastIndexOf('.')).equals(INSTANCE.version.substring(0, peiskVersion.lastIndexOf('.'))))
	    		  throw new WrongPeisKernelVersionException(INSTANCE.version, peiskVersion);
	    	  //Spawn the peiskmt thread which steps @100Hz
	    	  if (!shareThread) {
	    		  peiskThread = new PeiskThread();
	    		  peiskThread.start();
	    	  }
	          //Write the do-quit tuple
	    	  //PeisJavaMT.peisjava_setStringTuple("do-quit",null);
	      }      

	}

	@Override
	public void peisk_initialize(IntByReference argc, String[] args) {
		// TODO Auto-generated method stub
		
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
	public static boolean peisjava_insertTuple(PeisTuple tuple) {
		synchronized(peiskCriticalSection) {
			return ((PeisJavaInterface)PeisJavaMT.INSTANCE).peisk_insertTuple(tuple);
		}
	}	

	@Override
	public boolean peisk_insertTuple(PeisTuple tuple) {
		// TODO Auto-generated method stub
		return false;
	}

	/**
	 * @deprecated Functionality provided by {@link PeisTuple#equals(Object)}.
	 */
	@Deprecated
	@Override
	public boolean peisk_isEqual(PeisTuple tuple1, PeisTuple tuple2) {
		// TODO Auto-generated method stub
		return false;
	}
 
	/**
	 * @deprecated Functionality provided by {@link PeisTuple#isGeneralization(PeisTuple)}.
	 */
	@Deprecated
	@Override
	public boolean peisk_isGeneralization(PeisTuple tuple1, PeisTuple tuple2) {
		// TODO Auto-generated method stub
		return false;
	}

	/**
	 * Returns true if the given meta tuple is currently pointing to a
	 * valid tuple.
	 * @param metaKey The key of the meta tuple.
	 * @param metaOwner The owner of the meta tuple.
	 * @return {@code true} if the given meta tuple is currently pointing to a
	 * valid tuple.
	 */
	public static boolean peisjava_isIndirectTuple(String metaKey, int metaOwner) {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_isIndirectTuple(metaKey, metaOwner);
		}
	}

	@Override
	public boolean peisk_isIndirectTuple(String metaKey, int metaOwner) {
		// TODO Auto-generated method stub
		return false;
	}

	/**
	 * Ascertain if the peiskernel is running.
	 * @return {@code true} after initialization until we have performed a shutdown.
	 */
	public static boolean peisjava_isRunning() {
		return INSTANCE.peisk_isRunning();
	}
	
	@Override
	public boolean peisk_isRunning() {
		// TODO Auto-generated method stub
		return false;
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
			return ((PeisJavaInterface)PeisJavaMT.INSTANCE).peisk_isSubscribed(prototype);
		}
	}
	
	@Override
	public boolean peisk_isSubscribed(PeisTuple prototype) {
		// TODO Auto-generated method stub
		return false;
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
	
	@Override
	public int peisk_peisid() {
		// TODO Auto-generated method stub
		return 0;
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

	@Override
	public void peisk_printUsage(Pointer stream, short argc, String[] args) {
		// TODO Auto-generated method stub
		
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
	public static PeisTupleCallback peisjava_registerTupleCallback(String key, int owner, CallbackObject co) {
		return peiskThread.registerCallback(key, owner, co);
	}
	
	
	/**
	 * Register a callback on a tuple.  The callback routine must be implemented in
	 * the method {@link PeisJavaInterface.PeisTupleCallback#callback(PeisTuple, Pointer)} of a callback object which implements
	 * the interface class {@link PeisJavaInterface.PeisTupleCallback}. 
	 * @param key The key of the tuple to register the callback to.
	 * @param owner The owner of the tuple to register the callback to.
	 * @param callbackObjectClass The name of the callback object class.
	 * @return The {@link PeisJavaInterface.PeisTupleCallback} if the a callback object was successfully instantiated
	 * and queued for registration, {@code null} otherwise.
	 * @deprecated This method required the programmer to indicate the name of the callback object, which would then be
	 * instantiated by the PeisJavaMT through a rather elaborated mechanism.  Since the programmer could not create the instance, this made it cumbersome to
	 * exchange information between the relevant PEIS component and the callback.  The scheme for creating tuple callbacks has now changed and relies
	 * on the {@link CallbackObject} abstract class, which can be extended and instantiated directly by the programmer.  The correct method to use
	 * for registering such callback objects is {@link PeisJavaMT#peisjava_registerTupleCallback(String, int, CallbackObject)}.
	 */
	@Deprecated
	public static PeisTupleCallback peisjava_registerTupleCallback(String key, int owner, String callbackObjectClass) {
		return peiskThread.registerCallbackOld(key, owner, callbackObjectClass);
	}

	
	@Override
	public PeisCallbackHandle peisk_registerTupleCallback(String key,
			int owner, Pointer userdata, PeisTupleCallback fn) {
		// TODO Auto-generated method stub
		return null;
	}
	
	/**
	 * Unregister a tuple callback given the {@link PeisJavaInterface.PeisTupleCallback} object returned
	 * by {@link PeisJavaMT#peisjava_registerTupleCallback(String, int, String)}.
	 * @param tupleCallback The callback to unregister.
	 */
	public static void peisjava_unregisterTupleCallback(PeisTupleCallback tupleCallback) {
		peiskThread.unregisterCallback(tupleCallback);
	}

	@Override
	public void peisk_unregisterTupleCallback(PeisCallbackHandle handle) {
		// TODO Auto-generated method stub
		
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

	@Override
	public int peisk_reloadSubscription(PeisSubscriberHandle subscriber) {
		// TODO Auto-generated method stub
		return 0;
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

	@Override
	public void peisk_resultSetFirst(PeisTupleResultSet resultSet) {
		// TODO Auto-generated method stub
		
	}

	/**
	 * @deprecated Functionality provided by {@link PeisTupleResultSet#next()}.
	 */
	@Deprecated
	@Override
	public int peisk_resultSetNext(PeisTupleResultSet resultSet) {
		// TODO Auto-generated method stub
		return 0;
	}

	/**
	 * @deprecated Functionality provided by {@link PeisTupleResultSet#reset()}.
	 */
	@Deprecated
	@Override
	public void peisk_resultSetReset(PeisTupleResultSet resultSet) {
		// TODO Auto-generated method stub
		
	}

	/**
	 * @deprecated Functionality provided by {@link PeisTupleResultSet#next()}.
	 */
	@Deprecated
	@Override
	public PeisTuple peisk_resultSetValue(PeisTupleResultSet resultSet) {
		// TODO Auto-generated method stub
		return null;
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
	public static int peisjava_setIndirectStringTuple(String metaKey, int metaOwner, String value) {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_setIndirectStringTuple(metaKey, metaOwner, value);
		}
	}
	
	@Override
	public int peisk_setIndirectStringTuple(String metaKey, int metaOwner,
			String value) {
		// TODO Auto-generated method stub
		return 0;
	}

	/**
	 * @deprecated Functionality provided by {@link #peisjava_setStringTuple(int, String, String)}.
	 */
	@Deprecated
	@Override
	public void peisk_setRemoteTuple(String key, int owner, int len,
			Pointer data) {
		// TODO Auto-generated method stub
		
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
	
	@Override
	public void peisk_setStringTuple(String key, String value) {
		// TODO Auto-generated method stub
		
	}

	/**
	 * Simplifying wrapper for creating and setting a tuple in local tuplespace,
	 * propagating it to all subscribers. Provides backwards
	 * compatibility with kernel G3 and earlier.
	 * @param key The key of the tuple to set.
	 * @param len The length of the data to set.
	 * @param data The data to write into the tuple.
	 */
	public static void peisjava_setTuple(String key, int len, Pointer data) {
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_setTuple(key, len, data);
		}
	}

	@Override
	public void peisk_setTuple(String key, int len, Pointer data) {
		// TODO Auto-generated method stub
		
	}

	/**
	 * Set the nameof a tuple.
	 * @param tup The tuple to modify. 
	 * @param fullname The new name for this tuple.
	 * @return 0 iff the operation was successful.
	 */
	public static int peisjava_setTupleName(PeisTuple tup, String fullname) {
		synchronized(peiskCriticalSection) {
			return ((PeisJavaInterface)PeisJavaMT.INSTANCE).peisk_setTupleName(tup, fullname);
		}
	}
	
	@Override
	public int peisk_setTupleName(PeisTuple tup, String fullname) {
		// TODO Auto-generated method stub
		return 0;
	}

	/**
	 * Stops the running peis kernel.
	 */
	public static void peisjava_shutdown() {
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_shutdown();
		}
	}

	@Override
	public void peisk_shutdown() {
		// TODO Auto-generated method stub
		
	}

	/**
	 * Wrapper for subscribing to tuples with given key from given owner
	 * (or -1 for wildcard on owner).
	 * @param key The key of the tuple to subscribe.
	 * @param owner The owner of the tuple to subscribe (-1 for wildcard).
	 */
	public static PeisSubscriberHandle peisjava_subscribe(String key, int owner) {
		synchronized(peiskCriticalSection) {
			return ((PeisJavaInterface)PeisJavaMT.INSTANCE).peisk_subscribe(key, owner);
		}
		
	}
	
	@Override
	public PeisSubscriberHandle peisk_subscribe(String key, int owner) {
		// TODO Auto-generated method stub
		return null;
	}

	/**
	 * Subscribe using given abstract tuple as prototype.
	 * @param tuple The abstract tuple to use as prototype. 
	 * @return The handle for this tuple subscription if success, {@code null} otherwise.
	 */
	public static PeisSubscriberHandle peisjava_subscribeAbstract(PeisTuple tuple) {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_subscribeAbstract(tuple);
		}
	}

	@Override
	public PeisSubscriberHandle peisk_subscribeAbstract(PeisTuple tuple) {
		// TODO Auto-generated method stub
		return null;
	}

	/**
	 * Creates a subscription to the meta tuple given by metaKey and metaOwner.
	 * Neither metaKey nor metaOwner may contain wildcard.
	 * @param metaKey The key of the meta tuple to subscribe to.
	 * @param metaOwner The owner of the meta tuple to subscribe to.
	 * @return The subscription handle, {@code null} if the operation fails.
	 */	
	public static PeisSubscriberHandle peisjava_subscribeIndirect(String metaKey,
			int metaOwner) {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_subscribeIndirect(metaKey, metaOwner);	
		}
	}

	@Override
	public PeisSubscriberHandle peisk_subscribeIndirect(String metaKey,
			int metaOwner) {
		// TODO Auto-generated method stub
		return null;
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
	public static PeisSubscriberHandle peisjava_subscribeIndirectTuple(PeisTuple M) {
		synchronized(peiskCriticalSection) {
			return INSTANCE.peisk_subscribeIndirectTuple(M);
		}
	}
	
	@Override
	public PeisSubscriberHandle peisk_subscribeIndirectTuple(PeisTuple M) {
		// TODO Auto-generated method stub
		return null;
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
	
	@Override
	public boolean peisk_tupleExists(String key) {
		// TODO Auto-generated method stub
		return false;
	}
 
	/**
	 * @deprecated Functionality provided by {@link PeisTuple#isAbstract()}.
	 */
	@Deprecated
	@Override
	public boolean peisk_tupleIsAbstract(PeisTuple tuple) {
		// TODO Auto-generated method stub
		return false;
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

	@Override
	public String peisk_tuple_strerror(int error) {
		// TODO Auto-generated method stub
		return null;
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

	@Override
	public boolean peisk_unsubscribe(PeisSubscriberHandle handle) {
		// TODO Auto-generated method stub
		return true;
	}

	/**
	 * Sets the meta tuple to point to given real tuple.
	 * @param metaOwner The owner of the meta tuple.
	 * @param metaKey The key of the meta tuple.
	 * @param realOwner The owner of the real tuple.
	 * @param realKey The key of the real tuple.
	 */
	public static void peisjava_writeMetaTuple(int metaOwner, String metaKey,
			int realOwner, String realKey) {
		synchronized(peiskCriticalSection) {
			INSTANCE.peisk_writeMetaTuple(metaOwner, metaKey, realOwner, realKey);
		}
	}

	@Override
	public void peisk_writeMetaTuple(int metaOwner, String metaKey,
			int realOwner, String realKey) {
		// TODO Auto-generated method stub
		
	}
	
	/**
	 * @deprecated Functionality provided by {@link PeisTuple#toString()}.
	 */
	@Deprecated
	@Override
	public void peisk_printTuple(PeisTuple tuple) {
		// TODO Auto-generated method stub
		
	}


	/*
	 * Functions that are only present in the single thread version of the peiskernel
	 * and are not wrapped in peiskernel_mt (should be even less necessary than the
	 * above peisk_xxx methods). Calling these functions is still thread-safe, but is
	 * discouraged as some are wrapped by the peisjava_xxx methods (i.e., calling them may
	 * lead to peiskernel inconsistencies) and others because they are simply _very_ low level.
	 */
	
	@Override
	public boolean peisk_broadcast(int port, int len, Pointer data) {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public boolean peisk_broadcastSpecial(int port, int len, Pointer data,
			int flags) {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public boolean peisk_broadcastSpecialFrom(int from, int port, int len,
			Pointer data, int flags) {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public void peisk_getrawtime2(LongByReference t0, LongByReference t1) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public double peisk_getrawtimef() {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public void peisk_registerDeadhostHook(PeisDeadhostHook fn, Pointer arg) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public boolean peisk_registerHook(int port, PeisHook hook) {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public boolean peisk_sendMessage(int port, int destination, int len,
			Pointer data, int flags) {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public boolean peisk_sendMessageFrom(int from, int port, int destination,
			int len, Pointer data, int flags) {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public void peisk_step() {
		// TODO Auto-generated method stub
		
	}

	@Override
	public boolean peisk_unregisterHook(int port, PeisHook hook) {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public void peisk_wait(int useconds) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void peisk_waitOneCycle(int maxUSeconds) {
		// TODO Auto-generated method stub
		
	}
	
	private static int processOptions(FLAG... flags) {
		/*
		//7 : no-nulls, repeating 		---> blocking, (no-old-val-filtering)		---> BLOCKING
		//2 : no-nulls, non-repeating	---> blocking, old-val-filtering			---> BLOCKING, VALUE_FILTERING
		//1 : nulls, repeating			---> (no-blocking), (no-old-val-filtering)	---> (nothing)
		//? : null, non-repeating
    	if (Arrays.asList(flags).contains(FLAG.BLOCKING) && Arrays.asList(flags).contains(FLAG.VALUE_FILTERING)) return 2;
    	if (Arrays.asList(flags).contains(FLAG.BLOCKING)) return 7;
		return 1;
		*/
		int bitwise = 1;
		if (Arrays.asList(flags).contains(FLAG.BLOCKING)) bitwise = bitwise + 2;
		if (Arrays.asList(flags).contains(FLAG.FILTER_OLD)) bitwise = bitwise - 1;
		return bitwise;
	}

}
