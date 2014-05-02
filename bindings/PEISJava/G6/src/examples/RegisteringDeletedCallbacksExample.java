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


package examples;

import core.CallbackObject;
import core.PeisJavaMT;
import core.PeisTuple;
import core.WrongPeisKernelVersionException;
import core.PeisJavaInterface.PeisSubscriberHandle;

public class RegisteringDeletedCallbacksExample {

	public static void main(String[] args) {
		try {
			PeisJavaMT.peisjava_initialize(args);
		} catch (WrongPeisKernelVersionException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
			System.exit(1);
		}
		System.out.println("Hello there, I am: " + PeisJavaMT.peisjava_peisid());

		PeisSubscriberHandle hndl = PeisJavaMT.peisjava_subscribe(-1, "RFIDTags");
		int owner = PeisJavaMT.peisjava_findOwner("RFIDTags");
		PeisTuple tuple = PeisJavaMT.peisjava_getTuple(owner, "RFIDTags");
		
		PeisJavaMT.peisjava_registerTupleDeletedCallback(
							  tuple.owner,
							  tuple.getKey(),
							  new CallbackObject() {
							      private int numInvocations = 0;
							      @Override
								  public void callback(PeisTuple tuple) {
								      System.out.println("The tuple " + tuple.getKey() + " has been deleted!");
							      }
							  } //End CallbackObject definition
							  );
		
	}
}
