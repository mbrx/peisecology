#ifndef PEISPROXY_PRIVATE_H
#define PEISPROXY_PRIVATE_H

#include <peiskernel/peiskernel_mt.h>
#include <peiskernel/hashtable.h>
#include "peisproxy.h"

/** \ingroup PeisProxy
    @{
*/

/*****************************************************
** Global variables for internal use.
**
*****************************************************/

#define PROXY_UNKNOWN "unknown"


/** Information about interfaces used in the proxy. This is metainfo
    for the ProxyTracer.*/
typedef struct TracerIfaceInfo{
  int iface_no;
  char type[PEISPROXY_CHANNEL_NAME_SIZE];
  struct TracerIfaceInfo *next;
}TracerIfaceInfo;

/** This structure currently keeps track of the proxy's status. All
    other info such as what tuples are used for this proxy etc also
    can be used if someone wants to. But this implementation they are
    not implemented. */
typedef struct ProxyTracer{
  PeisProxy *proxy; ///< the proxy it is working for.
  int *proxy_status; ///< pointer for the status of the proxy.
  TracerIfaceInfo *ifaceInfo; ///< List of all used interfaces
  int totalIface; ///< keeps track of how many interfaces are used for
		  ///this proxy component.
  int activeIface; ///< how many interface's status are active right
		   ///now. Based on these two values the status of the proxy is determined.
  char *tupleslist;
}ProxyTracer;


/** It holds the information of instantiated proxy component. The
    memory for this tracer is allocated in the proxy_init(proxy *) function.*/
ProxyTracer *proxytracer;

/******************************************************/
/*                                                    */
/* Function definitions.                              */       
/*                                                    */    
/******************************************************/

/** It initializes the interface structure with default values.*/
void proxy_interfaceInitAbstract(ProxyInterface *iface);

/** sets all the callback functions for the tuples of the given interface.*/
void setInitCallbacks(ProxyInterface *iface);

/** Does a wildcard subscription for all available interfaces that are
    observing the signature right now. The arbitration callback
    function works on the response of it. Note that this function is part of
    the proxy-proxied-concept. */
int subscribeForInterfaces(ProxyInterface *iface);

/** The hardware(signature) data should be received from only one
    interface, according to the concept, even though more than one
    interfaces observe the signature. Therefore subscription for the signature data should
    be done with the arbitrated interface's peis-id and the
    previous/other subscription should be unsubscribed. This is done
    in this function. First unsubscribes all the exisiting
    subscription for the signature data and the subscribe only to the
    most relevent interface. */
int subscribeForProxiedData(ProxyInterface *iface);

/** This function counts how many interfaces are used in this proxy
    i.e., the number of channels are attached to proxied the object.*/
int proxy_countTotalInterfaces(ProxyTracer *tracer, ProxyInterface *iface);

/** Assign the status of the proxy based on ProxyTracer's total and
    active interfaces.*/
void proxy_setProxyStatus(ProxyTracer *tracer, ProxyInterface *iface);

/** Determine the status of the proxy whether active or inactive based
    on the status of the used interfaces.
*/
void proxy_proxyStatus(ProxyTracer *tracer, ProxyInterface *iface);

/** Set the status=1(active) of all available interfaces in this proxy.*/
void proxy_activateAllInterfaces(ProxyTracer *tracer);


/******************************************/
/** Takes the user function for initializing the interface
    structure. In the user function, the (void *userdata) should be
    casted as ProxyInterfce before using it. */
void proxy_interfaceInit(ProxyInterface *iface, ProxyInterfaceInit *fnInit);


/** Publishes the fields of structure as tuples with present values.*/
void proxy_publishInterfaceAsTuples(ProxyInterface *iface);

/** Time is in second.*/
void proxy_setInterfaceInactivationTime(ProxyInterface *iface, int second);

/** This is the core function for the proxy mechanism.*/
int proxy_implementInterface(ProxyInterface *iface);

/** Generate the tupleprefix for publishing interface's info as
    tuples. Return zero on sucess otherwise non-zero. */
int proxy_getInterfaceTuplePrefix(ProxyInterface *iface, char *tuple_prefix);


/** Function for debugging */
void proxy_printInterface(ProxyInterface *iface);



/** @} */
/* This ends the doxygen group PeisProxy */

#endif 
