/** \file peisproxy.h
    \todo Write the description of this file.
*/
/*
    Copyright (C) 2005 - 2006  Mathias Broxvall

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

#ifndef PEISPROXY_H
#define PEISPROXY_H

#include <peiskernel/peiskernel_mt.h>

/** \defgroup PeisProxy 
    \brief say something about peisproxy here

    say something longer here

    @{
*/

/** \todo Write comments.*/
#define PEISPROXY_INACTIVATE_IFACE 10
#define PEISPROXY_CHANNEL_NAME_SIZE 64
#define PEISPROXY_PREFIX_SIZE PEISPROXY_CHANNEL_NAME_SIZE+10
#define PEISPROXY_CONTINUE 10
#define PEISPROXY_SUCCESS 0
#define PEISPROXY_FAIL -1
#define PEISPROXY_ERROR 1

/***********Dubugging******************/
#define PROXY_DEBUG
int debug;
int subdebug; ///< for debugging sublist.c file.
int ifacedebug;

/***********************************/
typedef void (ProxyDataTranslator)(PeisTuple *tuple, void *interface );
typedef void (InterfaceArbiter)(PeisTuple *tuple, void *interface);
typedef void (ObserverArbiter)(PeisTuple *tuple, void *interface);
typedef void (ProxyInterfaceInit)(void *interface);
typedef void (ProxyChannelInit)(void *channel);


typedef struct IfaceMetaInfo{
  char oldSignature[100];
  int oldObserverData; /// < subscribed to for signature-data 
  int oldObserverIface;/// < subscribed to for interface
		       /// detection. Actually this one just for
		       /// efficient peis middleware implementation.
  
}IfaceMetaInfo;

/** The structure for define an interface for the proxy program.
 */
typedef struct ProxyChannel{
  char *tuple_prefix;
  int iface_no;
  char type[PEISPROXY_CHANNEL_NAME_SIZE];
  char unique_id[33]; ///< this is the machine address of the used hardware.
  char *signature_data; ///< hardware written data.
  int  observer; 
  int  RSSI;
  char last_sensed_time[33];
  int my_inactivation_time;
  int status;
  int used_sensors;

  ProxyDataTranslator *datatranslator;
  InterfaceArbiter *fnArbiter;
  
  //PeisMetaInfo *peisMetaInfo;
  IfaceMetaInfo metaInfo;
}ProxyChannel;

/** The wrapper for the ProxyChannel structure. The application
    programmer should use ProxyChannel instead of ProxyInterface.*/
typedef ProxyChannel ProxyInterface;


/** The basic structure for the proxy component. */
typedef struct PeisProxy{
  char name[65]; ///< name is not used as it has come from peiskernel.
  /** There are only two status: 1:active and 0:inactive. */
  int status;
  char signature[256];
  char last_sensed_time[32];
  int my_inactivation_time;
}PeisProxy;


/** \todo This structure should be implemented for indirect information.*/
typedef struct ProxyIndirectInfo{
  char tuplename[65];
  char defalutValue[65];
  int reset_duration;
  /**** meta info **/
  ProxyInterface *iface;
}ProxyIndirectInfo;

/******************************************/

void proxy_usages(int argc, char **args);

/** This function creates and initializes necessary ingredients. For
    example, creating the hashtable for collecting sub/callback
    handlers. A callback function is implemented for the general
    tuples of the proxy component. They are status, unique-id and
    last-sensed-time. Whether the application programmer can assign
    callback for these tuples again or not, it depends on the used
    middleware implementation. The PeisMiddleware does that. */
void proxy_init(PeisProxy *proxy);

/** Returns the current status of the proxy component.*/
int proxy_getProxyStatus(PeisProxy *proxy);

/** Takes the user function for initializing the channel
    structure. In the user function, the (void *userdata) should be
    casted as ProxyChannel before using it. 
*/
void proxy_channelInit(ProxyChannel *channel, ProxyChannelInit *fnInit);

/** This function publishes the content of the given channel as tuples
    at present. The prefix of the tuples is "proxy.ifaceN.*". where
    the "*" stands for the structure's fields and
    N->channel->iface_no; */
void proxy_publishChannelAsTuples(ProxyChannel *channel);

/** This function sets the inactivation time of the given
    channel. Time is in second. */
void proxy_setChannelInactivationTime(ProxyChannel *channel, int second);

/** It generates the prefix of the tuple for the given channel and
    returns the prefix in the given tuple_prefix memory. The prefix
    format is "proxy.type+iface_no" eg. "proxy.xbee1" when
    channel->iface_no=1 and channel->type="xbee". */
int proxy_getChannelTuplePrefix(ProxyChannel *channel, char *tuple_prefix);

/** Function for debugging */
void proxy_printChannel(ProxyChannel *channel);

/** This function reads the tuple from.  */
void proxy_indirectInfo(char *tuplename, ProxyChannel *channel, void *userdata, PeisTupleCallback *fn);


/** @} */
/* This ends the doxygen group PeisProxy */

#endif
