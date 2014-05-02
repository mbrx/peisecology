package examples;

import core.PeisJavaMT;
import core.PeisTuple;
import core.PeisTupleResultSet;
import core.WrongPeisKernelVersionException;
import core.PeisJavaMT.FLAG;

public class GettingMultipleTuplesExample {

	public static void main(String[] args) {
		try {
			PeisJavaMT.peisjava_initialize(args);
		} catch (WrongPeisKernelVersionException e1) {
			// TODO Auto-generated catch block
			e1.printStackTrace();
			System.exit(1);
		}
		System.out.println("Hello there, I am: " + PeisJavaMT.peisjava_peisid());
		PeisJavaMT.peisjava_subscribe(260, "kernel.*");
		PeisTupleResultSet result = PeisJavaMT.peisjava_getTuples(260, "kernel.*", FLAG.BLOCKING);
		System.out.println("HERE");
		PeisTuple res = null;
		while ((res = result.next()) != null) {
			System.out.println("Tuple: " + res.getStringData());
		}


	}
	
}
