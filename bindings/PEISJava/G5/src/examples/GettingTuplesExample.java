package examples;

import java.util.Calendar;
import java.util.GregorianCalendar;

import com.sun.jna.ptr.LongByReference;

import core.PeisJavaMT;
import core.PeisTuple;
import core.PeisTupleResultSet;
import core.WrongPeisKernelVersionException;
import core.PeisJavaInterface.PeisSubscriberHandle;

public class GettingTuplesExample {

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
        PeisSubscriberHandle hndl = PeisJavaMT.peisjava_subscribe("aTuple", -1);

        //Find it
        PeisTupleResultSet result = null;
        while((result = PeisJavaMT.peisjava_findTuples("aTuple")) == null) {/*wait until it's propagated*/}
        PeisTuple tuple = null;
        while ((tuple = result.next()) != null)
        	System.out.println(tuple.getKey() + ": " + tuple.getStringData());
        
        //Unsubscribe it
        PeisJavaMT.peisjava_unsubscribe(hndl);
        
	}
}
