package core;

public class WrongPeisKernelVersionException extends Exception {

	/**
	 * 
	 */
	private static final long serialVersionUID = 1L;
	private String peisjavaVersion = null;
	private String peiskernelVersion = null;
	
	public WrongPeisKernelVersionException(String pjV, String pkV) {
		peisjavaVersion = pjV;
		peiskernelVersion = pkV;
	}
	
	public String getMessage() {
		return "PeisJava version " + peisjavaVersion  + " is not compatible with installed PeisKernel version " + peiskernelVersion;
	}
	
	

}
