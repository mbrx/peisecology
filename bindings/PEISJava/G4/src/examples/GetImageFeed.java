package examples;

import java.awt.image.BufferedImage;
import java.util.Calendar;
import java.util.GregorianCalendar;

import com.sun.jna.ptr.LongByReference;

import core.PeisJavaMT;
import core.PeisTuple;
import core.PeisTupleResultSet;
import core.WrongPeisKernelVersionException;
import core.PeisJavaInterface.PeisSubscriberHandle;

public class GetImageFeed {

	public static void main(String[] args) {
		try {
			PeisJavaMT.peisjava_initialize(args);
		} catch (WrongPeisKernelVersionException e1) {
			// TODO Auto-generated catch block
			e1.printStackTrace();
			System.exit(1);
		}
		System.out.println("Hello there, I am: " + PeisJavaMT.peisjava_peisid());
        
        //Get someone else's tuple (start up SettingTuplesExample
        //first if you want to see something here)
        PeisSubscriberHandle hndl = PeisJavaMT.peisjava_subscribe("anImageTuple", -1);
		
        while (hndl == null) {
        	
        	System.out.println("waiting...");
	        try {
				Thread.sleep(100);
			} catch (InterruptedException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
			
        }
        		
        while (true) {
        	byte[] image = PeisJavaMT.peisjava_getByteTuple(2000, "anImageTuple");
	        System.out.println("Length of image = " + image.length);
	        
	        try {
				Thread.sleep(100);
			} catch (InterruptedException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
        }
         
        //Unsubscribe it
        //PeisJavaMT.peisjava_unsubscribe(hndl);
        
	}
}
