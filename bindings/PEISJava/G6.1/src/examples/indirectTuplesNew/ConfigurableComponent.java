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


package examples.indirectTuplesNew;

import java.util.Arrays;

import core.CallbackObject;
import core.PeisJavaMT;
import core.PeisTuple;
import core.WrongPeisKernelVersionException;
import core.PeisJavaInterface.PeisSubscriberHandle;

public class ConfigurableComponent {

	private static PeisTuple sourceSep = null;	
	
	public static void main(String[] args) {
		/*
		 * INIT this peis component with a known ID
		 * (because it is the universally known "configurator")
		 */
		try {
			PeisJavaMT.peisjava_initialize(args);
		} catch (WrongPeisKernelVersionException e1) {
			// TODO Auto-generated catch block
			e1.printStackTrace();
			System.exit(1);
		}
		System.out.println("Hello there, I am: " + PeisJavaMT.peisjava_peisid());

		PeisJavaMT.peisjava_setMetaTuple(PeisJavaMT.peisjava_peisid(), "referenceToStringInput", -1, "nil");
		PeisJavaMT.peisjava_setMetaTuple(PeisJavaMT.peisjava_peisid(), "referenceToSeparatorString", -1, "nil");
		
		
		PeisSubscriberHandle hndl1 = PeisJavaMT.peisjava_subscribeIndirectly(PeisJavaMT.peisjava_peisid(), "referenceToStringInput");
		PeisTuple source = null;
		while (source == null) {
			source = PeisJavaMT.peisjava_getTupleIndirectly(PeisJavaMT.peisjava_peisid(), "referenceToStringInput");
			try {
				Thread.sleep(100);
			} catch (InterruptedException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		}
		System.out.print(" Found metaTuple referenceToStringInput\n");

		PeisSubscriberHandle hndl2 = PeisJavaMT.peisjava_subscribeIndirectly(PeisJavaMT.peisjava_peisid(), "referenceToSeparatorString");
		while (sourceSep == null) {
			sourceSep = PeisJavaMT.peisjava_getTupleIndirectly(PeisJavaMT.peisjava_peisid(), "referenceToSeparatorString");
			try {
				Thread.sleep(100);
			} catch (InterruptedException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		}
		System.out.print(" Found metaTuple referenceToSeparatorString\n");

		//final String separatorString = sourceSep.getStringData();
		
		PeisJavaMT.peisjava_registerTupleCallback(
				source.owner,
				source.getKey(),
				//Begin CallbackObject definition
				//(as a local (anonymous) class)
				new CallbackObject() {
					@Override
					public void callback(PeisTuple tuple) {
						String data = tuple.getStringData();
						if (sourceSep != null) {
							String[] newData = data.split(sourceSep.getStringData());
							PeisJavaMT.peisjava_setStringTuple("SplitString", Arrays.toString(newData));
						}
					}
				} //End CallbackObject definition
		);
		
		
		

	}
}
