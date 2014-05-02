package examples;

import java.util.ArrayList;
import core.CallbackObject;
import core.PeisJavaInterface.PeisSubscriberHandle;
import core.PeisJavaMT;
import core.PeisTuple;
import core.PeisTupleResultSet;
import core.WrongPeisKernelVersionException;

public class RegisteringCallbacksExample {

	public static void main(String[] args) {
		try {
			PeisJavaMT.peisjava_initialize(args);
		} catch (WrongPeisKernelVersionException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
			System.exit(1);
		}
		System.out.println("Hello there, I am: " + PeisJavaMT.peisjava_peisid());

		PeisSubscriberHandle hndl = PeisJavaMT.peisjava_subscribe("RFIDTags", -1);
		
		//Find it
		PeisTupleResultSet result = null;
		while((result = PeisJavaMT.peisjava_findTuples("RFIDTags")) == null) { }
		PeisTuple tuple = result.next();
		PeisJavaMT.peisjava_registerTupleCallback(
							  tuple.getKey(),
							  tuple.owner,
							  new CallbackObject() {
							      private int numInvocations = 0;
							      @Override
								  public void callback(PeisTuple tuple) {
								  if (tuple.getStringData() != null && !tuple.getStringData().equals("") && !tuple.getStringData().equals("()")) {
								      System.out.println("####### TAG: " + tuple.getStringData());
								  }
							      }
							  } //End CallbackObject definition
							  );
		
	}
}
