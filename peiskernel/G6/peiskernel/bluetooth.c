/** \file bluetooth.c
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

#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <poll.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include <arpa/inet.h>

#ifndef WITH_BLUETOOTH
/* This is a list of stubs for all bluetooth layer public functions */

void peisk_initBluetooth() {}
void peisk_clUseBluetooth(char *device) { 
  fprintf(stderr,"Warning, kernel compiled without bluetooth support\n"); 
}
void peisk_addBluetoothLowlevelAddresses() {}
void peisk_closeBluetooth() {}

int peisk_haveBluetoothCapabilities() { return 0; }
int peisk_bluetoothIsConnectable(unsigned char btAddr[6],int port) { return 0; }
int peisk_bluetoothConnect(unsigned char btAddr[6],int port,int flags) { return 0; }
int peisk_bluetoothSendAtomic(struct PeisConnection *connection,struct PeisPackage *package,int len) { return -1; }
int peisk_bluetoothReceiveIncomming(struct PeisConnection *connection,struct PeisPackage *package) { return 0; }
void peisk_bluetoothCloseConnection(struct PeisConnection *connection) {}

#else

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/l2cap.h>

#ifdef WITH_BLUETOOTH
#define BLUETOOTH_PRIVATE
#endif

//#define PEISK_PRIVATE
#include "peiskernel.h"
#include "peiskernel_private.h"
#include "bluetooth.h"

/* This is the real code for all bluetooth related things */
/* This is the real code for all bluetooth related things */



void peisk_initBluetooth() {
  peisk_nBluetoothDevices=0;
  peisk_nBluetoothAdaptors=0;

  peisk_registerPeriodic(0.0, NULL, peisk_periodic_bluetooth);
  peisk_registerPeriodic(PEISK_PERIOD_BT_BPS, NULL, peisk_periodic_bps);
  peisk_registerPeriodic(PEISK_PERIOD_SAYHELLO, NULL, peisk_periodic_bluetoothSayHello);
}

void peisk_periodic_bluetooth(void *user) {
  peisk_bluetoothScan();
  peisk_bluetoothAnswerHello();
  peisk_bluetoothAcceptConnections();
  peisk_bluetoothConnectOutgoing();
}

void peisk_periodic_bps(void *user) {
  int i;  
  char name[256], val[256];

  for(i=0;i<peisk_nBluetoothAdaptors;i++) {
    PeisBluetoothAdaptor *adaptor = &peisk_bluetoothAdaptors[i];
    adaptor->bsAvgBps = adaptor->bsAvgBps * 0.8 + 0.2 * adaptor->bsCount;
    adaptor->bsCount = 0;
    sprintf(name,"kernel.%s.bps",adaptor->name);
    sprintf(val,"%d",(int)adaptor->bsAvgBps);
    peisk_setStringTuple(name,val);
    printf("%s=%s\n",name,val);
  }
}

void peisk_closeBluetooth() {
}

void peisk_clUseBluetooth(char *device) { 
  int opt,dd;
  char key[256], val[256];

  PeisBluetoothAdaptor *adaptor = &peisk_bluetoothAdaptors[peisk_nBluetoothAdaptors];
  adaptor->id = hci_devid(device);
  adaptor->outgoing.mode = 0;
  if(adaptor->id < 0) {
    perror("");
    fprintf(stderr,"Warning, failed to open adaptor %s\n",device);
    return;
  }
  if(hci_devinfo(adaptor->id,&adaptor->info)) {
    perror("");
    fprintf(stderr,"Failed to query adaptor %s\n",device);
    return;
  }
  printf("Current MTU ACL %d SCO %d\n",adaptor->info.acl_mtu,adaptor->info.sco_mtu);
  if(hci_devba(adaptor->id, &adaptor->addr) < 0) {
    perror("");
    fprintf(stderr,"Failed to query address of %s\n",device);
    return;
  }
  dd = hci_open_dev(adaptor->id);
  opt = hci_test_bit(HCI_RAW,&adaptor->info.flags);
  /* Setting adaptor to RAW mode, needed for scanning (?) */
  errno=0;
  if(ioctl(dd,HCISETRAW,opt) < 0) {
    if(errno == EACCES) {
      fprintf(stderr,"Warning, cannot gain RAW access to bluetooth device %s\n",device);
      /*return;*/
    }
  }
  hci_close_dev(dd);

  adaptor->interfaceSocket = socket(AF_BLUETOOTH, SOCK_RAW,BTPROTO_HCI);
  if(adaptor->interfaceSocket < 0) {
    fprintf(stderr,"Error, cannot create RAW socket to bluetooth device %s, dropping it\n",device);
    return;
  }
  opt=1;
  if (setsockopt(adaptor->interfaceSocket, SOL_HCI, HCI_DATA_DIR, &opt, sizeof(opt)) < 0) {
    perror("Can't enable data direction info");
    return;
  }
  
  opt=1;
  if (setsockopt(adaptor->interfaceSocket, SOL_HCI, HCI_TIME_STAMP, &opt, sizeof(opt)) < 0) {
    perror("Can't enable time stamp");
    return;
  }

  /* Setup filter */
  struct hci_filter flt;
  hci_filter_clear(&flt);
  hci_filter_all_ptypes(&flt);
  hci_filter_all_events(&flt);
  if (setsockopt(adaptor->interfaceSocket, SOL_HCI, HCI_FILTER, &flt, sizeof(flt)) < 0) {
    perror("Can't set filter");
    return;
  }
  
  /* Bind socket to the HCI device */
  struct sockaddr_hci isock_addr;
  isock_addr.hci_family = AF_BLUETOOTH;
  isock_addr.hci_dev = adaptor->id;
  if (bind(adaptor->interfaceSocket, (struct sockaddr *) &isock_addr, sizeof(isock_addr)) < 0) {
    printf("Can't attach to device %s. %s(%d)\n", device, strerror(errno), errno);
    return;
  }
  printf("Successfully bound a RAW socket to %s\n",device);

  adaptor->name = strdup(device);  
  adaptor->isScanning = 0;
  adaptor->nextScan = 0;
  peisk_nBluetoothAdaptors++;

  /* Give mac address of adaptor as value for the device tuple kernel.hciX */
  sprintf(key,"kernel.%s",device);
  ba2str(&adaptor->addr, val);
  peisk_setStringTuple(key,val);


  /* Setup any L2CAP port for connections */
  struct sockaddr_l2 listen_addr={0};

  adaptor->listenSocket = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
  if(adaptor->listenSocket < 0) {
    fprintf(stderr,"peisk::warning - cannot create listening socket for %s\n",device);
  } else {
    adaptor->port = 1000;
    listen_addr.l2_family = AF_BLUETOOTH;
    listen_addr.l2_bdaddr = adaptor->addr;
    /* Brute force search until we find a free port we can use */
    while(1) {
      listen_addr.l2_psm = adaptor->port;
      if(bind(adaptor->listenSocket, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) == 0) break;
      if(++adaptor->port > 32765) {
	adaptor->port=-1;
	close(adaptor->listenSocket);
	adaptor->listenSocket=-1;
	fprintf(stderr,"peisk::warning - cannot bind any port for %s\n",device);
	break;
      }
    }    
    printf("Using bluetooth port %d\n",adaptor->port);

    if(adaptor->listenSocket != -1 && 
       listen(adaptor->listenSocket, 1)) {
      adaptor->port=-1;
      close(adaptor->listenSocket);
      adaptor->listenSocket=-1;
      fprintf(stderr,"peisk::warning - cannot listen on %s\n",device);
    }
    if(adaptor->listenSocket != -1 &&
       fcntl(adaptor->listenSocket,F_SETFL,O_NONBLOCK) == -1){
      adaptor->port=-1;
      close(adaptor->listenSocket);
      adaptor->listenSocket=-1;
      fprintf(stderr,"peisk::warning - failed to set listening socket of %s non blocking\n",device);
    }
    /*    if(adaptor->listenSocket != -1) {
      peisk_bluetooth_setMTU(adaptor->listenSocket);
      }*/
  }

  /* Setup L2CAP port for hello messages */
  struct sockaddr_l2 hello_addr={0};

  adaptor->helloSocket = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
  hello_addr.l2_family = AF_BLUETOOTH;
  hello_addr.l2_bdaddr = adaptor->addr;
  hello_addr.l2_psm = htobs(0x1001); 
  if(adaptor->helloSocket != -1 && 
     bind(adaptor->helloSocket, (struct sockaddr *)&hello_addr, sizeof(hello_addr))) {
    close(adaptor->helloSocket);
    adaptor->helloSocket = -1;
    printf("peisk::warning, could not bind L2CAP socket 0x7E15 on %s\nWe will not be bluetooth connectable\n",device);    
  }
  if(adaptor->helloSocket != -1 && 
     listen(adaptor->helloSocket, 1)) {
    printf("peisk::warning, failed to listen to L2CAP on %s\nWe will not be bluetooth connectable",device);
    close(adaptor->helloSocket);
    adaptor->helloSocket = -1;
  } 
  if(adaptor->helloSocket != -1 && 
     fcntl(adaptor->helloSocket,F_SETFL,O_NONBLOCK) == -1) {
    printf("peisk::warning, failed to set L2CAP socket on %s non blocking\nWe will not be bluetooth connectable",device);
    close(adaptor->helloSocket);
    adaptor->helloSocket = -1;
  }
  /* Mark adaptor as not having any pending hello's right now */
  adaptor->pendingHelloFd = -1;

  printf("Listesting on %s: %02X:%02X:%02X:%02X:%02X:%02X;%d\n",device,
	 adaptor->addr.b[5],adaptor->addr.b[4],adaptor->addr.b[3],adaptor->addr.b[2],
	 adaptor->addr.b[1],adaptor->addr.b[0],adaptor->port);

}
void peisk_addBluetoothLowlevelAddresses() {
  int i,j;

  for(i=0;i<peisk_nBluetoothAdaptors&&
	peiskernel.hostInfo.nLowlevelAddresses<PEISK_MAX_LOWLEVEL_ADDRESSES;
      i++) {

    if(peisk_bluetoothAdaptors[i].helloSocket == -1) 
      /* This socket cannot listen, hence do not register as an incomming lowlevel address */
      continue;

    PeisLowlevelAddress *addr = &peiskernel.hostInfo.lowAddr[peiskernel.hostInfo.nLowlevelAddresses];
    strncpy(addr->deviceName, peisk_bluetoothAdaptors[i].name, sizeof(addr->deviceName)-1);
    addr->deviceName[sizeof(addr->deviceName)]=0;
    addr->isLoopback=0;
    addr->type = ePeisBluetooth;
    addr->addr.bluetooth.port = peisk_bluetoothAdaptors[i].port;
    for(j=0;j<6;j++)
      addr->addr.bluetooth.baddr[j] = peisk_bluetoothAdaptors[i].addr.b[5-j];    
    char str[256];
    ba2str(&peisk_bluetoothAdaptors[i].addr,str);
    printf("Added a lowlevel address: %s %d\n",str,addr->addr.bluetooth.port);

    peiskernel.hostInfo.nLowlevelAddresses++;
  }
}

void peisk_bluetoothScan() {  
  int i,j;
  unsigned char scanRequest[5] = {0x33,0x8b,0x9e,PEISK_BT_SCAN_LENGTH,0x00};
  static unsigned char *buf = NULL , *ctrl = NULL;
  int len, hdr_size = HCIDUMP_HDR_SIZE; 
  struct cmsghdr *cmsg;
  struct msghdr msg;
  struct iovec  iv;
  struct hcidump_hdr *dh;
  struct btsnoop_pkt *dp;
  struct frame frm;
  char str[256], val[256];
  int n;

  /* Go through all known bluetooth interfaces and 
     initiate scans on those that are not currenly scanning and have
     passed their time.
  */
 

  for(n=0;n<peisk_nBluetoothAdaptors;n++) {
    PeisBluetoothAdaptor *adaptor = &peisk_bluetoothAdaptors[n];
    if(adaptor->interfaceSocket < 0) continue;

    //printf("Initiating scan on device %d\n",n);

    /* Handle special case if time goes backwards. */
    if(adaptor->nextScan > peisk_timeNow+PEISK_BT_SCAN_PERIOD)
      adaptor->nextScan = peisk_timeNow+PEISK_BT_SCAN_PERIOD;
    /* Initiate new scans */

    if(adaptor->nextScan < peisk_timeNow) {
      adaptor->nextScan = peisk_timeNow + PEISK_BT_SCAN_PERIOD;
      adaptor->isScanning=1;
      /*printf("Initiating scan on %s\n",adaptor->name);*/
      fflush(stdout);
      hci_send_cmd(adaptor->interfaceSocket,0x01,0x01,sizeof(scanRequest),scanRequest);
    }

    /* Process any frames on the raw sockets */
    int sock = adaptor->interfaceSocket;
    int snap_len = HCI_MAX_FRAME_SIZE;
    if(!buf)
      buf = malloc(snap_len + hdr_size);
    if(!buf) {
      perror("Can't allocate data buffer for bluetooth devices");
      return;
    }
    dh = (void *) buf;
    dp = (void *) buf;
    frm.data = buf + hdr_size;
    
    if(!ctrl)
      ctrl = malloc(100);
    if (!ctrl) {
      free(buf);
      perror("Can't allocate control buffer");
      return;
    }
    
    memset(&msg, 0, sizeof(msg));
    
    iv.iov_base = frm.data;
    iv.iov_len  = snap_len;
    
    msg.msg_iov = &iv;
    msg.msg_iovlen = 1;
    msg.msg_control = ctrl;
    msg.msg_controllen = 100;
    
    len = recvmsg(sock, &msg, MSG_DONTWAIT);
    if (len < 0) {
      if (errno == EAGAIN || errno == EINTR)
	return;
      return;
    }
    printf("Read %d bytes from bluetooth socket %d\n",len,sock);

    /* Process control mes    printf("Read data is\n"sage */
    frm.data_len = len;
    frm.in = 0;
    
    cmsg = CMSG_FIRSTHDR(&msg);
    while (cmsg) {
      switch (cmsg->cmsg_type) {
      case HCI_CMSG_DIR:
	frm.in = *((int *) CMSG_DATA(cmsg));
	break;
      case HCI_CMSG_TSTAMP:
	frm.ts = *((struct timeval *) CMSG_DATA(cmsg));
	break;
      }
      cmsg = CMSG_NXTHDR(&msg, cmsg);
    }    
    frm.ptr = frm.data;
    frm.len = frm.data_len;
    
    unsigned char btAddr[6];
    unsigned char *data = frm.data;

    printf("frm.len = %d\n",frm.len);
    peisk_hexDump(data,len);

    if(frm.len == 4 &&
       data[0] == 0x04 &&
       data[1] == 0x01 &&
       data[2] == 0x01 &&
       data[3] == 0x00) {
      adaptor->isScanning = 0;
      /*printf("Interface %s is FINISHED\n",adaptor->name);*/
    }
    /* Short "normal" inquiry results */
    if(frm.len == 18 &&
       data[0] == 0x04 &&
       data[1] == 0x22 &&
       data[2] == 0x0f) {
      for(i=0;i<6;i++) btAddr[i] = data[9-i];
      for(i=0;i<peisk_nBluetoothDevices;i++) {
	for(j=0;j<6;j++) 
	  if(btAddr[j] != peisk_bluetoothDevices[i].btAddr[j]) break;
	if(j == 6) break;
	/*else printf("does not match %d (j=%d)\n",i,j);*/
      }
      if(i == peisk_nBluetoothDevices) {
	/* No previous device found */
	peisk_nBluetoothDevices++;      
	for(j=0;j<6;j++) peisk_bluetoothDevices[i].btAddr[j] = btAddr[j];
	for(j=0;j<PEISK_MAX_BLUETOOTH_ADAPTORS;j++) {
	  peisk_bluetoothDevices[i].rawStrength[j]=0;
	  peisk_bluetoothDevices[i].lastSeen[j]=0.0;
	}
	peisk_bluetoothDevices[i].name[0]=0;
	peisk_bluetoothDevices[i].nextHelloAttempt=0.0;
	peisk_bluetoothDevices[i].isConnectable=0;

	printf("Found a new device: "); 
	for(j=0;j<6;j++) printf("%02x:",peisk_bluetoothDevices[i].btAddr[j]);
	printf("\n");
      } 
      if(peisk_bluetoothDevices[i].name[0] == 0 && 0) {
	/** \todo Read names of bluetooth devices asynchronosly */
	/* Scan for name of this device */
	bdaddr_t baddr;
	for(j=0;j<6;j++) baddr.b[j] = btAddr[5-j];
	printf("Attempting to read remote name of: ");
	ba2str(&baddr,val);
	printf("%s\n",val);
	
	int s = hci_open_dev(adaptor->id);
	if(hci_read_remote_name(s,&baddr,255,val,500) == 0) {
	  printf("Read name: %s\n",val);
	  
	  strncpy(peisk_bluetoothDevices[i].name,val,255);	
	  sprintf(str,"kernel.%s.%02X%02X%02X%02X%02X%02X.name",adaptor->name,
		  btAddr[0],btAddr[1],btAddr[2],btAddr[3],btAddr[4],btAddr[5]);
	  peisk_setStringTuple(str,val);
	} else 
	  perror("Failed");
	close(s);
      }

      peisk_bluetoothDevices[i].lastSeen[n] = peisk_timeNow;
      peisk_bluetoothDevices[i].rawStrength[n] = data[17];
      /*printf("Device %d, iface %s <- %d\n",i,adaptor->name,data[17]);*/
      
      sprintf(str,"kernel.%s.%02X%02X%02X%02X%02X%02X.str",adaptor->name,
	      btAddr[0],btAddr[1],btAddr[2],btAddr[3],btAddr[4],btAddr[5]);
      sprintf(val,"%d",data[17]);
      peisk_setStringTuple(str,val);
    }
    else if(frm.len == 258 &&
	    data[0] == 0x04 &&
	    data[1] == 0x2f &&
	    data[2] == 0xff &&
	    data[3] == 0x01) {
      for(i=0;i<6;i++) btAddr[i] = data[9-i];
      for(i=0;i<peisk_nBluetoothDevices;i++) {
	for(j=0;j<6;j++) 
	  if(btAddr[j] != peisk_bluetoothDevices[i].btAddr[j]) break;
	if(j == 6) break;
	/*else printf("does not match %d (j=%d)\n",i,j);*/
      }
      if(i == peisk_nBluetoothDevices) {
	/* No previous device found */
	peisk_nBluetoothDevices++;      
	for(j=0;j<6;j++) peisk_bluetoothDevices[i].btAddr[j] = btAddr[j];
	for(j=0;j<PEISK_MAX_BLUETOOTH_ADAPTORS;j++) {
	  peisk_bluetoothDevices[i].rawStrength[j]=0;
	  peisk_bluetoothDevices[i].lastSeen[j]=0.0;
	}
	peisk_bluetoothDevices[i].name[0]=0;
	peisk_bluetoothDevices[i].nextHelloAttempt=0.0;
	peisk_bluetoothDevices[i].isConnectable=0;

	printf("Found a new device: "); 
	for(j=0;j<6;j++) printf("%02x:",peisk_bluetoothDevices[i].btAddr[j]);
	printf("\n");
      } 
      peisk_bluetoothDevices[i].lastSeen[n] = peisk_timeNow;
      peisk_bluetoothDevices[i].rawStrength[n] = data[17];
      sprintf(str,"kernel.%s.%02X%02X%02X%02X%02X%02X.str",adaptor->name,
	      btAddr[0],btAddr[1],btAddr[2],btAddr[3],btAddr[4],btAddr[5]);
      sprintf(val,"%d",data[17]);
      peisk_setStringTuple(str,val);
    }
  }
}
void peisk_bluetoothAnswerHello(void *data) {
  int n,len,i,j,len2;
  socklen_t opt = sizeof(struct sockaddr_l2);
  char buf[1024];
  char str[256];

  for(n=0;n<peisk_nBluetoothAdaptors;n++) {
    PeisBluetoothAdaptor *adaptor = &peisk_bluetoothAdaptors[n];
    if(adaptor->helloSocket == -1) continue;
    if(adaptor->pendingHelloFd == -1) {
      adaptor->pendingHelloFd = accept(adaptor->helloSocket,(struct sockaddr*) &adaptor->pendingHelloRemoteAddr, &opt);
      if(adaptor->pendingHelloFd == -1) continue;
      else {	
	adaptor->pendingHelloCnt=20;
	ba2str(&adaptor->pendingHelloRemoteAddr.l2_bdaddr, str);
	printf("Incoming HELLO connection from: %s\n",str);
	
	/* Send HELLO data to pager */
	len=peisk_bluetoothMakeHELLO(buf);
	/*printf("Responding with %d bytes\n",len);*/
	len2=write(adaptor->pendingHelloFd, buf, len);
	PEISK_ASSERT(len2 == len,("Wrote only %d of %d bytes in HELLO answer\n",len2,len));
      } 
    }
    len = read(adaptor->pendingHelloFd,buf,sizeof(buf));
    if(len > 0) {
      peisk_bluetoothParseHELLO(buf,len);
      /*ba2str(&adaptor->pendingHelloRemoteAddr.l2_bdaddr, str);*/
      /*printf("Got HELLO data from: %s\n",str);*/
      
      /* Find which BT device this was */
      for(i=0;i<peisk_nBluetoothDevices;i++) {
	for(j=0;j<6;j++) if(peisk_bluetoothDevices[i].btAddr[5-j] != adaptor->pendingHelloRemoteAddr.l2_bdaddr.b[j]) break;
	if(j == 6) {
	  if(peisk_bluetoothDevices[i].isConnectable == 0) {
	    peisk_bluetoothDevices[i].isConnectable=1;
	    /*printf("Incoming ");
	    for(j=0;j<6;j++) printf("%02X:",peisk_bluetoothDevices[i].btAddr[j]);
	    printf(" is a PEIS\n");*/
	  }
	}
      }
	

      close(adaptor->pendingHelloFd);
      adaptor->pendingHelloFd=-1;
    }
  }
}
int peisk_bluetoothMakeHELLO(char *buf) {
  char *orig = buf;
  int version = htonl(peisk_protocollVersion);
  char str[64]={0};
  PeisHostInfoPackage package;
  memcpy((void*) buf,&version,sizeof(int)); buf += sizeof(int);  
  strcpy(str,peisk_networkString);
  str[62]=0;
  str[63]=0; /* typically used for flags */
  memcpy(buf,str,64); buf += 64;
  package.isCached = 0;
  package.hostInfo = peiskernel.hostInfo;
  peisk_hton_hostInfo(&package.hostInfo);
  memcpy(buf,&package,sizeof(package)); buf += sizeof(package);
  return buf - orig;
}
void peisk_bluetoothParseHELLO(char *buf,int len) {
  int version;
  char str[64];
  PeisHostInfoPackage package;

  if(len != sizeof(int) + 64 + sizeof(package)) {
    fprintf(stderr,"Warning, received bluetooth HELLO of wrong length (%d)\n",len);
    return;
  }
  memcpy((void*) &version, buf, sizeof(int)); buf += sizeof(int);
  memcpy(str,buf,64); buf += 64;
  memcpy(&package,buf,sizeof(package));
  if(version != htonl(peisk_protocollVersion)) {
    fprintf(stderr,"Warning, received bluetooth HELLO of wrong version\n");
    return;
  }
  str[62]=0;
  if(strcmp(str,peisk_networkString) != 0) {
    fprintf(stderr,"Warning, received bluetooth HELLO with wrong networkstring %s\n",str);
    return;
  }
  
  /* Convert package to be in HOST byte order */
  peisk_ntoh_hostInfo(&package.hostInfo);
  /* Send hostinfo to be processed by the P2P layer */
  peisk_handleBroadcastedHostInfo(&package.hostInfo);
}


void peisk_periodic_bluetoothSayHello(void *data) {
  int i,j,len,len2;
  static int status=0;
  static int helloDevice=-1;
  static int helloFd;
  struct sockaddr_l2 addr = { 0 };
  char buf[1024];
  static double helloTimeout=0;

  /** 0, nothinging happening. 1, started connecting, 2 waiting for
      reply */
  static int mode = 0;

  
  if(mode == 0) {

    /* Find the first bluetooth device to say hello to, if any */
    for(i=0;i<peisk_nBluetoothDevices;i++)
      if(peisk_timeNow > peisk_bluetoothDevices[i].nextHelloAttempt &&
	 peisk_bluetoothDevices[i].isConnectable == 0) {
	for(j=0;j<peisk_nBluetoothAdaptors;j++)
	  if(peisk_timeNow > peisk_bluetoothDevices[i].lastSeen[j] - PEISK_BLUETOOTH_KEEPALIVE) break;
	if(j != peisk_nBluetoothAdaptors) break;
      }
	 
    if(i == peisk_nBluetoothDevices) 
      /* No device found, nothing to do */
      return;
    /* Remember which device we attempting to say hi to */
    helloDevice=i;
    /* Update when he should next be greeted */
    peisk_bluetoothDevices[i].nextHelloAttempt = peisk_timeNow + PEISK_HELLO_TRYAGAIN;
    /*printf("Saying HELLO to %02X:%02X:%02X:%02X:%02X:%02X\n",
	   peisk_bluetoothDevices[i].btAddr[0],peisk_bluetoothDevices[i].btAddr[1],peisk_bluetoothDevices[i].btAddr[2],
	   peisk_bluetoothDevices[i].btAddr[3],peisk_bluetoothDevices[i].btAddr[4],peisk_bluetoothDevices[i].btAddr[5]);
    */

    helloFd = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
    /* Bind the bluetooth interface with highest signal strength to
       socket */
    addr.l2_family = AF_BLUETOOTH;
    addr.l2_psm = htobs(-1);
    int bestStrength=0, bestAdaptor=-1;
    for(j=0;j<peisk_nBluetoothAdaptors;j++) 
      if(peisk_timeNow > peisk_bluetoothDevices[i].lastSeen[j] - PEISK_BLUETOOTH_KEEPALIVE &&
	 peisk_bluetoothDevices[i].rawStrength[j] > bestStrength) { 
	bestStrength=peisk_bluetoothDevices[i].rawStrength[j];
	bestAdaptor=j; 
      }
    PEISK_ASSERT(bestAdaptor!=-1,("No adaptor found? should not be possible\n"));

    addr.l2_bdaddr = peisk_bluetoothAdaptors[bestAdaptor].addr;
    bind(helloFd, (struct sockaddr *)&addr, sizeof(addr));

    /*ba2str(&addr.l2_bdaddr,buf);
      printf("Bound %s (%s) for outgoing HELLO connection\n",peisk_bluetoothAdaptors[bestAdaptor].name,buf);*/

    /* Establish a connection */
    addr.l2_family = AF_BLUETOOTH;
    addr.l2_psm = htobs(0x1001);
    for(j=0;j<6;j++)
      addr.l2_bdaddr.b[j] = peisk_bluetoothDevices[i].btAddr[5-j];
    
    ba2str( &addr.l2_bdaddr, buf );
    printf("Starting HELLO connection to %s\n",buf);


    
    if(fcntl(helloFd,F_SETFL,O_NONBLOCK) != 0) {
      perror("Failed to set bluetooth socket nonblocking\n");
    }
    connect(helloFd, (struct sockaddr *)&addr, sizeof(addr));
    helloTimeout = peisk_timeNow + PEISK_HELLO_TIMEOUT;
    mode = 1;
  }
  if(mode == 1) {
    i = helloDevice;

    struct pollfd fds;
    fds.fd = helloFd;
    fds.events = -1;
    fds.revents = 0;
    if(poll(&fds,1,0)) {
      socklen_t slen;
      slen=sizeof(status);
      if(getsockopt(helloFd,SOL_SOCKET,SO_ERROR,(void*) &status,&slen)) {
	printf("Get sockopt failed...\n");
      }
      /*printf("Connection finished: status=%d (time elapsed: %.3fs)\n",status,peisk_gettimef()-helloTimeout+10.0);*/
      
      if(status) {
	/*printf("Failed to say HELLO\n");*/
	close(helloFd);
	mode = 0;
	helloFd = -1;
	helloDevice = -1;
      } else {
	len=peisk_bluetoothMakeHELLO(buf);	
	/*printf("Sending %d bytes HELLO\n",len);*/
	len2=write(helloFd,buf,len);
	PEISK_ASSERT(len2 == len,("Wrote only %d of %d bytes in HELLO message\n",len2,len));
	mode = 2;
      }      
    }
  }
  if(mode == 2) {
    i = helloDevice;
    
    len = read(helloFd,buf,sizeof(buf));
    if(len > 0) {
      /*printf("Got HELLO response\n",buf);*/
      peisk_bluetoothParseHELLO(buf,len);

      peisk_bluetoothDevices[i].isConnectable = 1;
      /*printf("Outgoing ");
      for(j=0;j<6;j++) printf("%02X:",peisk_bluetoothDevices[i].btAddr[j]);
      printf(" is a PEIS\n");*/
      close(helloFd);
      mode = 0;
      helloFd = -1;
      helloDevice = -1;      
    }
  }

  if(mode > 0 && helloTimeout < peisk_timeNow) {
    /*printf("Giving up... %.1fs passed\n",peisk_timeNow-(helloTimeout-PEISK_HELLO_TIMEOUT));*/
    close(helloFd);
    mode = 0;
    helloFd = -1;
    helloDevice = -1;
  }
}
 
int peisk_haveBluetoothCapabilities() { return peisk_nBluetoothAdaptors?1:0; }
int peisk_bluetoothIsConnectable(unsigned char btAddr[6],int port) {
  int i,j;
  PeisBtDevice *device;
    
  for(i=0;i<peisk_nBluetoothDevices;i++) {
    if(peisk_bluetoothAdaptors[i].outgoing.mode != 0) continue;
    device = &peisk_bluetoothDevices[i];
    for(j=0;j<6;j++) if(device->btAddr[j] != btAddr[j]) break;
    if(j != 6) continue;
    for(j=0;j<peisk_nBluetoothAdaptors;j++)
      if(device->rawStrength[j] > PEISK_BLUETOOTH_MIN_CONNECTABLE_STRENGTH &&
	 device->lastSeen[j] > peisk_timeNow - PEISK_BLUETOOTH_KEEPALIVE) {
	return 1;
      }
    return 0;
  }
  return 0;
}


PeisConnection *peisk_bluetoothConnect(unsigned char btAddr[6],int port,int flags) {
  int i,j,bestStrength;
  PeisBtDevice *device;
  PeisBluetoothAdaptor *adaptor;

  printf("Attempting to connect to "); peisk_printBluetoothAddress(btAddr); printf(";%d\n",port);

  /* Find the device corresponding to this address */
  for(i=0;i<peisk_nBluetoothDevices;i++) {
    device = &peisk_bluetoothDevices[i];
    for(j=0;j<6;j++) if(device->btAddr[j] != btAddr[j]) break;
    if(j == 6) break;
  }
  if(i == peisk_nBluetoothDevices) {
    printf("No such device found\n");
    return NULL;
  }

  /*printf("Adaptor modes: %d\n",peisk_bluetoothAdaptors[0].outgoing.mode);
  printf("Last seen: %f\n",device->lastSeen[0]-peisk_timeNow);
  printf("Strength: %d\n",device->rawStrength[0]);
  */

  /* Find the best interface for this address */
  for(i=0,bestStrength=0,adaptor=NULL;i<peisk_nBluetoothAdaptors;i++) {
    if(peisk_bluetoothAdaptors[i].outgoing.mode == 0 && 
       device->lastSeen[i] > peisk_timeNow - PEISK_BLUETOOTH_KEEPALIVE &&
       device->rawStrength[i] > PEISK_BLUETOOTH_MIN_CONNECTABLE_STRENGTH &&
       device->rawStrength[i] > bestStrength) {
      adaptor = &peisk_bluetoothAdaptors[i];
      bestStrength = device->rawStrength[i];
    }    
  }
  if(!adaptor) {
    printf("No suitable adaptor found\n");
    return NULL;
  }
  /*printf("Using adaptor: %s\n",adaptor->name);*/

  /* Allocate connection structure to use. This will allocate and
     place the connection in PENDING mode. */
  adaptor->outgoing.connection = peisk_newConnection();
  adaptor->outgoing.connection->type = eBluetoothConnection;
  adaptor->outgoing.connection->connection.bluetooth.adaptor = adaptor;
  printf("Preparing connection: %x id: %d\n",(unsigned int) adaptor->outgoing.connection,adaptor->outgoing.connection->id);

  /* Prepare connection request structure and connection */
  adaptor->outgoing.flags = flags;
  adaptor->outgoing.mode = 1;
  adaptor->outgoing.timeout = peisk_timeNow + PEISK_CONNECT_TIMEOUT;


  /* Finally, initiate connection attempt. Will be finalized by
     bluetoothConnectOutgoing periodic function */
  struct sockaddr_l2 addr = { 0 };
  adaptor->outgoing.socket = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
  addr.l2_family = AF_BLUETOOTH;
  addr.l2_psm = htobs(-1); /* should be -1 or 0 ?? */ 
  addr.l2_bdaddr = adaptor->addr;
  bind(adaptor->outgoing.socket, (struct sockaddr *)&addr, sizeof(addr));

  printf("Setting MTU of outgoing socket\n");
  peisk_bluetooth_setMTU(adaptor->outgoing.socket);

  addr.l2_family = AF_BLUETOOTH;
  addr.l2_psm = htobs(port);
  for(j=0;j<6;j++)
    addr.l2_bdaddr.b[j] = btAddr[5-j];
  if(fcntl(adaptor->outgoing.socket,F_SETFL,O_NONBLOCK) != 0) {
    perror("Failed to set bluetooth socket nonblocking\n");
  }
  connect(adaptor->outgoing.socket, (struct sockaddr *)&addr, sizeof(addr));

  return adaptor->outgoing.connection;
}
void peisk_printBluetoothAddress(unsigned char btAddr[6]) {
  int i;
  for(i=0;i<6;i++) { printf("%02X",btAddr[i]); if(i!=5) printf(":"); }  
}
void peisk_bluetoothAcceptConnections() {
  int i,len;
  PeisBluetoothAdaptor *adaptor;
  socklen_t opt = sizeof(struct sockaddr_l2);
  char str[256];
  PeisConnectMessage message;
  PeisConnection *connection;

  for(i=0;i<peisk_nBluetoothAdaptors;i++) {
    adaptor = &peisk_bluetoothAdaptors[i];
    if(adaptor->incomming.mode == 0) {      
      adaptor->incomming.socket = accept(adaptor->listenSocket,(struct sockaddr*) &adaptor->incomming.remoteAddr, &opt);
      if(adaptor->incomming.socket == -1) continue;
      printf("Setting MTU of incomming socket\n");
      peisk_bluetooth_setMTU(adaptor->incomming.socket);

#ifdef OLD
      struct l2cap_options opts;
      struct l2cap_conninfo conn;
      socklen_t optlen;

      printf("Setting MTU of incomming socket\n");
      memset(&opts, 0, sizeof(opts));
      optlen = sizeof(opts);
      if (getsockopt(adaptor->incomming.socket, SOL_L2CAP, L2CAP_OPTIONS, &opts, &optlen) < 0) {
	perror("Can't get default L2CAP options");
      }

      /* Set new options */
      printf("Old L2CAP MTU: %d %d\n",opts.omtu,opts.imtu);
      opts.omtu = PEISK_MAX_PACKAGE_SIZE;
      opts.imtu = PEISK_MAX_PACKAGE_SIZE;
      /*        if (rfcmode > 0)
		opts.mode = rfcmode;*/
#endif

      ba2str(&adaptor->incomming.remoteAddr.l2_bdaddr,str);
      printf("Incomming connection from: %s\n",str);

      if(fcntl(adaptor->incomming.socket,F_SETFL,O_NONBLOCK) != 0) {
	perror("Failed to set incomming socket nonblocking\n");
      }      

      adaptor->incomming.timeout = peisk_timeNow + 5.0;
      adaptor->incomming.mode = 1;
    }
    if(adaptor->incomming.mode == 1) {
      len = read(adaptor->incomming.socket,(void*)&message,sizeof(message));
      if(len == sizeof(message)) {
	/* Create connection structure to use */
	printf("Received connection message:\n"); peisk_hexDump(&message,len); printf("\n");
	connection = peisk_newConnection();
	if(!connection) {
	  fprintf(stderr,"peisk::error - failed to create connection structure\n");
	  return;
	}
	if(peisk_verifyConnectMessage(connection,&message) != 0) {
	  printf("do not accept him\n");
	  peisk_abortConnect(connection);
	  close(adaptor->incomming.socket);
	  adaptor->incomming.mode = 0; 
	  continue;
	}
	connection->type = eBluetoothConnection;
	connection->connection.bluetooth.socket = adaptor->incomming.socket;
	connection->connection.bluetooth.adaptor = adaptor; 

	printf("connection %x accepted\n",(int)connection);

	/* Let P2P layer handle this connection */    
	peisk_incommingConnectFinished(connection,&message);
	
	if(1 || peisk_printLevel & PEISK_PRINT_CONNECTIONS)
	  printf("peisk: accepted new incomming BLUETOOTH connection to %d with index: %d, flags=%d\n",
		 message.id,connection->id,message.flags);

	adaptor->incomming.mode = 0;
      } else if(len > 0) {
	fprintf(stderr,"Warning, received incorrect length on incomming bluetooth connection\n");
	close(adaptor->incomming.socket);
	adaptor->incomming.mode = 0; 
	continue;
      }
    }
    if(adaptor->incomming.mode == 1 && adaptor->incomming.timeout < peisk_timeNow) {
      /* peisk_abortConnect(adaptor->outgoing.connection); ?? */
      peisk_freeConnection(connection);
      close(adaptor->incomming.socket);
      adaptor->incomming.mode = 0;
    }
  }
}

void peisk_bluetoothConnectOutgoing() {
  int i;
  PeisBluetoothAdaptor *adaptor;
  int status=0;

  for(i=0;i<peisk_nBluetoothAdaptors;i++) {
    adaptor = &peisk_bluetoothAdaptors[i];
    if(adaptor->outgoing.mode == 0) continue;
    if(adaptor->outgoing.mode == 1) {
      /* See if connect() have finished */
      struct pollfd fds;
      fds.fd = adaptor->outgoing.socket;
      fds.events = -1;
      fds.revents = 0;
      if(poll(&fds,1,0)) {
	socklen_t slen;
	slen=sizeof(status);
	if(getsockopt(adaptor->outgoing.socket,SOL_SOCKET,SO_ERROR,(void*) &status,&slen)) {
	  printf("Get sockopt failed...\n");
	}
	printf("Connection finished: status=0x%x (time elapsed: %.3fs)\n",status,
	       peisk_gettimef()-adaptor->outgoing.timeout+PEISK_CONNECT_TIMEOUT);	
	if(status) {
	  printf("Outgoing connection failed: "); perror(""); printf("\n");
	  peisk_abortConnect(adaptor->outgoing.connection);
	  close(adaptor->outgoing.socket);
	  adaptor->outgoing.mode = 0;
	} else {
	  adaptor->outgoing.connection->type=eBluetoothConnection;
	  adaptor->outgoing.connection->connection.bluetooth.adaptor=adaptor;
	  adaptor->outgoing.connection->connection.bluetooth.socket=adaptor->outgoing.socket;

	  printf("Outgoing connection %x has id %d\n",
		 (unsigned int)  adaptor->outgoing.connection,adaptor->outgoing.connection->id);

	  PeisConnectMessage message;
	  peisk_initConnectMessage(&message,adaptor->outgoing.flags);
	  printf("Sending connection message:\n"); peisk_hexDump(&message,sizeof(message)); printf("\n");
	  send(adaptor->outgoing.socket,&message,sizeof(message),MSG_NOSIGNAL);
	  peisk_outgoingConnectFinished(adaptor->outgoing.connection,adaptor->outgoing.flags);

	  if(1 || peisk_printLevel & PEISK_PRINT_CONNECTIONS)
	    fprintf(stdout,"peisk: new outbound BLUETOOTH connection established\n");

	  adaptor->outgoing.mode = 0;
	}
      } 
    }
    if(adaptor->outgoing.timeout < peisk_timeNow &&
       adaptor->outgoing.mode != 0) {
      printf("Giving up on outgoing CONNECT\n");
      peisk_abortConnect(adaptor->outgoing.connection);
      close(adaptor->outgoing.socket);
      adaptor->outgoing.mode = 0;
    }
  }
}

int peisk_bluetoothSendAtomic(PeisConnection *connection,PeisPackage *package,int len) {
  int status;
  void *data = (void*) package;
  PeisPackageHeader *header;

  PEISK_ASSERT(connection->connection.bluetooth.adaptor >= &peisk_bluetoothAdaptors[0] &&
	       connection->connection.bluetooth.adaptor <= &peisk_bluetoothAdaptors[peisk_nBluetoothAdaptors],
	       ("ERROR: Invalid adaptor pointer in bluetooth connection #%d towards %d\n",
		connection->id,connection->neighbour.id));

  /* Refuse to send if we already have sent too many packages this period */
  if(connection->connection.bluetooth.adaptor->bsCount > PEISK_BT_MAX_BPS)
    return -1;
  /* Make a random chance to refuse packages if we have an average speed that is too high. 
     This lets the allowed bandwidth be evenly distributed between all connections on this interface, and also
     to be shared "over time" (ie. do not send everything at the begining of each period) */
  if(connection->connection.bluetooth.adaptor->bsAvgBps > PEISK_BT_RED_BPS) {
    if(rand() % (PEISK_BT_MAX_BPS-PEISK_BT_RED_BPS) < 
       connection->connection.bluetooth.adaptor->bsAvgBps-PEISK_BT_RED_BPS) 
      return -1;
  }

  header=&package->header;
  header->linkCnt = htonl(connection->outgoingIdCnt++);

  /** \todo Let the linklayer send operations send partial messages
      and queue the leftovers. */
  errno=0;
  status=send(connection->connection.bluetooth.socket,(void*) data,len,MSG_DONTWAIT|MSG_NOSIGNAL);
  if(status != -1 && status < len) {
    fprintf(stderr,"ERROR - only %d of %d bytes sent over L2CAP bluetooth connection\n",status,len);
    peisk_closeConnection(connection->id);
    return -1;
  }
  if(status > 0) {
    peisk_logTimeStamp(stdout);
    printf("BT send: %d bytes TO: %d ",status,connection->neighbour.id);
    /*peisk_hexDump(package,status);*/
    peisk_printNetPackageInfo(package);
    /* Increment bytes/second counter */
    connection->connection.bluetooth.adaptor->bsCount += status;
  }


  if(errno == EPIPE || errno == EBADF || errno == EINVAL) {
    if(1 || peisk_printLevel & PEISK_PRINT_CONNECTIONS) {
      fprintf(stdout,"peisk: bluetooth connection %d broken pipe (sendAtomic %d bytes)\n",connection->id,len);
      perror(":");
    }
    peisk_closeConnection(connection->id);
    return -1;
  } else if(errno == EAGAIN) {
    if(status > 0) { if(peisk_printLevel & PEISK_PRINT_PACKAGE_ERR) printf("send - EAGAIN, status=%d\n",status); }
    /* Package was not sent - don't remove it from queue and don't attempt any more packages for now */
    /* Also, decrease outgoingIdCnt so we do not count this package
       multiple times */
    connection->outgoingIdCnt--;
    return -1;
  }
  return 0;
}

int peisk_bluetoothReceiveIncomming(struct PeisConnection *connection,struct PeisPackage *package) {
  int status;
  double t0, t1;

  static float lastReceive=0.0; /* For debugging only */

  errno=0;
  t0 = peisk_gettimef();
  status=recv(connection->connection.bluetooth.socket,(void*)package,sizeof(PeisPackage),MSG_NOSIGNAL);
  t1 = peisk_gettimef();
  if(t1 - t0 > 0.1) {
    printf("Receive took %.3fs\n",t1-t0);
  }
  if(errno == EAGAIN || status == 0) return 0;       /* Nothing received, come back later */

  if(status > 0) {
    peisk_logTimeStamp(stdout);
    printf("BT receive: %d bytes, sock: %d ",status,connection->connection.bluetooth.socket);
    /*peisk_hexDump(package,status);*/
    peisk_printNetPackageInfo(package);
    lastReceive = peisk_timeNow;
  }

  if(errno == EPIPE || status == -1) {          /* Error - close socket */
    if(1 || peisk_printLevel & PEISK_PRINT_CONNECTIONS) {
      peisk_logTimeStamp(stdout);
      printf("peisk: warning - error EPIPE in BT receive, closing socket status: %d\n",status);
      perror("");
      printf("Time since last read: %.1fs\n",peisk_timeNow - lastReceive);
    }
    peisk_closeConnection(connection->id);
    return 0;
  }
  if(status < sizeof(package->header)) {
    /* For L2CAP it is an error to not receive the full data in each
       recv operation. */
    fprintf(stderr,"Warning, only got partial data on BT connection\n");
    peisk_closeConnection(connection->id);
    return 0;
  }
  if(ntohs(package->header.datalen) > PEISK_MAX_PACKAGE_SIZE) { /* Check for bad package length */
    if(1 || peisk_printLevel & PEISK_PRINT_CONNECTIONS)
      fprintf(stdout,"peisk: error, too large (%d) package received on BT connection %d\n",package->header.datalen,connection->id);
    /*peisk_syncflush(connection);*/
    peisk_closeConnection(connection->id);
    return 0;
  }
  return 1;
}

void peisk_bluetoothCloseConnection(struct PeisConnection *connection) {
  PEISK_ASSERT(connection->type == eBluetoothConnection,("Attempting to close non-bluetooth connection\n"));
  close(connection->connection.bluetooth.socket);
}

void peisk_bluetooth_setMTU(int sk) {
  printf("DEBUG peisk_bluetooth_setMTU DISABLED\n");
  return;

  struct l2cap_options opts;
  struct l2cap_conninfo conn;
  socklen_t optlen;

  memset(&opts, 0, sizeof(opts));
  optlen = sizeof(opts);
  if (getsockopt(sk, SOL_L2CAP, L2CAP_OPTIONS, &opts, &optlen) < 0) {
    perror("Can't get default L2CAP options");
  }
  /* Set new options */
  printf("Old L2CAP MTU: %d %d\n",opts.omtu,opts.imtu);
  opts.omtu = 300; //PEISK_MAX_PACKAGE_SIZE;
  opts.imtu = 300; //PEISK_MAX_PACKAGE_SIZE;
  /*        if (rfcmode > 0)
	    opts.mode = rfcmode;*/
  if (setsockopt(sk, SOL_L2CAP, L2CAP_OPTIONS, &opts, sizeof(opts)) < 0) {
    perror("Can't set L2CAP options: %s (%d)");
  }
  if (getsockopt(sk, SOL_L2CAP, L2CAP_OPTIONS, &opts, &optlen) < 0) {
    perror("Can't get default L2CAP options");
  }
  /* Set new options */
  printf("New L2CAP MTU: %d %d\n",opts.omtu,opts.imtu);
}

#endif



