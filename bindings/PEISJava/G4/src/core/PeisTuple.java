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

import com.sun.jna.NativeLibrary;
import com.sun.jna.NativeLong;
import com.sun.jna.Pointer;
import com.sun.jna.Structure;

/**
 * Represents a tuple in the local tuplespace. Also used to instantiate
 * (partial) tuple as used when doing searches in the local and in
 * the distributed tuple space. Wildcards on specific fields are
 * specified using {@code null} values (or -1 for integers).
 * The preferred method for creating tuples are the factory
 * methods {@link PeisJavaMT#peisjava_createPeisTuple(String name)},
 * {@link PeisJavaMT#peisjava_createAbstractPeisTuple(String name)} and
 * {@link PeisJavaMT#peisjava_createMetaPeisTuple(String metaKey, String realKey, int realOwner)}
 * as they also takes care of tuple initialization.
 * @author fpa
 *
 */
public class PeisTuple extends Structure implements Comparable<PeisTuple> {
	
	/**
	 * The owner (controlling peis) of the data represented by this tuple, or -1 as wildcard.
	 */
	public int owner;

	/**
	 * ID of Peis last writing to this tuple, or -1 as wildcard.
	 */
	public int creator;

	/**
	 * Timestamp assigned when first written into owner's tuplespace or -1 as wildcard.
	 */
	public NativeLong[] ts_write;

	/**
	 * Timestamp as assigned by user or -1 as wildcard.
	 */
	public NativeLong[] ts_user;

	/**
	 * Timestamp when tuple expires. Zero expires never, -1 as wildcard.
	 */
	public NativeLong[] ts_expire;

	/**
	 * Data of tuple or {@code null} if wildcard.
	 */
	public Pointer data;

	/**
	 * Used length of data for this tuple if data is non-null, otherwise -1.
	 */
	public int datalen;

	/*
	 * Private, placed here only to ensure proper memory allocation.
	 */

	/** All keys of this tuple. */
	public Pointer[] keys;

	/** Depth of key or -1 as wildcard. */
	public int keyDepth;

	/** Size of allocated data buffer. Usually only used by the tuple
		      space privately. Should be zero (uses datalen size instead) or a
		      value >= datalen */
	public int alloclen;

	/** Memory for storing the (sub)keys belonging to this tuple. Only
		      for internal use. */
	public byte[] keybuffer;

	/** True if tuple has not been read (locally) since last received
		      write. If used in a prototype, then any nonzero value means that
		      only new tuples will be accepted. (findTuple returns zero if not
		      new). */
	public int isNew;
	
	/** Sequence number incremented by owner when setting the tuple,
	 *  internal use only. */
	public int seqno;
	
	/**
	 * Create a new tuple.  The preferred method for creating tuples are the factory
	 * methods {@link PeisJavaMT#peisjava_createPeisTuple(String name)},
	 * {@link PeisJavaMT#peisjava_createAbstractPeisTuple(String name)} and
	 * {@link PeisJavaMT#peisjava_createMetaPeisTuple(String metaKey, String realKey, int realOwner)}
	 * as they also takes care of tuple initialization.
	 */
	public PeisTuple() {
		ts_write = new NativeLong[2];
		ts_user = new NativeLong[2];
		ts_expire = new NativeLong[2];
		datalen = -1;
		keys = new Pointer[7];
		alloclen = 0;
		keybuffer = new byte[128];
		data = null;
	}
	
	/**
	 * See this tuple is compatible with a given tuple, i.e., could be unified.
	 * This function is suitable to be used when sorting lists of tuples
	 * or getting the max/min of tuples as well as in the search routines.
	 * Note that the data field of tuples are compared using non case
	 * sensitive string compares. In the future this might be modifiable
	 * with tuple specific flags.
	 * @param arg0 The tuple to compare to.
	 * @return Zero if compatible, less then zero if this tuple is < arg0 and
	 * greater than zero if this tuple is > arg0.
	 */
	@Override
	public int compareTo(PeisTuple arg0) {
		synchronized(PeisJavaMT.peiskCriticalSection) {
			return PeisJavaMT.INSTANCE.peisk_compareTuples(this, arg0);
		}
	}

	/**
	 * Ascertain if this tuple is equal to a given tuple.
	 * @param arg0 The tuple to test against.
	 * @return {@code true} iff the two tuples are equal.
	 */
	@Override
	public boolean equals(Object arg0) {
		//check for self-comparison
		if (this == arg0) return true;
		if (!(arg0 instanceof PeisTuple)) return false;
		synchronized(PeisJavaMT.peiskCriticalSection) {
			return PeisJavaMT.INSTANCE.peisk_isEqual(this, (PeisTuple)arg0);
		}
	}
	
	/**
	 * Test if this tuple is abstract (contains any wildcards).
	 * @return {@code true} if the tuple is abstract.
	 */
	public boolean isAbstract() {
		synchronized(PeisJavaMT.peiskCriticalSection) {
			return PeisJavaMT.INSTANCE.peisk_tupleIsAbstract(this);
		} 		
	}
	
	/**
	 * Get a String representation of this tuple.
	 * @return A String describing this tuple.
	 */
	public String toString() {
		synchronized(PeisJavaMT.peiskCriticalSection) {
			Pointer stdout = NativeLibrary.getInstance ("c").getFunction("stdout").getPointer(0);
			String dump = stdout.getString(0);
			PeisJavaMT.INSTANCE.peisk_printTuple(this);
			return stdout.getString(0).substring(dump.length());
		}
	}
	
	/**
	 * Clone this tuple (including the data field).
	 * @return The new tuple.
	 */
	public PeisTuple clone() {
		synchronized(PeisJavaMT.peiskCriticalSection) {
			return PeisJavaMT.INSTANCE.peisk_cloneTuple(this);
		}
	}
	
	/**
	 * Similar to {@link #compareTo(PeisTuple)} but treats wildcards
	 * differently. 
	 * @param tuple The tuple to compare against.
	 * @return {@code true} if this tuple is a generalization or equal to
	 * the given tuple (e.g., this tuple has a wildcard or the same data as the given tuple in
	 * every field).
	 */
	public boolean isGeneralization(PeisTuple tuple) {
		synchronized(PeisJavaMT.peiskCriticalSection) {
			return PeisJavaMT.INSTANCE.peisk_isGeneralization(this, tuple);
		} 
	}
	
	/**
	 * Get the data in this tuple as a String. 
	 * @return A string containing the data in this tuple.
	 */
	public String getStringData() {
		if (this.data != null)
			return this.data.getString(0);
		return null;
	}
	
	/**
	 * Get the data in this tuple as a byte array. 
	 * @return A byte array containing the data in this tuple.
	 */
	public byte[] getByteData() {
		if (this.data != null)
			return this.data.getByteArray(0, this.datalen);
		return null;
	}
	
	/**
	 * Return the key of this tuple.
	 * @return The key of this tuple.
	 */
	public String getKey() {
		String ret = null;
		for (int i = 0; i < this.keyDepth; i++) {
			if (i == 0) ret = keys[i].getString(0);
			else if (keys[i] == null) ret += ".*";
			else ret += ("." + keys[i].getString(0));
		}
		return ret; 
	}
	
	/**
	 * Get the mime type of this tuple.
	 * @return The mime type of this tuple (text/plain if this is a String tuple).
	 */
	public String getMimeType() {
		if (data == null) return null;
		String stringData = data.getString(0);
		if (datalen == stringData.length()+1)
			return "text/plain";
		return stringData;
	}
}
