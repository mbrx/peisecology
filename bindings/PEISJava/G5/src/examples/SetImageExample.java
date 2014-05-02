package examples;

import java.io.DataInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;

import core.PeisJavaMT;
import core.PeisTuple;
import core.WrongPeisKernelVersionException;

public class SetImageExample {
	
	public static void main(String[] args) {
		try {
			PeisJavaMT.peisjava_initialize(args);
		} catch (WrongPeisKernelVersionException e1) {
			// TODO Auto-generated catch block
			e1.printStackTrace();
			System.exit(1);
		}
		System.out.println("Hello there, I am: " + PeisJavaMT.peisjava_peisid());

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
		
	}

}
