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

import core.InvalidAbstractTupleUseException;
import core.PeisJavaMT;
import core.PeisTuple;
import core.WrongPeisKernelVersionException;
import core.PeisJavaInterface.PeisSubscriberHandle;

public class GettingTuplesBADExample {

	public static void main(String[] args) {
		try {
			PeisJavaMT.peisjava_initialize(args);
		} catch (WrongPeisKernelVersionException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
			System.exit(1);
		}
		System.out.println("Hello there, I am: " + PeisJavaMT.peisjava_peisid());
        
        //Get someone else's tuple (start up SettingTuplesExample
        //first if you want to see something here)
        PeisSubscriberHandle hndl = PeisJavaMT.peisjava_subscribe(-1, "aTuple");

        //This is WRONG!!  If you want to find a tuple with a wildcard in
        //the key and/or in the owner, then you should use peisjava_getTuplesByAbstract() (plural!) 
        PeisTuple abs = new PeisTuple();
        PeisJavaMT.peisjava_initAbstractTuple(abs);
        PeisTuple result = null;
        //... here we are punished by an exception ;)
		try {
			result = PeisJavaMT.peisjava_getTupleByAbstract(abs);
		} catch (InvalidAbstractTupleUseException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
        
        System.out.println("Found tuple: " + result);
        
        //Unsubscribe it
        PeisJavaMT.peisjava_unsubscribe(hndl);
        
	}
}
