package examples;

import com.sun.jna.Pointer;
import core.PeisTuple;
import core.PeisJavaInterface.PeisTupleCallback;

public class MyCallbackObject implements PeisTupleCallback {

	@Override
	public void callback(PeisTuple tuple, Pointer userdata) {
		System.out.println("# callback ############");
		System.out.println("#######################");
		System.out.println("#######################");
		System.out.println("########## invocation #");		
	}

}
