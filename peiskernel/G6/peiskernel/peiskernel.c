/** \file peiskernel.c
     Implements the peiskernel library.
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

/** \mainpage Peis Kernel

  \author Mathias Broxvall

  \version G6: v0.6.0.3

  The PeisKernel is a library that provides all services needed for 
  software programs to participate as PEIS components in the PEIS
  Ecology network.   

  It provies all the functionalitis for network and wireless
  communication as well as higher level functionalities for
  synchornizing clocks, providing semantic information, a shared
  tuplespace etc. 

  Users of the peiskernel should primarily see the \ref tuples
  "Tuplespace layer" and/or the 
  \ref peiskmt "multithreaded Peis Kernel" for the basic
  operations of how to use this library.   

  Developers, or expert users, may also want to read up on the
  different modules and layers that implement the peiskernel. You may
  want to start with the \ref PeisKernel "Peis Kernel" module. 
*/



#include <stdio.h>
#include <getopt.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/time.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/select.h>
#include <math.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define PEISK_PRIVATE
#include "peiskernel.h"
#include "peiskernel_tcpip.h"

#include "bluetooth.h"

/* Global variables */
PeisKernel peiskernel;

PeisPackageHeader *peisk_lastPackage;
int peisk_lastConnection;
int peisk_int_isRunning=0;
int peisk_debugPackages=0;
int peisk_printPortStatistics=0;
int peisk_id=-1;
int peisk_printLevel=-1;
char *peisk_networkString="default";

/** Try to keep each protocoll bump within one generation in multiples
    of 100. Ie, G4 = 400-499, G5 = 500-599 */
int peisk_protocollVersion=500;

int peisk_netMetricCost=2;

/** Keeps track of linenumbers as files are parsed. */
int peisk_lineno;

/** Temporary variables to hold commandline arguments */
int peisk_argc;
char *peisk_cl_args[64];
char *peisk_cl_hostname;
char *peisk_cl_componentName;
char *peisk_cl_user;

/** Helper function to give the next token from the 
    commandline or the currently loaded options file. */
char *peisk_cliArg();
/** Function to parse all commandline arguments */
void peisk_getOptions(int *argc,char **args);
/** The actual options parsing */
void peisk_parseOptions(FILE *fp,char *filename);

typedef struct PeisOptArg {
  char *name;
  int args;
} PeisOptArg;

PeisOptArg peisk_cliOptions[] = {
  {"port",1}, {"connect",1}, {"address", 1}, {"id", 1}, {"name",1}, {"hostname",1},
  {"network",1},
  {"debug-packages",0}, {"debug-routes",0}, {"debug-tuples",0},
  {"leaf", 0}, {"silent", 0},
  {"print-status", 0}, {"print-connections", 0}, {"print-package-errors", 0},
  {"print-tuple-errors", 0}, {"print-debug-steps", 0}, {"print-warnings", 0}, 
  {"print-port-statistics", 0}, {"user", 1}, 
  {"set-tuple",2},
  {"load",1},
  {"time-master",0},
  {"package-loss",1},
  {"net-metric",1},
  {"bluetooth",1},
  {NULL,-1},
};

void peisk_getOptions(int *argc,char **args) {
  int i,j,numArgs;

  peisk_argc=0;


  /* First, copy commandline options (removing leading "--peis-" and
  deleting them and their respective options from the given
  commandline structure. */
  for(i=0;i<*argc;i++) {
    if(strncmp(args[i],"--peis-",7) == 0) {
      peisk_cl_args[peisk_argc]=strdup(args[i]+7);
      /* See how many arguments this option needed */
      for(j=0;;j++) {
	if(!peisk_cliOptions[j].name) {
	  fprintf(stderr,"Error, unknown commandline option --peis-%s\n",peisk_cl_args[peisk_argc]);
	  break;
	}
	if(strcmp(peisk_cl_args[peisk_argc],peisk_cliOptions[j].name) == 0) {
	  break;
	}
      }
      if(!peisk_cliOptions[j].name) continue;
      numArgs=peisk_cliOptions[j].args;

      if(i + numArgs >= *argc) {
	fprintf(stderr,"Too few arguments remaining for option --peis-%s\n",peisk_cl_args[peisk_argc]);
      }

      /* Increment our peisk_argc for the option */
      peisk_argc++;

      /* Now copy this number of arguments to our list of args */
      for(j=0;j<numArgs;j++)
	peisk_cl_args[peisk_argc++]=strdup(args[i+1+j]);

      /* And delete the option and it's arguments from the given
	 commandline args array */
      for(j=i+numArgs+1;j<*argc;j++)
	args[j-numArgs-1]=args[j];
      *argc -= numArgs+1;
      
      /* Compensate for deleting this option from the array */
      i--;

    }
  }

  /* debug */
  /*  printf("peis arguments: ");
  for(i=0;i<peisk_argc;i++)
    printf("%s ",peisk_cl_args[i]);
  printf("\nremaining arguments: ");
  for(i=0;i<*argc;i++)
    printf("%s ",args[i]);
    printf("\n");*/
}

char *peisk_getNextOption(int *pos,FILE *fp) {
  if(fp) {
    char *token=peisk_getToken(fp);
    /*if(peisk_printLevel & PEISK_PRINT_STATUS) printf("peisk::read token '%s'\n",token); */
    return token;
  }
  else if(*pos >= peisk_argc) return NULL;
  else return peisk_cl_args[(*pos)++];
}

void peisk_parseOptions(FILE *fp,char *filename) {
  int pos=0;
  char *token,*arg;
  char *key, *value;

  while(1) {
    if(pos >= peisk_argc) return;
    token = peisk_getNextOption(&pos,fp);
    if(!token) return;

    if(strcmp(token,"port") == 0)
      peiskernel.tcp_serverPort=atoi(peisk_getNextOption(&pos,fp));
    else if(strcmp(token,"connect") == 0)
      peisk_autoConnect(peisk_getNextOption(&pos,fp));
    else if(strcmp(token,"address") == 0 || strcmp(token,"id") == 0) {
      arg=peisk_getNextOption(&pos,fp);
      peiskernel.id=atoi(arg);
      free(arg);
    }
    else if(strcmp(token,"debug-packages") == 0)
      peisk_debugPackages=1;
    else if(strcmp(token,"user") == 0) {
      peisk_cl_user=peisk_getNextOption(&pos,fp);
    }
    else if(strcmp(token,"debug-routes") == 0)
      peisk_debugRoutes=1;
    else if(strcmp(token,"debug-tuples") == 0)
      peisk_debugTuples=1;
    else if(strcmp(token,"name") == 0)
      peisk_cl_componentName=peisk_getNextOption(&pos,fp);
    else if(strcmp(token,"hostname") == 0)
      peisk_cl_hostname=peisk_getNextOption(&pos,fp);
    else if(strcmp(token,"leaf") == 0) {
      /*peiskernel.isLeaf=1;*/
      printf("Warning, ignoring --peis-leaf flag since it is no longer needed\n");
    } else if(strcmp(token,"silent") == 0)
      peisk_printLevel=0;
    else if(strcmp(token,"print-status") == 0)
      peisk_printLevel |= PEISK_PRINT_STATUS;
    else if(strcmp(token,"print-connections") == 0)
      peisk_printLevel |= PEISK_PRINT_CONNECTIONS;
    else if(strcmp(token,"print-package-errors") == 0)
      peisk_printLevel |= PEISK_PRINT_PACKAGE_ERR;
    else if(strcmp(token,"print-tuple-errors") == 0)
      peisk_printLevel |= PEISK_PRINT_TUPLE_ERR;
    else if(strcmp(token,"print-debug-steps") == 0)
      peisk_printLevel |= PEISK_PRINT_DEBUG_STEPS;
    else if(strcmp(token,"print-warnings") == 0)
      peisk_printLevel |= PEISK_PRINT_WARNINGS;
    else if(strcmp(token,"print-port-statistics") == 0)
      peisk_printPortStatistics=1;
    else if(strcmp(token,"time-master") == 0)
      peiskernel.isTimeMaster=1;
    else if(strcmp(token,"package-loss") == 0) {
      arg=peisk_getNextOption(&pos,fp);
      peiskernel.simulatePackageLoss=atof(arg);
      free(arg);
    }
    else if(strcmp(token,"net-metric") == 0) {
      arg=peisk_getNextOption(&pos,fp);
      peisk_netMetricCost=atoi(arg);
      if(peisk_netMetricCost<1||peisk_netMetricCost>16)
	fprintf(stderr,"Invalid --peis-net-metric argument!\n");
	peisk_netMetricCost=2;
    }
    else if(strcmp(token,"network") == 0) {
      peisk_networkString = peisk_getNextOption(&pos,fp);
      if(strlen(peisk_networkString)>62) {
	printf("Warning, too long --peis-network string, trunkating to 62 bytes\n");
	peisk_networkString[62]=0;
      }
    }
    else if(strcmp(token,"set-tuple") == 0) {
      if(peiskernel.id == -1) {
	fprintf(stderr,"Warning, cannot assign initial values to tuples without using the --peis-id option\n");
      }
      key = peisk_getNextOption(&pos,fp);
      value = peisk_getNextOption(&pos,fp);
      if(key && value)
	peisk_setStringTuple(key,value);
      else
	printf("Warning, invalid key/value pair to set-tuple: %s %s\n",key,value);
      free(key);
      free(value);
    }
    else if(strcmp(token,"bluetooth") == 0) {
      arg = peisk_getNextOption(&pos,fp);
      peisk_clUseBluetooth(arg);
      free(arg);
    }
    else if(strcmp(token,"load") == 0) {
      arg = peisk_getNextOption(&pos,fp);
      FILE *fp2 = fopen(arg,"rb");
      if(!fp2) {
	fprintf(stderr,"Warning, failed to load '%s'\n",arg);
      } else {
	int tmp=peisk_lineno;
	peisk_lineno=0;
	peisk_parseOptions(fp2,arg);
	peisk_lineno=tmp;
      }
      free(arg);
    }
    free(token);
  }
}



void peisk_printUsage(FILE *stream,short argc,char **args) {
  fprintf(stream,"peiskernel options:\n");
  fprintf(stream," --peis-id <int>                Specify an address to be used in the peis network\n");
  fprintf(stream," --peis-name <name>             Alternative name of component\n");
  fprintf(stream," --peis-user <name>             Alternative name of user\n");
  fprintf(stream," --peis-hostname <name>         Alternative name of host computer\n");
  fprintf(stream," --peis-port <port>             Select port for incomming connections\n");
  fprintf(stream," --peis-connect <url>           Specify an initial start connection to be used\n");
  fprintf(stream," --peis-debug-packages          Generates a lot of debug information\n");
  fprintf(stream," --peis-debug-routes            Print debuging information for routing tables\n");
  fprintf(stream," --peis-load <file>             Load extra commandlines from file\n");
  fprintf(stream," --peis-leaf                    Leaf node only allowing connections to localhost(s)\n");
  fprintf(stream," --peis-silent                  Suppress all printouts to stdout\n");
  fprintf(stream," --peis-time-master             Overrides time synchronisation of ecology\n");
  fprintf(stream," --peis-package-loss <float>    Introduces an artifical package loss for debugging\n");
  fprintf(stream," --peis-print-status            Enable printing status info (default off)\n");
  fprintf(stream," --peis-print-connections       Enable printing connection info (default off)\n");
  fprintf(stream," --peis-print-package-errors    Enable printing package errors (default off)\n");
  fprintf(stream," --peis-print-tuple-errors      Enable printing tuple errors (default off)\n");
  fprintf(stream," --peis-print-debug-steps       Enable printing out lots of debugging steps (default off)\n");
  fprintf(stream," --peis-print-warnings          Enable printing of misc warnings about which only Mathias cares about\n");
  fprintf(stream," --peis-print-port-statistics   Lots of port statistics that only Mathias cares about\n");
  fprintf(stream," --peis-network <string>        Set network connection string (default default)\n");
  fprintf(stream," --peis-set-tuple <key> <value> Assign tuple value to key\n");
  fprintf(stream," --peis-net-metric <value>      Cost to communicate over network (default 2)\n");
  fprintf(stream," --peis-bluetooth <device>      Use given bluetooth adaptor\n");
}

void peisk_initialize(int *argc,char **args) {
  int i, j;
  char name[256];
  char *hostname, *progname;
  unsigned char *ip;
  char tmp[256];
  int t0,t1;
  PeisConnectionMgrInfo *connMgrInfo;

  /*PeisHostInfo *hi; char *foo = (char*) &hi;
  printf("PeisHostInfo: %d bytes\n",sizeof(PeisHostInfo));
  printf("PeisHostInfoPackage: %d bytes\n",sizeof(PeisHostInfoPackage));
  */

  /**                                             **/
  /** Initialize global variables to safe values  **/
  /**                                             **/


  /* Shutdown any previously running peiskernel */
  if(peisk_int_isRunning) peisk_shutdown();

  /* Initialize random numbers */
  peisk_getrawtime2(&t0,&t1);
  /*srand((int) time(NULL));*/
  srand((int) t1);

  /* Setup default values for all parameters */
  peiskernel.id = rand() % 10000; /* Ugly fix to get a reasonable default id address */
  peiskernel.tcp_serverPort=8000;
  peiskernel.udp_serverPort=8000;
  peiskernel.tcp_isListening=0;
  peiskernel.lastStep = peisk_gettimef();
  peiskernel.nextConnectionId = 1;
  peisk_printLevel = 0; /* PEISK_PRINT_STATUS | PEISK_PRINT_CONNECTIONS; */ /* MB - was 0 */
  peiskernel.highestPeriodic=0;
  peiskernel.highestConnection=0;
  hostname = NULL;
  peiskernel.isLeaf=0;
  peiskernel.timeOffset[0]=0;
  peiskernel.timeOffset[1]=0;
  peiskernel.isTimeMaster=0;
  peiskernel.avgStepTime=0.1;
  peiskernel.incomingTraffic=0;
  peiskernel.outgoingTraffic=0;
  peiskernel.nAcknowledgementPackages=0;
  peiskernel.deadhostHook = NULL;
  peiskernel.nAckHooks = 0;
  peiskernel.tick = 0;
  peiskernel.broadcastingCounter = 0;
    
  peisk_cl_user = getenv("USER");
  if(!peisk_cl_user) peisk_cl_user = "unknown";

  /* Setup traps for all important signals */
  signal(SIGINT, peisk_trapCtrlC);
  signal(SIGTERM, peisk_trapCtrlC);
  signal(SIGPIPE, peisk_trapPipe);

  /* Initialize TCP/IP interfaces */
  peiskernel_initNetInterfaces();

  /* Clear old connections */
  for(i=0;i<PEISK_MAX_CONNECTIONS;i++) {
    peiskernel.connections[i].id=-1;
    peiskernel.connections[i].routingTable = NULL;
    for(j=0;j<PEISK_MAX_ROUTING_PAGES;j++)
      peiskernel.connections[i].routingPages[j] = NULL;
  }
  peiskernel.freeQueuedPackages=NULL;
  peiskernel.nDirConnReqs=0;

  /* We do not want to shutdown immediatly */
  peiskernel.doShutdown=0;

  /* Remove all old hooks */
  for(i=0;i<PEISK_NPORTS;i++) peiskernel.hooks[i]=NULL;

  /* Remove all old periodic functions */
  for(i=0;i<PEISK_MAX_PERIODICS;i++) peiskernel.periodics[i].periodicity=-1.0;

  /* Clear assembly buffers */
  for(i=0;i<PEISK_MAX_LONG_MESSAGES;i++) {
	peiskernel.assemblyBuffers[i].seqid = 0;
	peiskernel.assemblyBuffers[i].seqlen = 0;
	peiskernel.assemblyBuffers[i].allocated_seqlen = 0;
	peiskernel.assemblyBuffers[i].received = NULL;
	peiskernel.assemblyBuffers[i].data = NULL;
  }

  /* Initialize routing information */
  peiskernel.routingTable = peisk_hashTable_create(PeisHashTableKey_Integer);

  // --AS
  if(peiskernel.routingTable == NULL) {
    fprintf(stderr,"peisk: error, failed to create routing table\n");
    exit(-1);
  }

  for(i=0;i<PEISK_LOOPINFO_HASH_SIZE;i++) peiskernel.loopHashTable[i]=-1;
  for(i=0;i<PEISK_LOOPINFO_SIZE;i++) peiskernel.loopTable[i].id=-1;

  /**                     **/
  /** Initialize modules  **/
  /**                     **/  

  peisk_initBluetooth();
  peisk_initP2PLayer();
  peisk_registerDefaultServices();

  /**                            **/
  /** Parse commandline options  **/
  /**                            **/

  /* Copy and clean commandline options */
  peisk_getOptions(argc,args);
  /* Do the actual options parsing */
  peisk_parseOptions(NULL,"command-line");

  /**                            **/
  /**     Start everything       **/
  /**                            **/


  /* Startup server */
  peisk_restartServer();
  if(peiskernel.tcp_isListening)
    if(peisk_printLevel & PEISK_PRINT_STATUS)
      printf("peisk: serving at port %d\n",peiskernel.tcp_serverPort);
  peisk_id = peiskernel.id;
  peiskernel.magicId = rand();

  /* Start services that need all commandline options to have been
     parsed first */
  peisk_registerDefaultServices2();

  /**                                         **/
  /** Create hostInfo structure for this peis **/
  /**                                         **/

  /* Backup mechanism for determining hostname if not given as commandline argument */
  hostname=peisk_cl_hostname;
  if(!hostname) hostname=getenv("HOSTNAME");
  if(!hostname) {
    FILE *fp = fopen("/etc/hostname","r");
    if(!fp) fp = fopen("/etc/HOSTNAME","r");
    if(!fp) { if(system("hostname > /tmp/peisk_cl_hostname")!=-1) fp = fopen("/tmp/peisk_cl_hostname","r"); }
    if(fp) {
      if(!fgets(name,255,fp)) sprintf(name,"johndoe");
      fclose(fp);
      for(i=0;i<255&&name[i];i++) if(name[i]=='\n' || name[i]==' ' || name[i] == '.') name[i]=0;
      hostname=strdup(name);
    }
    else {
      char name[256];
      sprintf(name,"johndoe-%d",peisk_id / 100);
      hostname=strdup(name);
    }
  }

  /* Cleanup the hostname (remove any suffix'ed domain parts) */
  /* Note. we do not keep the domain name since it anyway is usually not detected correctly
     by the backup mechanisms above in a consistent way between all hosts on the same domain. */
  for(i=0;i<255;i++) {
    if(hostname[i] == 0 || hostname[i] == '.' || hostname[i] == ' ') break;
    else name[i] = hostname[i];
  }
  name[i]=0;

  /* This removes all initial directories from program path/name */
  if(peisk_cl_componentName)
    progname=peisk_cl_componentName;
  else {
    for(progname = args[0]+strlen(args[0]);progname>=args[0]&&*progname!='/';progname--) {} progname++;
  }

  peiskernel.hostInfo.id = peiskernel.id;
  peiskernel.hostInfo.magic = peiskernel.magicId;
  peiskernel.hostInfo.networkCluster = peiskernel.id;
  int mypid = getpid();
  snprintf(peiskernel.hostInfo.fullname,sizeof(peiskernel.hostInfo.fullname),"%s@%s!%d",progname,name,mypid);
  snprintf(peiskernel.hostInfo.hostname,sizeof(peiskernel.hostInfo.hostname),"%s",name);

  /* Add default tuples */
  peisk_setStringTuple("kernel.hostname",peiskernel.hostInfo.hostname);
  snprintf(tmp,sizeof(tmp),"v%s (proto %d)",VERSION,peisk_protocollVersion);
  peisk_setStringTuple("kernel.version",tmp);
  peisk_setStringTuple("kernel.name",progname);
  peisk_setStringTuple("kernel.do-quit","");
  peisk_setStringTuple("kernel.user",peisk_cl_user);
  snprintf(tmp,sizeof(tmp),"%d",peiskernel.id);
  peisk_setStringTuple("kernel.id",tmp);


  if(peisk_printLevel & PEISK_PRINT_STATUS)
    printf("peisk #%d: started\n",peisk_id);
  /*, name=%s\n",peiskernel.hostInfo.fullname,peiskernel.id);*/

  /** Compute all lowlevel addresses we can be reached on */
  peiskernel.hostInfo.nLowlevelAddresses=0;

  if(peiskernel.tcp_isListening) {

    for(i=0,j=peiskernel.hostInfo.nLowlevelAddresses;i<peisk_nInetInterfaces;i++) {
      /* If this is a leaf node, then ignore all network interfaces
	 which are not loopback interfaces */
      if(peiskernel.isLeaf && !peisk_inetInterface[i].isLoopback) continue;

      /* DEBUG - we are disabling UDP for now... */
      if(peiskernel.udp_serverPort != 0  && 0) {
	peiskernel.hostInfo.lowAddr[j].type = ePeisUdpIPv4;
	ip = (unsigned char *)&peisk_inetInterface[i].ip;
	peiskernel.hostInfo.lowAddr[j].addr.udpIPv4.ip[0]=ip[0];
	peiskernel.hostInfo.lowAddr[j].addr.udpIPv4.ip[1]=ip[1];
	peiskernel.hostInfo.lowAddr[j].addr.udpIPv4.ip[2]=ip[2];
	peiskernel.hostInfo.lowAddr[j].addr.udpIPv4.ip[3]=ip[3];
	peiskernel.hostInfo.lowAddr[j].addr.udpIPv4.port=peiskernel.udp_serverPort;
      } else {
	peiskernel.hostInfo.lowAddr[j].type = ePeisTcpIPv4;
	ip = (unsigned char *)&peisk_inetInterface[i].ip;
	peiskernel.hostInfo.lowAddr[j].addr.tcpIPv4.ip[0]=ip[0];
	peiskernel.hostInfo.lowAddr[j].addr.tcpIPv4.ip[1]=ip[1];
	peiskernel.hostInfo.lowAddr[j].addr.tcpIPv4.ip[2]=ip[2];
	peiskernel.hostInfo.lowAddr[j].addr.tcpIPv4.ip[3]=ip[3];
	peiskernel.hostInfo.lowAddr[j].addr.tcpIPv4.port=peiskernel.tcp_serverPort;
      }
      strncpy(peiskernel.hostInfo.lowAddr[j].deviceName,peisk_inetInterface[i].name,sizeof(peiskernel.hostInfo.lowAddr[i].deviceName));
      peiskernel.hostInfo.lowAddr[j].isLoopback = peisk_inetInterface[i].isLoopback ? 1 : 0;
      j++;
    }
    peiskernel.hostInfo.nLowlevelAddresses=j;
  }

  peisk_addBluetoothLowlevelAddresses();

  /* Initialize host/neighbor information hashtables */
  peiskernel.hostInfoHT = peisk_hashTable_create(PeisHashTableKey_Integer);
  peiskernel.connectionMgrInfoHT = peisk_hashTable_create(PeisHashTableKey_Integer);

  /* Insert our own host into the list of known hosts */
  peisk_insertHostInfo(peiskernel.id,&peiskernel.hostInfo);
  connMgrInfo = (PeisConnectionMgrInfo*) malloc(sizeof(PeisConnectionMgrInfo));
  peisk_insertConnectionMgrInfo(peiskernel.id,connMgrInfo);
  connMgrInfo->nTries = 0;
  connMgrInfo->nextRetry = peisk_timeNow;
  connMgrInfo->usefullTraffic=0;
  connMgrInfo->lastUsefullTraffic=0;

  /* Set knownHosts[0] to ourselves */
  /*peisk_knownHosts[0] = peiskernel.hostInfo;
  peisk_knownNeighbours[0].nTries=-1;
  peisk_knownNeighbours[0].nextRetry=0;
  */

  printf("PeisKernel v%s (protocoll %d)@%s %s ",VERSION,peisk_protocollVersion,__DATE__,__TIME__);
  peisk_printHostInfo(&peiskernel.hostInfo);


  /* Everything initialized */
  peisk_int_isRunning=1;

  PeisRoutingInfo *routingInfo;
  if(peisk_hashTable_getValue(peiskernel.routingTable,(void*)(intA)peiskernel.id,(void**)(void*)&routingInfo) == 0) {
    PEISK_ASSERT(routingInfo->connection == NULL,("Route to ourselves points to nonzero connection\n"));
  }
}

void peisk_shutdown() {
  int i;

  if(!peisk_int_isRunning) 
    /* Don't attempt multiple shutdown's at once */
    return;
  peiskernel.doShutdown=2;

  /* Before shutting down completly - send notifications of our
     imminent death */
  peisk_sendExitNotification();

  /* Run for two more seconds so shutdown messages has a chance of beeing propagated to everyone */
  printf("peiskernel shutting down\n");
  peisk_wait(2000000);
  peisk_int_isRunning=0;


  /* Close all open connections */
  for(i=0;i<PEISK_MAX_CONNECTIONS;i++) {
    if(peiskernel.connections[i].id == -1) continue;
    peisk_closeConnection(peiskernel.connections[i].id);
  }

  /* Close server socket */
  if(peiskernel.tcp_isListening) { peiskernel.tcp_isListening=0; close(peiskernel.tcp_serverSocket); }

  if(peiskernel.tcp_broadcast_receiver != -1) { close(peiskernel.tcp_broadcast_receiver); peiskernel.tcp_broadcast_receiver=-1; }

  /* Close bluetooth devices */
  peisk_closeBluetooth();
}

void peisk_trapCtrlC(int Sig) {
  /* Catches Ctrl-C signals to shutdown cleanly */
  fprintf(stderr,"peisk: caught ctrl-c\n");
  if(peisk_int_isRunning==0 || peiskernel.doShutdown != 0) {
    fprintf(stderr,"peisk: repeated ctrl-c, doing hard shutdown\n");
    exit(-1);
  }
  /* Instead of calling shutdown immediatly we signal that this needs to be
     done by the next thread (?) which visits the peisk_step function. */
  peiskernel.doShutdown=1;
}
void peisk_trapPipe(int Sig) {
  /* \todo Find which socket caused signal and close connection */
  fprintf(stderr,"peisk: broken pipe: %d\n",Sig);
  fflush(stderr);
  /* NOTE - does not work, how do we know which socket it is that broke????
     int i;
     for(i=0;i<PEISK_MAX_CONNECTIONS;i++) {
     if(peisk_printLevel & PEISK_PRINT_STATUS)
     printf("Connection %d socket=%d\n",i,peiskernel.connections[i].connection.tcp.socket);
     if(peiskernel.connections[i].id != -1 && peiskernel.connections[i].connection.tcp.socket == Sig)
     peisk_closeConnection(peiskernel.connections[i].id);
     }
  */
}


void peisk_getrawtime2(int *t0,int *t1) {
  struct timeval tv;
  gettimeofday(&tv,NULL);
  *t0 = tv.tv_sec;
  *t1 = tv.tv_usec;
}
void peisk_gettime2(int *t0,int *t1) {
  struct timeval tv;
  gettimeofday(&tv,NULL);
  *t0 = tv.tv_sec + peiskernel.timeOffset[0];
  *t1 = tv.tv_usec + peiskernel.timeOffset[1];
  *t0 += *t1 / 1000000;
  *t1 = *t1 % 1000000;
  if(*t1<0) { *t1 += 1000000; *t0 -= 1; }
}

int peisk_gettime() {
  int timenow[2];
  peisk_gettime2(&timenow[0],&timenow[1]);
  return timenow[0];
}

double peisk_gettimef() {
  int timenow[2];
  peisk_gettime2(&timenow[0],&timenow[1]);
  return ((double) timenow[0]) + 1e-6*((double)timenow[1]);
}
double peisk_getrawtimef() {
  int timenow[2];
  peisk_getrawtime2(&timenow[0],&timenow[1]);
  return ((double) timenow[0]) + 1e-6*((double)timenow[1]);
}

int peisk_registerHookWithName(int port,PeisHook *hook,char *name) {
  PeisHookList *list = (PeisHookList *) malloc(sizeof(PeisHookList));
  if(port < 0 || port > PEISK_NPORTS) return -2;
  list->hook = hook;
  list->name = name;
  list->next = peiskernel.hooks[port];
  peiskernel.hooks[port] = list;
  return 0;
}

int peisk_unregisterHook(int port,PeisHook *hook) {
  if(port < 0 || port > PEISK_NPORTS) return -1;
  PeisHookList *list = peiskernel.hooks[port];
  PeisHookList **prev = &peiskernel.hooks[port];
  while(list) {
    if(list->hook == hook) {
      *prev = list->next;
      free(list);
      break;
    }
    prev = &(list->next);
    list=list->next;
  }

  if(peiskernel.hooks[port]) { peiskernel.hooks[port]=NULL; return 1; }
  return 0;
}

PeisHookList *peisk_lookupHook(int port) {
  if(port < 0 || port > PEISK_NPORTS) return NULL;
  return peiskernel.hooks[port];
}

void peisk_registerPeriodicWithName(double period,void *data,PeisPeriodic *hook,char *name) {
  int i;
  for(i=0;i<PEISK_MAX_PERIODICS;i++) if(peiskernel.periodics[i].periodicity == -1.0) break;
  if(i == PEISK_MAX_PERIODICS) {
    fprintf(stderr,"peisk: error, too many periodic functions registered\n");
    exit(0);
  }
  peiskernel.periodics[i].periodicity = period;
  peiskernel.periodics[i].data = data;
  peiskernel.periodics[i].hook = hook;
  peiskernel.periodics[i].last = peisk_gettimef();
  peiskernel.periodics[i].name = name;
  if(i > peiskernel.highestPeriodic) peiskernel.highestPeriodic = i;
}

void peisk_wait(int useconds) {
  double t0=peisk_gettimef()+1e-6*useconds;
  fd_set readSet, writeSet, excpSet;
  int n=0;
  int ret;

#ifdef GUMSTIX
  struct timeval timeout;
#else
  struct timespec timeout;
#endif

  double offset=peiskernel.timeOffset[0] + 1e-6*peiskernel.timeOffset[1];
  double offset2;

  peisk_step();
  while(peisk_int_isRunning)  {

    /* Compensate if kernel clock has changed */
    offset2=peiskernel.timeOffset[0]+1e-6*peiskernel.timeOffset[1];
    if(fabs(offset2 - offset) > 0.1) {
      /*printf("adjusting time to sleep to with %f secs\n",offset2-offset);*/
      t0 += offset2-offset; offset=offset2;
    }

    timeout.tv_sec = 0;
#ifdef GUMSTIX
    timeout.tv_usec = MIN((int)1e4,
			  (int)(1e6 * fmod((t0 - peisk_gettimef()),1.0)));
    if(timeout.tv_usec <= 0) break;
#else
    timeout.tv_nsec = MIN((int)1e7,
			  (int)(1e9 * fmod((t0 - peisk_gettimef()),1.0)));
    if(timeout.tv_nsec <= 0) break;
#endif

    FD_ZERO(&readSet);
    FD_ZERO(&writeSet);
    FD_ZERO(&excpSet);
    n=0;

    if(peiskernel.avgStepTime < 0.005) {
      /* Perform a forced sleep if we are running to quickly to
	 compensate for the pselect bug */

    } else {
      /* Perform a normal sleep for 10ms or until we have new data
	 available */
      peisk_setSelectReadSignals(&n,&readSet,&writeSet,&excpSet);
    }

    errno=0;
#ifdef GUMSTIX
    ret=select(n,&readSet,&writeSet,&excpSet,&timeout);
#else
    ret=pselect(n,&readSet,&writeSet,&excpSet,&timeout,NULL);
#endif

    if(ret == -1) perror("peisk_wait::select");

    /*
      double t1=peisk_gettimef();
      if(t1 - t0 < 0.0001) {
      printf("sleep: %3.6f nsec=%d ret=%d ",t1-t0,timeout.tv_nsec,ret);
      int i;
      printf("FD: ");
      for(i=0;i<n;i++) {
      if(FD_ISSET(i,&readSet)) printf("r%d ",i);
      if(FD_ISSET(i,&writeSet)) printf("w%d ",i);
      if(FD_ISSET(i,&excpSet)) printf("e%d ",i);
      }
      printf("\n"); fflush(stdout);
      }*/

    peisk_step();
  }
}

void peisk_waitOneCycle(int maxUSeconds) {
  fd_set readSet, writeSet, excpSet;
  int n=0;
  int ret;

#ifdef GUMSTIX
  struct timeval timeout;
#else
  struct timespec timeout;
#endif


  timeout.tv_sec = 0;
#ifdef GUMSTIX
  timeout.tv_usec = MIN((int)1e4,
			(int)(1e6 * fmod(1e-6*maxUSeconds,1.0)));
  if(timeout.tv_usec <= 0) return;
#else
  timeout.tv_nsec = MIN((int)1e7,
			(int)(1e9 * fmod(1e-6*maxUSeconds,1.0)));
  if(timeout.tv_nsec <= 0) return;
#endif

  FD_ZERO(&readSet);
  FD_ZERO(&writeSet);
  FD_ZERO(&excpSet);
  n=0;

  if(peiskernel.avgStepTime < 0.005) {
    /* Perform a forced sleep if we are running to quickly to
       compensate for the pselect bug */
  } else {
    /* Perform a normal sleep for 10ms or until we have new data
       available */
    peisk_setSelectReadSignals(&n,&readSet,&writeSet,&excpSet);
  }

  /** \todo let the wait functions wake up when an outgoing socket to
      which we are trying to send (a) is available for writing and (b)
      has any data to send. Also consider how to deal with new data
      from multithreaded applications for this! */

  errno=0;
#ifdef GUMSTIX
  ret=select(n,&readSet,&writeSet,&excpSet,&timeout);
#else
  ret=pselect(n,&readSet,&writeSet,&excpSet,&timeout,NULL);
#endif
  
  if(ret == -1) perror("peisk_wait::select");
}

int peisk_hashString(char *string) {
  int val=0, i;
  for(i=0;string[i];i++) val += tolower(string[i])*(i+1);
  return val;
}


int peisk_waitForRead(int fd,double t) {
  fd_set readSet,writeSet,excpSet;
  int n;
#ifdef GUMSTIX
  struct timeval timeout;
  timeout.tv_sec = (int) t;
  timeout.tv_usec = (int) (fmod(t,1.0)*1e6);
#else
  struct timespec timeout;
  timeout.tv_sec = (int) t;
  timeout.tv_nsec = (int) (fmod(t,1.0)*1e9);
#endif

  n=fd;
  FD_ZERO(&readSet);
  FD_ZERO(&writeSet);
  FD_ZERO(&excpSet);
  FD_SET(fd,&readSet);

#ifdef GUMSTIX
  if(select(n,&readSet,&writeSet,&excpSet,&timeout))
    return 1;
  else
    return 0;
#else
  if(pselect(n,&readSet,&writeSet,&excpSet,&timeout,NULL))
    return 1;
  else
    return 0;
#endif
}

int peisk_waitForWrite(int fd,double t) {
  fd_set readSet,writeSet,excpSet;
  int n;

#ifdef GUMSTIX
  struct timeval timeout;
  timeout.tv_sec = (int) t;
  timeout.tv_usec = (int) (fmod(t,1.0)*1e6);
#else
  struct timespec timeout;
  timeout.tv_sec = (int) t;
  timeout.tv_nsec = (int) (fmod(t,1.0)*1e9);
#endif

  n=fd;
  FD_ZERO(&readSet);
  FD_ZERO(&writeSet);
  FD_ZERO(&excpSet);
  FD_SET(fd,&writeSet);

#ifdef GUMSTIX
  if(select(n,&readSet,&writeSet,&excpSet,&timeout))
    return 1;
  else
    return 0;
#else
  if(pselect(n,&readSet,&writeSet,&excpSet,&timeout,NULL))
    return 1;
  else
    return 0;
#endif
}

void peisk_hexDump(void *data,int datalen) {
  /* Print a hex-dump of the received data */
  int i, row, nRows=datalen/16;
  for(row=0;row<nRows||(row==nRows && datalen%16 != 0);row++) {
    /* Print hex values */
    for(i=0;i<16 && (row < nRows || i < (datalen%16));i++) {
      printf("%02x",((unsigned char*)data)[i+row*16]);
      if(i % 4 == 3) printf(" ");
    }
    /* Fill out remaining space */
    for(;i<16;i++) {
      printf("  ");
      if(i % 4 == 3) printf(" ");
    }
    /* Print chars */
    printf("    ");

    for(i=0;i<16 && (row < nRows || i < (datalen%16));i++) {
      unsigned char c = ((unsigned char*)data)[i+row*16];
      if(isprint(c))
	printf("%c",((unsigned char*)data)[i+row*16]);
      else
	printf("@");
      if(i % 4 == 3) printf(" ");
    }
    printf("\n");
  }
}

char *peisk_getToken(FILE *fp) {
  char c;
  int pos=0;
  int isnum;

  char *token;
  int buflen;
  buflen=256;
  token = malloc(buflen);

  /* Skip all whitespace */
  while(1) {
    if(feof(fp)) return NULL;   /* Error, end of file before next token */
    c=getc(fp);
    if(c == '\n') peisk_lineno++;
    if(c == '#') {
      /* Begining of comment, skip to end of line */
      while(1) { if(feof(fp)) return NULL; c=getc(fp);  if(c == '\n') peisk_lineno++; if(c == '\n') break; }
      continue;
    }
    if(!isspace(c)) break;
  }
  if(c == EOF) return NULL;
  /* c is now first non whitespace character */
  token[pos++]=c; token[pos]=0;

  /* Some characters are whole tokens by themselves. */
  if(c == '=' || c == ',' || c == '{' || c == '}' || c == '[' || c == ']') return token;

  if(isdigit(c)) isnum=1;
  else isnum=0;

  /* Handle strings in a special way */
  /* "stri ngs" -> "stri ngs" while 'stri ngs' -> stri ngs */
  if(c == '"' || c == '\'') {
    if(c == '\'') pos--;
    while(1) {
      if(feof(fp)) {
	/* End of file inside string, not ok */
	token[pos]=0; printf("Error, EOF inside string:\n%s\n",token);
	return NULL;
      }
      c=getc(fp);
      if(c == '\n') peisk_lineno++;
      if(c == '"') { token[pos++]=c; token[pos]=0; return token; }
      if(c == '\'') { token[pos]=0; return token; }
      if(c == '\\') {
	if(feof(fp)) {
	  /* End of file inside string, not ok */
	  token[pos]=0; printf("Error, EOF inside string:\n%s\n",token);
	  return NULL;
	}
	c=getc(fp);
	if(c == '\n') peisk_lineno++;
      }
      token[pos++]=c;

      /* Check if we need a larger buffer to hold whole string */
      if(pos >= buflen-5) {
	/* Buffer not long enough to read token */
	buflen += 256;
	token = realloc(token,buflen);
	if(!token) {
	  /* Error, cannot allocate new memory for buffer */
	  printf("Cannot allocate memory hold token\n");
	  return NULL;
	}
      }

    }
  }

  while(1) {
    if(feof(fp)) return token; /* End of file during token, it's ok */
    c=getc(fp);
    if(c == '\n') peisk_lineno++;
    if(isspace(c)) return token; /* Token finished */
    if(c == '=' || c == ',' || c == '{' || c == '}' || c == '[' || c == ']' || c == '"' || c == '\'') {
      ungetc(c,fp);
      if(c == '\n') peisk_lineno--;
      return token;
    }
    token[pos++]=c; token[pos]=0;
    if(pos >= buflen - 5)  {
      /* Buffer not long enough to read token */
      buflen += 256;
      token = realloc(token,buflen);
      if(!token) {
	/* Error, cannot allocate new memory for buffer */
	return NULL;
      }
      return token;
    }
  }
}

int peisk_localTCPServerPort() {
  return peiskernel.tcp_serverPort;
}

int peisk_isRunning() { return peisk_int_isRunning; }
int peisk_peisid() { return peisk_id; }

PeisHostInfo *peisk_lookupHostInfo(int id) {
  PeisHostInfo *hostInfo;
  if(peisk_hashTable_getValue(peiskernel.hostInfoHT,(void*)(intA)id,(void**) &hostInfo) == 0)
    return hostInfo;
  else return NULL;
}
PeisConnectionMgrInfo *peisk_lookupConnectionMgrInfo(int id) {
  PeisConnectionMgrInfo *connMgrInfo;
  if(peisk_hashTable_getValue(peiskernel.connectionMgrInfoHT,(void*)(intA)id,(void**) &connMgrInfo) == 0)
    return connMgrInfo;
  else return NULL;
}

void peisk_insertHostInfo(int id,PeisHostInfo *hostInfo) {
  /*printf("Inserting %d into known hosts\n",id);*/
  peisk_hashTable_insert(peiskernel.hostInfoHT,(void*)(intA)id,(void *) hostInfo);
}

void peisk_insertConnectionMgrInfo(int id,PeisConnectionMgrInfo *connMgrInfo) {
  peisk_hashTable_insert(peiskernel.connectionMgrInfoHT,(void*)(intA)id,(void *) connMgrInfo);
}

void peisk_printHostInfo(PeisHostInfo *hostInfo) {
  int i,j;
  printf("%d: %s\nInterfaces: ",hostInfo->id,hostInfo->fullname);
  for(i=0;i<hostInfo->nLowlevelAddresses;i++) {
    if(hostInfo->lowAddr[i].type == ePeisTcpIPv4)
      printf("%d.%d.%d.%d:%d ",
	     hostInfo->lowAddr[i].addr.tcpIPv4.ip[0],hostInfo->lowAddr[i].addr.tcpIPv4.ip[1],
	     hostInfo->lowAddr[i].addr.tcpIPv4.ip[2],hostInfo->lowAddr[i].addr.tcpIPv4.ip[3],
	     (int) hostInfo->lowAddr[i].addr.tcpIPv4.port);
    else if(hostInfo->lowAddr[i].type == ePeisBluetooth) {
      for(j=0;j<6;j++) printf("%02X%c",hostInfo->lowAddr[i].addr.bluetooth.baddr[j],j==6?';':':');
      printf("%d ",hostInfo->lowAddr[i].addr.bluetooth.port);
    }
  }
  printf("\n");
}

void peisk_logTimeStamp(FILE *stream) {
  fprintf(stream,"%3.2f ",fmod(peisk_gettimef(),1000));
}

const char *peisk_hostname() {
  return peiskernel.hostInfo.hostname;
}
