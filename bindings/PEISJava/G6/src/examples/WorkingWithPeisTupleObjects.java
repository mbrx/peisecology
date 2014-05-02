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

import java.util.Arrays;

import core.InvalidAbstractTupleUseException;
import core.PeisJavaMT;
import core.PeisTuple;
import core.WrongPeisKernelVersionException;

public class WorkingWithPeisTupleObjects {

	public static void main(String[] args) {
		try {
			PeisJavaMT.peisjava_initialize(args);
		} catch (WrongPeisKernelVersionException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
			System.exit(1);
		}
		System.out.println("Hello there, I am: " + PeisJavaMT.peisjava_peisid());
		
		//Tuples created like this exist only in RAM,
		//they are not automatically inserted into the tuple space
		//(therefore you will not see them in tupleview).
		PeisTuple tuple1 = new PeisTuple();
		PeisJavaMT.peisjava_initTuple(tuple1);
		PeisJavaMT.peisjava_setTupleName(tuple1, "abcd");
		tuple1.setStringData("blabla");

		PeisTuple tuple2 = new PeisTuple();
		PeisJavaMT.peisjava_initTuple(tuple2);
		PeisJavaMT.peisjava_setTupleName(tuple2, "bcde");
		tuple2.setStringData("blabla");
		
		PeisTuple tuple3 = new PeisTuple();
		PeisJavaMT.peisjava_initTuple(tuple3);
		PeisJavaMT.peisjava_setTupleName(tuple3, "abcd.def");
		tuple3.setStringData("blabla");
		
		PeisTuple tuple4 = new PeisTuple();
		PeisJavaMT.peisjava_initTuple(tuple4);
		PeisJavaMT.peisjava_setTupleName(tuple4, "abcd.*");
		tuple4.setStringData("blabla");
		
		PeisTuple tuple5 = new PeisTuple();
		PeisJavaMT.peisjava_initTuple(tuple5);
		PeisJavaMT.peisjava_setTupleName(tuple5, "efghijk");
		tuple5.setStringData("blabla");

		//Since PeisTuple implements Comparable, tuples
		//can be sorted through the Arrays.sort() method
		PeisTuple[] allTuples = new PeisTuple[5];
		allTuples[0] = tuple1;
		allTuples[1] = tuple2;
		allTuples[2] = tuple3;
		allTuples[3] = tuple4;
		allTuples[4] = tuple5;
		
		System.out.println("Unsorted tuples:\n===============");
		for (PeisTuple tup : allTuples)
			System.out.println(tup + "\n--------");
		System.out.println("===============");
		
		//Comparison methods provided by PeisTuple class
		System.out.println("tuple1 ?= tuple2: " + (tuple1.equals(tuple2)));
		System.out.println("tuple1 ?= tuple2: " + (tuple1.equals(tuple1)));
		System.out.println("tuple1 ?= tuple2: " + (tuple1.equals(tuple5)));
		System.out.println("tuple1 ?< tuple2: " + (tuple2.compareTo(tuple1)));
		System.out.println("tuple2 ?< tuple1: " + (tuple1.compareTo(tuple2)));
		System.out.println("tuple1 ?< tuple3: " + (tuple1.compareTo(tuple3)));
		System.out.println("tuple1 ?gen tuple3: " + (tuple1.isGeneralization(tuple3)));
		System.out.println("tuple1 ?gen tuple2: " + (tuple1.isGeneralization(tuple2)));
		System.out.println("tuple3 ?gen tuple4: " + (tuple3.isGeneralization(tuple4)));
		System.out.println("tuple4 ?gen tuple3: " + (tuple4.isGeneralization(tuple3)));
		
		//Sort them (calls the compareTo() method)
		Arrays.sort(allTuples);
		
		System.out.println("Sorted tuples:\n===============");
		for (PeisTuple tup : allTuples)
			System.out.println(tup + "\n--------");
		System.out.println("===============");

		//Now if I insert them they will be in the tuplespace and
		//we will see then in Tupleview
		//(note also that you have to catch an exception when
		//inserting a tuple - one of the above will in fact trigger the exception)
		for (PeisTuple tup : allTuples) {
			try { PeisJavaMT.peisjava_insertTuple(tup); }
			catch (InvalidAbstractTupleUseException e) { e.printStackTrace(); }
		}
				
	}
}
