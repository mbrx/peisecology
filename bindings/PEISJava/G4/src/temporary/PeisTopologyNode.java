package temporary;
import com.sun.jna.Structure;

/**
 * Contains all topological information about the current network.
 * @author fpa
 *
 */
public class PeisTopologyNode extends Structure {
	  public int requestId;
	  public int id;
	  public int nNeighbours;
	  public byte[] padding = new byte[4];
	  public int[] neighbours = new int[128];
	  public byte[] flags = new byte[128];
	  public double timestamp;
}
