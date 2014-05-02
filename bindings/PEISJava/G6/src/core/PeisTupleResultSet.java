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

import com.sun.jna.Pointer;
import com.sun.jna.Structure;

/**
 * Class for maintaining result sets returned by {@link PeisJavaMT#peisjava_findTuples(String)}.
 * @author fpa
 *
 */
public class PeisTupleResultSet extends Structure {
	public Pointer tuples;
	public int index;
	public int nTuples;
	public int maxTuples;
//	protected void finalize() throws Throwable {
//		try { PeisJavaMT.INSTANCE.peisk_deleteResultSet(this); }
//		finally { super.finalize(); }
//	}
	
	/**
	 * Get the next result in the result set.
	 * @return The next tuple in the result set, {@code null} if no more tuples exist.
	 */
	public PeisTuple next() {
		synchronized(PeisJavaMT.peiskCriticalSection) {
			if (PeisJavaMT.INSTANCE.peisk_resultSetNext(this) == 0) return null;
			PeisTuple ret = PeisJavaMT.INSTANCE.peisk_resultSetValue(this);
			return ret;
		}
	}
	
	/**
	 * Reset this result set to point to the first tuple.
	 */
	public void reset() {
		synchronized(PeisJavaMT.peiskCriticalSection) {
			PeisJavaMT.INSTANCE.peisk_resultSetReset(this);
		}
	}

}
