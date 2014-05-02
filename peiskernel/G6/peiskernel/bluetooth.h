/** \file bluetooth.h
   All functions for managing bluetooth connections
*/
/*
    Copyright (C) 2005 - 2012  Mathias Broxvall

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA
*/

#ifndef BLUETOOTH_H
#define BLUETOOTH_H

/** \ingroup LinkLayer */
/** \defgroup BluetoothLayer Bluetooth layer
    Bluetooth is an alternative link layer that can be used standalone
    or in conjunction with the TCP link layer. The bluetooth link
    layer module is compile time optional, and requires that
    libbluetooth-dev is installed to compile. 

    The bluetooth layer can use one or more bluetooth adapters, these
    are given as commandline options using --peis-bluetooth hciX
    where hciX is a specific bluetooth adaptor. Use "hcitool dev" to
    see which bluetooth adaptors are available on your system.     
    It can use multiple adaptors by beeing given multiple instances of
    that option. It does not rqeuire to use all adaptors available on
    the local machine. 

    Currently, peis components need to be run with root priviliges (!)
    in order to be able to use the bluetooth adaptors, this is
    neccessary to enable RAW interface mode to overcome some of the
    limitations in the current linux bluetooth interface drivers and
    libraries. 

    Multiple peiscomponents can be run using the same bluetooth
    adaptors. However, only one component can use the adaptor for
    "broadcasting" that this bluetooth adaptor is connected to a
    PEIS. Currently, the first PEIS that opens the device gets this
    privilege, and hence, when that PEIS is closed no other PEIS will
    make incoming connections to any PEIS on this computer. This can
    be fixed in the future. 

    In order to find other PEIS that have bluetooth adaptors, each
    PEIS with an adaptor performs SCAN operations using the bluetooth
    adaptor in RAW mode. This gives a list of all other bluetooth
    devices (not just PEIS) in the vicinity, as well as their signal
    strengths. The signal strenghts are reported as tuples, to be used
    by higher level applications to compute locations of devices. 

    By regurarly attempting to connect using L2CAP port 7315 (See \ref
    Hello "HELLO connections) to detected bluetooth devices, it can be
    determined if they are PEIS and basic host information data can be
    transmitted between the two. This also transmits the port numbers
    of any ports for incomming connections. 

    By using the signal strengths, and the results of the periodic
    HELLO connections, it is possible to determine with a bluetooth
    connection between two PEIS might be possible. This is reflected
    by the peisk_bluetoothIsConnectable function, and is used by the
    connectionMangager to determine which devices should connect to
    which. 

    See also \ref LinkLayer "the link layer" and 
    \ref Hello "HELLO connections"
*/
/** @{ */

/** True if we have the capabilities of connecting to bluetooth
    devices. This is true iff peiskernel is compiled with bluetooth
    support and we have atleast one (outgoing) bluetooth device
    registered. 
*/
int peisk_haveBluetoothCapabilities();

/** Initializes all bluetooth internal datastructures */
void peisk_initBluetooth();

/** Deallocates all bluetooth structures and closes open SDP sockets */
void peisk_closeBluetooth();

/** Commandline option for using a specific bluetooth device */
void peisk_clUseBluetooth(char *device);

/** Appends all found lowlevel bluetooth addresses that we can
    _listen_ to, to the list of kernel lowlevel addresses  */
void peisk_addBluetoothLowlevelAddresses();

/**  True if it is beliveable that we can connect to this bluetooth device */
int peisk_bluetoothIsConnectable(unsigned char btAddr[6],int port);

/**  Attempts to connect to given bluetooth address.
    Uses the adaptor with highest signal strength to this destination */
PeisConnection *peisk_bluetoothConnect(unsigned char btAddr[6],int port,int flags);

/** Performs the sending of data on a bluetooth L2CAP connection. Package
    points to a PeisPackage structure including data for a total 
    length of len bytes. Returns zero on success. */
int peisk_bluetoothSendAtomic(PeisConnection *connection,PeisPackage *package,int len);

/** Attempts to read a package from a bluetooth L2CAP connection. 
    Returns non-zero if there was a package.  */
int peisk_bluetoothReceiveIncomming(struct PeisConnection *connection,struct PeisPackage *package);

void peisk_bluetoothCloseConnection(struct PeisConnection *connection);

/** @} */

/********************* Internal functions ******************/
/********************* Internal functions ******************/
/********************* Internal functions ******************/
#ifdef BLUETOOTH_PRIVATE

/** \ingroup BluetoothLayer */
/** \defgroup BluetoothPrivate Private bluetooth definitions
    This group contains all definitions and functions used for the
    implementation of the bluetooth layer.  These are not intended to
    be used outside the file \ref bluetooth.c, and are protected by
    the BLUETOOTH_PRIVATE ifdef.     
*/
/** @{ */

/**  Maximum number of usable BT adaptors */
#define PEISK_MAX_BLUETOOTH_ADAPTORS 8
/**  How often scans will be made on BT adaptors */
#define PEISK_BT_SCAN_PERIOD 8
/**  How long each scan on a BT adaptor will be. 
    Measured in 1.24 (?) second intervals */
#define PEISK_BT_SCAN_LENGTH 4
/** Maximum number of visible bluetooth devices seen on any interface, any further devices
    are ignored. */ 
#define PEISK_MAX_DEVICES 128

/** Maximum sending speed (in bytes per second) allowed on any interface */
#define PEISK_BT_MAX_BPS 2000
/** When the RED (Random Early Detection) throttling cuts in on a bluetooth interface */
#define PEISK_BT_RED_BPS (PEISK_BT_MAX_BPS/2)

/**  How long we still consider bluetooth devices that have not
    been seen in a scan recently. */
#define PEISK_BLUETOOTH_KEEPALIVE 60.0

/**  Minimum report bluetooth RSSI value to consider target
    connectable */
#define PEISK_BLUETOOTH_MIN_CONNECTABLE_STRENGTH 150

/** Represents a pending outgoing connection for a given bluetooth adaptor */
typedef struct {
  /** Which state the pending connection is in. 0 nothing, 1 awaiting reply */
  int mode;
  /** Which address to connect to */
  unsigned char btAddr[6];
  /** Which port to connect to */
  int port;
  /** Unusued */
  int flags;
  /** Connection socket */
  int socket;
  /** When to give up on this connection attempt */
  double timeout;
  /** The pending PeisConnection structure which is an interface to the P2P layer */
  struct PeisConnection *connection;
} PeisBluetoothPendingOutgoingConnection;

/** Represents a pending incomming connection on given adaptor */
typedef struct {
  int mode;
  int socket;
  struct sockaddr_l2 remoteAddr;
  /** When to give up on incomming connection */
  double timeout;
} PeisBluetoothPendingIncommingConnection;

/** Information and sockets for all available bluetooth adaptors */
typedef struct PeisBluetoothAdaptor {
  int id;
  struct hci_dev_info info;
  char *name;
  bdaddr_t addr;

  /** Stores the state of any current pending outgoing connections */
  PeisBluetoothPendingOutgoingConnection outgoing;

  /** Stores the state of any current pending incomming connections */
  PeisBluetoothPendingIncommingConnection incomming;

  /** Main L2CAP port for receiving connections */
  int listenSocket;
  int port;

  /** Socket to control raw interface */
  int interfaceSocket;
  int isScanning;
  double nextScan;

  /** This is an L2CAP socket answering HELLO requests identifying us as a PEIS node */
  int helloSocket;
  /** File descriptor of any pending hello coming in on this interface. -1 if no pending */
  int pendingHelloFd;
  /** Count down when we will give up on this pending hello. After this time we will give up on the hello message */
  int pendingHelloCnt;
  /** Address of any remote hello connection */
  struct sockaddr_l2 pendingHelloRemoteAddr;

  /** Count how many bytes have been sent on this interface during this period */
  int bsCount;
  /** Sliding average of how many bytes we are sending each second */
  float bsAvgBps;

} PeisBluetoothAdaptor;

/**  Represents any bluetooth device found in the vicinity. Note,
    not only PEIS bluetooth components.  */
typedef struct PeisBtDevice {
  unsigned char btAddr[6];
  char name[256];
  unsigned char rawStrength[PEISK_MAX_BLUETOOTH_ADAPTORS];
  double lastSeen[PEISK_MAX_BLUETOOTH_ADAPTORS];
  /** When to next attempt to say hi to this device */
  double nextHelloAttempt;
  /** True if device is a connectable PEIS */
  int isConnectable;
} PeisBtDevice;

/** Raw hci header */
struct hcidump_hdr {
	uint16_t	len;
	uint8_t		in;
	uint8_t		pad; 
	uint32_t	ts_sec;
	uint32_t	ts_usec; 
} __attribute__ ((packed));
#define HCIDUMP_HDR_SIZE (sizeof(struct hcidump_hdr))

/** Raw bluetooth device frame */
struct frame {
	void		*data;
	uint32_t	data_len;
	void		*ptr;
	uint32_t	len;
	uint16_t	dev_id;
	uint8_t		in;
	uint8_t		master;
	uint16_t	handle;
	uint16_t	cid;
	uint16_t	num;
	uint8_t		dlci;
	uint8_t		channel;
	unsigned long	flags;
	struct timeval	ts;
	int		pppdump_fd;
	int		audio_fd;
};



/** List of all opened bluetooth devices */
PeisBluetoothAdaptor peisk_bluetoothAdaptors[PEISK_MAX_BLUETOOTH_ADAPTORS];
/** Number of opened bluetooth devices */
int peisk_nBluetoothAdaptors;

/** Number of seen bluetoot devices in neighbourhood */
int peisk_nBluetoothDevices;
/** List of all seen bluetooth devices in neighbourhood */
PeisBtDevice peisk_bluetoothDevices[PEISK_MAX_DEVICES];

/**  Periodically sends service discovery requests to seen devices. 
*/
void peisk_periodic_bluetoothServiceDiscovery(void *data);
/**  Generates a HELLO message into the given buffer. Returns size of message */
int peisk_bluetoothMakeHELLO(char *buf);
/**  Parses a HELLO message and adds to known hosts etc. */
void peisk_bluetoothParseHELLO(char *buf,int len);

/** @} Bluetooth private */

/** \ingroup BluetoothPrivate */
/** \defgroup Hello Hello Connections
    This is bluetooth connections that are established on L2CAP port
    0x7315 in order to test if a given bluetooth device is in fact a
    PEIS. A connection message consisting of connection string,
    peisid, and hostinfo should be send and received on it to
    establish that we are talking to another PEIS. 
*/
/** @{ */
/** Main periodic function for functions to run on every period. */
void peisk_periodic_bluetooth(void *user);
/**  Attempts to send HELLO connections to detected bluetooth devices */
void peisk_periodic_bluetoothSayHello(void *data);
/**  Updated counters of how many bytes each interface is transmitting per second. */
void peisk_periodic_bps(void *data);


/**  Handles I/O for periodic scanning of nearby bluetooth
     devices and signal strengths */
void peisk_bluetoothScan();
/**  Accepts HELLO connections and responds with local peis information */
void peisk_bluetoothAnswerHello();
/** Accepts incomming connections and establishes a (pending) socket */
void peisk_bluetoothAcceptConnections();

/**  How often we attempt to say hello to any other bluetooth devices */
#define PEISK_PERIOD_SAYHELLO 1.0
/**  How often we should counters for tracking outgoing tranmissions */
#define PEISK_PERIOD_BT_BPS 1.0
/** Time between each attempt to say hello to a specific device */
#define PEISK_HELLO_TRYAGAIN 30.0
/** Maximum time to try to establish a HELLO connection to another
    device */
#define PEISK_HELLO_TIMEOUT 20.0
/** Maximum time to try to establish an outgoing connection to another bluetooth device */
#define PEISK_CONNECT_TIMEOUT 20.0


/* Prints the given bluetooth mac address */
void peisk_printBluetoothAddress(unsigned char btAddr[6]);

/** Periodically goes through all adaptors and see if we currenlty have any pending
    outgoing connection attempts. If so, process them accordingly or
    cancel them if the timeout have been reached.  */
void peisk_bluetoothConnectOutgoing();


/** Initializes the MTU of a given bluetooth socket to reasonable values */
void peisk_bluetooth_setMTU(int sk);

#endif
/** @} HelloConnections */

#endif
