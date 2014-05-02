/** \file services.h
   Defines some of the basic services implemented ontop of the P2P
   layer. Some of these integrate closely and can be considered a part
   of the P2P layer. 
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

#ifndef SERVICES_H
#define SERVICES_H

/** \ingroup peisk */
/** \defgroup GenericServices Internal Services
    This page groups together all of the PeisKernel services
    implemented on top of the P2P layer and which can not be
    considered part of any other layer (eg. part of the P2P layer or
    part of the tuple layer). 
 */
/** @{ */

/** \brief Broadcasted when a host is removing itself from the network. */
typedef struct PeisDeadHostMessage {
  int id;
  int why;
} PeisDeadHostMessage;

/** \brief Returned when requesting a traceroute to given node */
typedef struct PeisTracePackage {
  int hops;
  int hosts[PEISK_MAX_HOPS];
} PeisTracePackage;

/** \brief Host should be deleted because we have received a death notification. */
#define PEISK_DEAD_MESSAGE 0
/** \brief Host should be deleted because we have lost the route to him. */
#define PEISK_DEAD_ROUTE 1
/** \brief Host should be deleted because we have seen him die and be reborn. */
#define PEISK_DEAD_REBORN 2


/** \brief Period for how often we flush stdout/stderr */
#define PEISK_FLUSH_PERIOD  0.2


/** \brief Periodically generates some debuging information */
void peisk_periodic_debug(void *data);                    

/** \brief Creates informational kernel tuples for debugging and general info */
void peisk_periodic_kernelInfo(void *data);

/** \brief Periodically flushes stdout/stderr to work better with peisinit
    rerouting of them */
void peisk_periodic_flushPipes(void *data);

/** \brief Sends a traceroute package towards target, result will
    appear in peisk_traceResponse when available.  */
void peisk_sendTrace(int target);                               

/** \brief Responds to traceroute requests */
int peisk_hook_trace(int port,int destination,int sender,int datalen,void *data);


/** \brief Sends an exit notification to other components when this component exits. */
void peisk_sendExitNotification();

/** \brief Removes hosts announced as dead on the network from the knownHosts list */
int peisk_hook_deadHost(int port,int destination,int sender,int datalen,void *data); 

/** Sends broadcasted timesyncs to all other components. */
void peisk_periodic_timeSync(void *);
/** Receives broadcasted timesyncs and adjusts local time. 
    Updates timing information for all periodic functions and calls
    peisk_adjustTupleTimes(..) to reflect new timing information. */
int peisk_hook_timeSync(int port,int destination,int sender,int datalen,void *data);


/** Maximum number of authosts allowed to be registered */
#define PEISK_MAX_AUTOHOSTS          64                                   
/** Period between connection attempts on registered autohost */
#define PEISK_AUTHOST_PERIOD        30.0                                  

/** Used to record hosts will regularly be attempted to connect to. */
typedef struct PeisAutoHost {
  char *url;                     /**< Pointer to URL of host to attempt to connect to */
  int isConnected;               /**< Zero if not connected, otherwise id of current connection */
  double lastAttempt;            /**< Timepoint of last connection attempt */
} PeisAutoHost;

/** \defgroup NetworkTime Distributed Network Clock
    When communicating data between notes a shared notion of time is
    essential in order to correctly interpret when events and data was
    produced. Unfortunatly the internal clocks of types of devices we
    consider often can differ by several seconds or even minutes due
    to eg. incorrect configurations or a drift of the internal
    clock. In order to compensate for these discrepancies peiskernel
    implements a distributed network clock that is propagated to all
    nodes on the network.  

    double peisk_gettimef()
    Returns the current clock value on the network as the number of
    seconds and microseconds elapsed since the unix epoch.

    This clock value is usually monotonically increasing, but can be reset
    to a lower value under a few special situations. When the clock is
    moved in such a case, the timing information inside each tuple is also
    changed accordingly to preserve relative times for creation, expiry
    etc.  

    All timing information in tuples and periodics are expressed in
    terms of this network clock and all user programs should use this
    network clock to express time. 

    The network clock is effectively computed as a difference between
    the current clock of the host and the agreed upon clock. The
    protocoll for agreeing upon the clock value consists of a simple
    periodic broadcast that is done by one of the PEIS components in
    the network. If a component have recently seen such a broadcast by
    some other component with a higher peis-id then it will not
    broadcast. Otherwise it will send out it's own notion of the
    clock. When a clock value is received it is used to update the
    clock difference only if this would increase it.  

    A timemaster is a PEIS component that have been authorative
    information that it's initial clock is correct (eg. from external
    NTP clocks). Time messages from such a component always win over
    timing messages from less authoritative components.  

    Note that the distributed clock is only accurate to the latency of
    the network. There is currently no measurement of the network
    latencies or other way of compensating for this time delay.  
*/

/** \defgroup Autohost Automatic host connections
    \brief This term refers to a host that the peiskernel will always attempt
    to maintain an active connection to. A built in service
    will monitor this connection and reestablish it whenever it is
    closed.

    The main purpose of estabilishing autohosts are to either (a)
    connect networks which do not talk with  multicasts between each
    other or goes through firewalls or (b) connect to leaf nodes. 
*/

/** @} Generic Services */

#endif
