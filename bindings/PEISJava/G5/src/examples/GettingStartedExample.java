package examples;

import core.PeisJavaMT;
import core.WrongPeisKernelVersionException;

public class GettingStartedExample {

   public static void main(String[] args) {
      try {
		PeisJavaMT.peisjava_initialize(args);
	} catch (WrongPeisKernelVersionException e) {
		// TODO Auto-generated catch block
		e.printStackTrace();
		System.exit(1);
	}
      System.out.println("Hello there, I am: " + PeisJavaMT.peisjava_peisid());
        
      //Set a String tuple
      PeisJavaMT.peisjava_setStringTuple("myTuple","myStringValue");

      //Get my own String tuple
      String tupleData = PeisJavaMT.peisjava_getStringTuple("myTuple");
      System.out.println("The tuple 'myTuple' contains: " + tupleData);
   }
}
