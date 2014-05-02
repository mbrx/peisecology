package temporary;
import com.sun.jna.Pointer;
import com.sun.jna.Structure;

/**
 * Representation of different lowlevel addresses. This structure is
 * always stored in HOST byte order.
 * @author fpa
 *
 */
public class PeisLowlevelAddress extends Structure {
	/*
	class AuxStructure extends Structure {
		public byte[] ip = new byte[4];
		NativeLong port;
	}
	*/
	class AddressUnion extends Structure {
		public Pointer tcpIPv4;
		public Pointer udpIPv4;
	}
	public byte[] deviceName = new byte[8];
	public byte isLoopback;
	public byte[] padding = new byte[3];
	public int type;
	public AddressUnion addr;
}
