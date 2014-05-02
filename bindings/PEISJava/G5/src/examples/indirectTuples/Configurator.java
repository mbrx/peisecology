package examples.indirectTuples;

import java.util.Arrays;
import java.util.Random;

import core.PeisJavaMT;
import core.PeisTupleResultSet;
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
			PeisSubscriberHandle hndl = PeisJavaMT.peisjava_subscribe(producers[i], -1);
			PeisTupleResultSet result = null;
			while ((result = PeisJavaMT.peisjava_findTuples(producers[i])) == null) {
				System.out.print(".");
				try {
					Thread.sleep(200);
				} catch (InterruptedException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				}
			}
			System.out.print(" Found tuple " + producers[i] + "\n");
			producerIDs[i] = result.next().owner;
		}

		/*
		 * Create META tuple that references the real tuple 
		 */
		PeisJavaMT.peisjava_createMetaPeisTuple("sensorReference", "?", -1);
		//Alternatively:
		//PeisJavaMT.peisjava_setStringTuple("sensorReference", "(META -1 ?)");
		
		/*
		 * Create a thread that switches the reference to the real sensor every so often
		 */  
		Thread referenceChangerThread = new Thread(new Runnable() {
			public void run() {
				int producer = 0;
				while (true) {
					//Set a string tuple -- no mime type is set for String tuples.
					PeisJavaMT.peisjava_setStringTuple("sensorReference", "(META " + producerIDs[producer] + " " + producers[producer] + ")");
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
