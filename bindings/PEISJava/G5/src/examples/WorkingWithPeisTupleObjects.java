package examples;

import java.util.Arrays;

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
		
		//Tuples should be created with the factory method and not
		//instantiated directly (this avoids you having to do initialization).
		//Also, the factory method will return null if a tuple cannot be
		//instantiated (e.g., tuple with same (name,owner) exists).
		PeisTuple tuple1 = PeisJavaMT.peisjava_createPeisTuple("abcd");
		PeisTuple tuple2 = PeisJavaMT.peisjava_createPeisTuple("bcde");
		PeisTuple tuple3 = PeisJavaMT.peisjava_createPeisTuple("abcd.def");
		PeisTuple tuple4 = PeisJavaMT.peisjava_createPeisTuple("abcd.*");
		PeisTuple tuple5 = PeisJavaMT.peisjava_createPeisTuple("abcd");

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

		//Since PeisTuple implements Comparable, tuples
		//can be sorted through the Arrays.sort() method
		PeisTuple[] allTuples = new PeisTuple[5];
		allTuples[0] = tuple1;
		allTuples[1] = tuple2;
		allTuples[2] = tuple3;
		allTuples[3] = tuple4;
		allTuples[4] = tuple5;
		Arrays.sort(allTuples);
		System.out.println("Sorted tuples:\n===============");
		for (PeisTuple tup : allTuples)
			System.out.println(tup.getKey() + ": " + tup.getStringData());
		System.out.println("===============");
		
	}
}
