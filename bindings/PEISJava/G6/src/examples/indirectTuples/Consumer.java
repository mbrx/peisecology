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


package examples.indirectTuples;

import core.CallbackObject;
import core.PeisJavaMT;
import core.PeisTuple;
import core.WrongPeisKernelVersionException;
import core.PeisJavaInterface.PeisSubscriberHandle;

public class Consumer {

	public static void main(String[] args) {
		try {
			PeisJavaMT.peisjava_initialize(args);
		} catch (WrongPeisKernelVersionException e1) {
			// TODO Auto-generated catch block
			e1.printStackTrace();
			System.exit(1);
		}
		System.out.println("Hello there, I am: " + PeisJavaMT.peisjava_peisid());

		/*
		 * Subscribe and find configurator tuple
		 * (we also know the ID of the configurator)
		 */
		
		//Find peisID of configurator
		PeisSubscriberHandle hndl = PeisJavaMT.peisjava_subscribe(-1, "sensorReference");
		int metaOwner = PeisJavaMT.peisjava_findOwner("sensorReference");
		
		//Subscribe to and get tuple pointed to by configurator
		PeisSubscriberHandle hndl1 = PeisJavaMT.peisjava_subscribeIndirectly(metaOwner, "sensorReference");
		PeisTuple source = PeisJavaMT.peisjava_getTupleIndirectly(metaOwner, "sensorReference", PeisJavaMT.FLAG.BLOCKING);
		System.out.print(" Found metaTuple sensorReference\n");
		
		/*
		 * Register a callback object that does something with the
		 * real tuple values as they are updated.  The real tuple is
		 * subscribed to through the configurator's META tuple.
		 */
		PeisJavaMT.peisjava_registerTupleCallback(
				source.owner,
				source.getKey(),
				//Begin CallbackObject definition
				//(as a local (anonymous) class)
				new CallbackObject() {
					private int numInvocations = 0;
					@Override
					public void callback(PeisTuple tuple) {
						System.out.println("####### Invocation " + (++numInvocations) + " #######");
						System.out.println(PeisJavaMT.peisjava_getTupleIndirectly(9999, "sensorReference").getStringData());
						System.out.println("############################");
					}
				} //End CallbackObject definition
		);
		
	}
}
