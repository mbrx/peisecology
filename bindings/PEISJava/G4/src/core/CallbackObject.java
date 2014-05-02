package core;

import com.sun.jna.Pointer;

public abstract class CallbackObject implements PeisJavaInterface.PeisTupleCallback {

	public abstract void callback(PeisTuple tuple);
	
	@Override
	public void callback(PeisTuple tuple, Pointer userdata) {
		this.callback(tuple);
	}
	
}