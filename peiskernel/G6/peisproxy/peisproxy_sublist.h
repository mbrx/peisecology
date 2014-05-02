#ifndef PEISPROXY_SUBLIST_H
#define PEISPROXY_SUBLIST_H

#include <peiskernel/peiskernel_mt.h>
#include <peiskernel/hashtable.h>
#include "peisproxy.h"

 
/*****************************************************
** Global variables for internal use.
**
*****************************************************/

#define PEISPROXY_HASH_KEY_SIZE 256


/** This list contains all subscription and callback registration
    handlers. Note the key for the hashtable is 
    tuplename.iface->iface_no.iface->type.iface->uid. */
PeisHashTable *proxy_sub_list;


/** This structure for saving all subscription and callback
    registrations have beed done for the implementation. All of them
    are saved in a hashtable called proxy_sub_list.*/
typedef struct ProxySubInfo{
  int iface_no;
  PeisSubscriberHandle subHandle;
  PeisCallbackHandle callbackHandle;
}ProxySubInfo;


/******************************************************/
/*                                                    */
/* Function definitions.                              */       
/*                                                    */    
/******************************************************/

/** Generate the key for hashtable entry from the given tuple name
    and some fields of provided interface. Return zero on
    success. The key format:
    tuplename.iface->iface_no.iface->type.iface->uid  */
int proxy_generateHashKey(char *key, char *tuplename, ProxyInterface *iface);

/** memory allocation for the hashtable value is done here.*/
int proxy_saveSubInfo(char *key, ProxySubInfo *handle, ProxyInterface *iface);
/** Get the value of the given hash key. Note: the key-value pair is
    not removed from the hashtable when this operation is done. For
    removing the key-value proxy_deleteSubInfo should be called. */ 
ProxySubInfo *proxy_getSubInfoFromList(char *key);

/** It does free the allocated memory for the hashtable value. */
int proxy_deleteSubInfo(char *key);

/** It unsubscribes from the subscribed tuple as well as does free
    the allocated memory for value.*/
void proxy_unsubscribeTuple(char *tuplename, char *hashkey);


void proxy_unsubscribeAllForIface(ProxyInterface *iface);

/** Deletes all existing keys and values from the sub_list. */
void proxy_unsubscribeAll();



#endif 
