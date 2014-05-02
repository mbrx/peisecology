package temporary;
import com.sun.jna.Pointer;
import com.sun.jna.Structure;

/**
 * Basic host information providing fullname, PEIS address, reachable
 * lowlevel addresses etc. This structure is always stored in HOST
 * byte order.
 * @author fpa
 *
 */
public class PeisHostInfo extends Structure {

	
	public PeisHostInfo() {
		super(-1, ALIGN_NONE);
	}
	
	public void useMemory(Pointer p) {
		super.useMemory(p);
	} 
	
	
	/**
	 *  Address of this kernel on the network. -1 if not a valid/unused
	 *  hostinfo.
	 */
	public int id;
	
	/**
	 * Name of host computer.
	 */
	public byte[] hostname = new byte[64];
	
	/**
	 * A human readable fully qualified name describing this PEIS. Must
	 * be a null terminated string.
	 */
	public byte[] fullname = new byte[64];
	
	/**
	 * Number of lowlevel addresses of all network interfaces.
	 */
	public byte nLowlevelAddresses;                     
	
	/**
	 * Number of lowlevel addresses of all network interfaces.
	 */
	public byte[] padding = new byte[3];
	
	/**
	 * The lowlevel addresses of all network interfaces.
	 */
	public PeisLowlevelAddress lowAddr;
	
	/**
	 * Timepoint host was last seen on local network or on P2P network.
	 */
	public double lastSeen;         

	/**
	 * Id number of network cluster this host belong to.
	 * This is the lowest peis-id which can be routed to.
	 */
	public int networkCluster;
	
	public byte[] padding2 = new byte[4];
}
