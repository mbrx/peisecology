/*
    Copyright (C) 2008 - 2011 Federico Pecora
    
    Based on libpeiskernel (Copyright (C) 2005 - 2011  Mathias Broxvall).
    
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


import java.util.Date;

import com.sun.jna.Memory;
import com.sun.jna.Pointer;
import com.sun.jna.Structure;
import com.sun.jna.ptr.PointerByReference;

import core.PeisJavaMT.ENCODING;

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
	public int[] ts_write;

	/**
	 * Get the expiration time of this tuple.
	 */
	public int[] getTs_expire() {
		return ts_expire;
	}

	/**
	 * Set the expiration time of this tuple.
	 * @param tsExpire The new expiration time of this tuple.
	 */
	public void setTs_expire(int[] tsExpire) {
		ts_expire = tsExpire;
	}

	/**
	 * Get the mime type of this tuple.
	 * @return The mime type of this tuple (e.g., text/plain if this tuple was set with setStringTuple()).
	 */
	/*
	public String getMimeType() {
		if (data == null) return null;
		String stringData = data.getString(0);
		if (datalen == stringData.length()+1)
			return "text/plain";
		return stringData;
	}
	*/
	public String getMimetype() {
		//return new String(mimetype.getString(0));
		return mimetype;
	}

	/**
	 * Set the mime type of this tuple.
	 * @param mimetype The new mime type of this tuple.
	 */
	public void setMimetype(String mimetype) {
		/*
		Memory m = new Memory(mimetype.length()+1);
		m.write(0, mimetype.getBytes(), 0, mimetype.length());
		this.mimetype = m;
		*/
		this.mimetype = mimetype;
	}

	/**
	 * Returns {@code true} iff this tuple is new.
	 * @return {@code true} iff this tuple is new.
	 */
	public boolean isNew() {
		return (isNew == 1);
	}

	/**
	 * Sets the {@code isNew} flag of this tuple to 1 if arg is {@code true}, 0 otherwise.
	 * @param isNew Whether the tuple is to be considered new.
	 */
	public void setNew(boolean isNew) {
		if (isNew) this.isNew = 1;
		else this.isNew = 0;
	}

	/**
	 * Sets the encoding of this tuple.
	 * @param encoding The encoding of this tuple.
	 */
	public void setEncoding(ENCODING enc) {
		if (enc.equals(ENCODING.ASCII)) encoding = 0;
		else if (enc.equals(ENCODING.BINARY)) encoding = 1;
		else encoding = -1;
	}

	/**
	 * Timestamp as assigned by user or -1 as wildcard.
	 */
	public int[] ts_user;

	/**
	 * Timestamp when tuple expires. Zero expires never, -1 as wildcard.
	 */
	public int[] ts_expire;

	/** The encoding of this tuple (binary or ASCII). */
	public int encoding;

	/** The mimetype of this tuple. */
	public String mimetype;

	/**
	 * Data of tuple or {@code null} if wildcard.
	 */
	public PointerByReference data;

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

	/** Appended sequence number incremented by owner when setting the tuple,
	 *  internal use only. */
	public int appendSeqNo;
	
	/**
	 * Create a new tuple.  The preferred method for creating tuples are the factory
	 * methods {@link PeisJavaMT#peisjava_createPeisTuple(String name)},
	 * {@link PeisJavaMT#peisjava_createAbstractPeisTuple(String name)} and
	 * {@link PeisJavaMT#peisjava_createMetaPeisTuple(String metaKey, String realKey, int realOwner)}
	 * as they also takes care of tuple initialization.
	 */
	public PeisTuple() {
		ts_write = new int[2];
		ts_user = new int[2];
		ts_expire = new int[2];
		datalen = -1;
		keys = new Pointer[7];
		alloclen = 0;
		keybuffer = new byte[128];
		data = new PointerByReference();
		encoding = -1;
		mimetype = "";
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
	@Override
	public String toString() {
		int len = 80*20+1;
		Memory m = new Memory(len);
		synchronized(PeisJavaMT.peiskCriticalSection) {
			PeisJavaMT.INSTANCE.peisk_snprintTuple(m, len, this);
			/*
			CInterface cinterface = (CInterface) Native.loadLibrary("c", CInterface.class);
			Pointer stdout = NativeLibrary.getInstance("c").getFunction("stdout").getPointer(0);
			String dump = stdout.getString(0);
			PeisJavaMT.INSTANCE.peisk_printTuple(this);
			cinterface.fflush(stdout);
			return stdout.getString(0).substring(dump.length());
			*/
		}
		return m.getString(0);
	}
	
	/**
	 * Clone this tuple (including the data field).
	 * @return The new tuple.
	 */
	@Override
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
			return this.data.getPointer().getString(0);
		return null;
	}
	
	/**
	 * Get the data in this tuple as a byte array. 
	 * @return A byte array containing the data in this tuple.
	 */
	public byte[] getByteData() {
		if (this.data != null)
			return this.data.getPointer().getByteArray(0, this.datalen);
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
	 * Returns the owner of this tuple.
	 * @return The owner of this tuple.
	 */
	public int getOwner() {
		return this.owner;
	}
	
	/**
	 * Set the owner of this tuple.
	 * @param owner The owner of this tuple.
	 */
	public void setOwner(int owner) {
		this.owner = owner;
	}
	
	
	/**
	 * Get the encoding of this tuple.
	 * @return The encoding of this tuple.
	 */
	public PeisJavaMT.ENCODING getEncoding() {
		if (encoding == -1) return null;
		if (encoding == 0) return ENCODING.ASCII;
		return ENCODING.BINARY;
	}
	
	/**
	 * Set the data of this tuple as a String.
	 * @param data The String containing the data.
	 */
	public void setStringData(String data) {
		Memory m = new Memory(data.length()+1);
		m.write(0, data.getBytes(), 0, data.length());
		byte[] terminator = {0x0};
		m.write(data.length(), terminator, 0, 1);
		if (this.data == null) this.data = new PointerByReference(); 
		this.data.setPointer(m);
		this.datalen = data.length()+1;
	}
	

	/**
	 * Set the data of this tuple as a byte array.
	 * @param data The byte array containing the data.
	 */
	public void setByteData(byte[] data) {
		Memory m = new Memory(data.length);
		m.write(0, data, 0, data.length);
		this.data.setPointer(m);
		this.datalen = data.length;
	}
	
	
	private final static Date getPeisDate(int[] ts) {

		if(ts[0] < 0 || ts[1] < 0) {
			return null; //Wildcard (-1) / invalid time stamp
		}
		
		if(ts[0] == 0 && ts[1] == 0) {
			return null; //Time stamp not set
		}
		
		//Convert ts[0] in s and ts[1] us to ms
		final long s = ts[0];
		final long us = ts[1];
		
		final long ms = (s*1000) + (us/1000);
		
		//This seems to work. The assumption is that the 
		//PEIS time is in UTC and that Java does the necessary adjustments.
		return new Date(ms);
	}
	
	/**
	 * @return The write time stamp as a {@link Date} or <code>null</code> in case the time is undefined.
	 */
	public Date getDateOfWrite() {
		return getPeisDate(ts_write);
	}
	
	/**
	 * @return The user time stamp as a {@link Date} or <code>null</code> in case the the time is undefined.
	 * <b>Notice that the user time stamp is not set by default and is irrelevant in most applications.</b>
	 * @see #getDateOfWrite()
	 * @see #getDateOfExpire()
	 */
	public Date getDateOfUser() {
		return getPeisDate(ts_user);
	}
	
	/**
	 * @return The expire time stamp as a {@link Date} or <code>null</code> in case the time is undefined.
	 */
	public Date getDateOfExpire() {
		return getPeisDate(ts_expire);
	}

}
