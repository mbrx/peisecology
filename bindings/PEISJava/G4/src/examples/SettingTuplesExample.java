package examples;

import java.io.DataInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;

import core.PeisJavaMT;
import core.PeisTuple;
import core.WrongPeisKernelVersionException;

public class SettingTuplesExample {
	
	public static void main(String[] args) {
		try {
			PeisJavaMT.peisjava_initialize(args);
		} catch (WrongPeisKernelVersionException e1) {
			// TODO Auto-generated catch block
			e1.printStackTrace();
			System.exit(1);
		}
		System.out.println("Hello there, I am: " + PeisJavaMT.peisjava_peisid());

        //Set a string tuple -- no mime type is set for String tuples.
		PeisJavaMT.peisjava_setStringTuple("aTuple","aStringValue");

		//Set a byte array tuple -- passing null mime type will append
		//"example/peis" mime type by default.
		byte[] byteData = (new String("aByteArrayValue")).getBytes();
		PeisJavaMT.peisjava_setByteArrayTuple(null, "anotherTuple", byteData);
		
		//Set another byte tuple with image data -- we set the mime type
		//appropriately to image/jpg so we get to see it in tupleview
		File jpeg = new File("penguin.jpg");
		byte[] bytes = new byte[(int)jpeg.length()];
		DataInputStream in = null;
		try { in = new DataInputStream(new FileInputStream(jpeg)); }
		catch (FileNotFoundException e) { e.printStackTrace(); }
		if (in != null) {
			try {
				in.readFully(bytes);
				in.close();
			} catch (IOException e) { e.printStackTrace(); }
		}
		PeisJavaMT.peisjava_setByteArrayTuple("image/jpg", "anImageTuple", bytes);
		
		//Get a tuple from local tuple space and show mime type
		PeisTuple tuple = PeisJavaMT.peisjava_getTuple("anImageTuple");
		System.out.println("Mime type of anImageTuple: " + tuple.getMimeType());
		
		PeisTuple tuple1 = PeisJavaMT.peisjava_getTuple("anotherTuple");
		System.out.println("Mime type of anotherTuple: " + tuple1.getMimeType());
		
		PeisTuple tuple2 = PeisJavaMT.peisjava_getTuple("aTuple");
		System.out.println("Mime type of aTuple: " + tuple2.getMimeType());


	}

}
