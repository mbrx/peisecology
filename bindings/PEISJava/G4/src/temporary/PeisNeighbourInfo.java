package temporary;
import com.sun.jna.Structure;

/**
 * Host information for neighbours we can connect to in future.
 * @author fpa
 *
 */
public class PeisNeighbourInfo extends Structure {
	/**
	 * Total number of tries since last successfull connection.
	 */
	public int nTries;
	
	public byte[] padding = new byte[4];
	
	/**
	 * Timepoint when we can next try to connect to this host.
	 */
	public double nextRetry;


}
