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

import java.util.Arrays;

import core.PeisJavaMT;
import core.WrongPeisKernelVersionException;
import core.PeisJavaInterface.PeisSubscriberHandle;

public class Configurator {

	public static void main(String[] args) {
		/*
		 * INIT this peis component with a known ID
		 * (because it is the universally known "configurator")
		 */
		try {
			String newArgs[] = new String[args.length+2];
			for (int i = 0; i < args.length; i++)
				newArgs[i] = args[i];
			if (Arrays.binarySearch(args, "--peis-id") < 0) {
				newArgs[args.length] = "--peis-id";
				newArgs[args.length+1] = "9999";
			}
			PeisJavaMT.peisjava_initialize(newArgs);
		} catch (WrongPeisKernelVersionException e1) {
			// TODO Auto-generated catch block
			e1.printStackTrace();
			System.exit(1);
		}
		System.out.println("Hello there, I am: " + PeisJavaMT.peisjava_peisid());
		
		final String[] producers = {"sensorValueA", "sensorValueB"};
		final int[] producerIDs = new int[producers.length];
		/*
		 * Subscribe to the real tuples
		 */
		for (int i = 0; i < producers.length; i++) {
			PeisSubscriberHandle hndl = PeisJavaMT.peisjava_subscribe(-1, producers[i]);
			//PeisTuple producer = PeisJavaMT.peisjava_getTuple(owner, producers[i], PeisJavaMT.FLAG.BLOCKING);
			producerIDs[i] = PeisJavaMT.peisjava_findOwner(producers[i]);
			System.out.println("[Configurator] found owner of " + producers[i]);
		}

		/*
		 * Create a thread that switches the reference (META tuple) to the real sensor every so often
		 */  
		Thread referenceChangerThread = new Thread(new Runnable() {
			public void run() {
				int producer = 0;
				while (true) {
					PeisJavaMT.peisjava_setMetaTuple(PeisJavaMT.peisjava_peisid(), "sensorReference", producerIDs[producer], producers[producer]);
					try {
						Thread.sleep(5000);
					} catch (InterruptedException e) {
						// TODO Auto-generated catch block
						e.printStackTrace();
					}
					producer = (producer+1)%producers.length;
				}
			}
		},"referenceChangerThread");
		referenceChangerThread.start();
		

	}
}
