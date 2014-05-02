package examples.indirectTuples;

import java.io.DataInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.Arrays;
import java.util.Random;

import core.PeisJavaMT;
import core.PeisTuple;
import core.WrongPeisKernelVersionException;

public class ProducerA {

	
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
		 * Create a thread that publishes a result every so often
		 */ 
		Thread publisherThread = new Thread(new Runnable() {
			Random rand = new Random(12345);
			public void run() {
				while (true) {
					//Set a string tuple -- no mime type is set for String tuples.
					PeisJavaMT.peisjava_setStringTuple("sensorValueA",""+rand.nextInt(10));
					try {
						Thread.sleep(500);
					} catch (InterruptedException e) {
						// TODO Auto-generated catch block
						e.printStackTrace();
					}
				}
			}
		},"publisherThread");
		publisherThread.start();
	}
}
