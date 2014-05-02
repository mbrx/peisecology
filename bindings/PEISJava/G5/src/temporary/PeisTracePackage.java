package temporary;
import com.sun.jna.Structure;
import com.sun.jna.ptr.IntByReference;

/**
 * Package returned when requesting a traceroute to a given node.
 * @author fpa
 *
 */
public class PeisTracePackage extends Structure {
	public int hops;
	public IntByReference hosts;
}
