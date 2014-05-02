/*
    Copyright (C) 2008 - 2011 Federico Pecora
    
    Based on libpeiskernel (Copyright (C) 2005 - 2011  Mathias Broxvall).
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301 USA.
*/


package examples;

import java.io.DataInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;

import core.PeisJavaMT;
import core.WrongPeisKernelVersionException;
import core.PeisJavaMT.ENCODING;

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
		//PeisJavaMT.peisjava_setByteArrayTuple("image/jpg", "anImageTuple", bytes);
		PeisJavaMT.peisjava_setTuple("anImageTuple", bytes, "image/jpg", ENCODING.BINARY);
		
	}

}
