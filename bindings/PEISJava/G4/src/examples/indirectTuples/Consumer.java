package examples.indirectTuples;

import java.util.Random;

import core.CallbackObject;
import core.PeisJavaMT;
import core.PeisTuple;
import core.PeisTupleResultSet;
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
		PeisSubscriberHandle hndl = PeisJavaMT.peisjava_subscribeIndirect("sensorReference", 9999);
		PeisTupleResultSet result = null;
		while ((result = PeisJavaMT.peisjava_findTuples("sensorReference")) == null) {
			System.out.print(".");
			try {
				Thread.sleep(200);
			} catch (InterruptedException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		}
		System.out.print(" Found metaTuple sensorReference\n");
		PeisTuple source = PeisJavaMT.peisjava_getIndirectTuple("sensorReference", 9999);
		
		/*
		 * Register a callback object that does something with the
		 * real tuple values as they are updated.  The real tuple is
		 * subscribed to through the configurator's META tuple.
		 */
		PeisJavaMT.peisjava_registerTupleCallback(
				source.getKey(),
				source.owner,
				//Begin CallbackObject definition
				//(as a local (anonymous) class)
				new CallbackObject() {
					private int numInvocations = 0;
					@Override
					public void callback(PeisTuple tuple) {
						System.out.println("####### Invocation " + (++numInvocations) + " #######");
						System.out.println(PeisJavaMT.peisjava_getIndirectTuple("sensorReference", 9999).getStringData());
						System.out.println("############################");
					}
				} //End CallbackObject definition
		);
		
	}
}
