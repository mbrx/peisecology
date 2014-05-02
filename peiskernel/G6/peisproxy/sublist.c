#include <stdio.h>
#include <peiskernel/peiskernel_mt.h>
#include <peiskernel/hashtable.h>
#include <stdlib.h>
#include <string.h>
#include "peisproxy.h"
#include "peisproxy_sublist.h"

#define PEISPROXY_HASH_KEY_SIZE 256

/*******for debugging*************************/
extern int debug;
extern int subdebug;

/**********Global variables*******************/

extern PeisHashTable *proxy_sub_list;


/*************************************/
void printHashTable(PeisHashTable *hashtable){
  PeisHashTableIterator iter;
  char *key;
  ProxySubInfo *handle;
  
  /* Iterate over all SUBSCRIBERS */
  for(peisk_hashTableIterator_first(hashtable,&iter);
      peisk_hashTableIterator_next(&iter);)
    if(peisk_hashTableIterator_value(&iter, (void**)(void*) &key, (void**) (void*) &handle)) {
      printf("hashkey: %s %d(sub) %d(cb)\n",key,handle->subHandle,handle->callbackHandle);
    }
  
}

int proxy_generateHashKey(char *key, char *tuplename, ProxyInterface *iface){
  char ifaceno[32];
  int keysize;

  sprintf(ifaceno,"%d",iface->iface_no);
  keysize=strlen(tuplename)+strlen(ifaceno)+strlen(iface->type)+strlen(iface->unique_id)+3;
  if(keysize>=PEISPROXY_HASH_KEY_SIZE){ 
    key[0]='\0';
    return keysize;
  }
  sprintf(key,"%s*%s%d*%s",
	  tuplename,iface->type,iface->iface_no,iface->unique_id);
  
  return 0;
  
}

int proxy_saveSubInfo(char *key, ProxySubInfo *handle, ProxyInterface *iface){
  int ret;
  ProxySubInfo *item=(ProxySubInfo*)malloc(sizeof(ProxySubInfo));
  item->iface_no=iface->iface_no;
  item->subHandle=handle->subHandle;
  item->callbackHandle=handle->callbackHandle;

  ret=peisk_hashTable_insert(proxy_sub_list,key,(void*)item);

  if(subdebug){
    printf("inserted %s:%d(sub) %d(cb)\n",key,handle->subHandle,handle->callbackHandle);
  }

  return ret;
}


ProxySubInfo *proxy_getSubInfoFromList(char *key){
  ProxySubInfo *handle;
  int ret;
  
  ret=peisk_hashTable_getValue(proxy_sub_list,(void*)key, (void**)(void*)&handle);
  if (ret!=0) {
    if(subdebug) printf("sublist.c->getSubInfo:erro %d: %s is not available in the hashtable.\n",ret,key);
    return NULL;
  }

  if(subdebug){
    printf("retrived %s:%d(sub) %d(cb)\n",key,handle->subHandle,handle->callbackHandle);
  }
  return handle;
}



int proxy_deleteSubInfo(char *key){
  ProxySubInfo *value;
  int ret;
  
  ret=peisk_hashTable_getValue(proxy_sub_list,(void*)key, (void**)(void*)&value);
  if (ret!=0){
    if(subdebug) printf("deleteSubInfo: %s not available in the sublist.\n",key);
    return ret;
  }

  ret=peisk_hashTable_remove(proxy_sub_list, (void*)key);
  if (ret!=0) { 
    if (subdebug)  printf("error %d: %s cannot be removed from hashtable.\n",ret,key);
    return ret;
  }

  free(value);
  return 0;
}

void proxy_unsubscribeTuple(char *tuplename, char *hashkey){
  ProxySubInfo *handle;
  /** delete all subscription that matches with the key.**/
  if (subdebug){
    printf("hashkey to delete:%s \n",hashkey);
    printf("hashtable content: \n");
    printHashTable(proxy_sub_list);
  }

  while((handle=proxy_getSubInfoFromList(hashkey))!=NULL){
    printf("Unregistering %d\n",handle->callbackHandle);
    peiskmt_unsubscribe(handle->subHandle);
    peiskmt_unregisterTupleCallback(handle->callbackHandle);
    proxy_deleteSubInfo(hashkey);
  }

  return;
}


void proxy_unsubscribeAllForIface(ProxyInterface *iface){
  PeisHashTableIterator iter;
  char *key;
  ProxySubInfo *handle;
  int iterValue=1;

  peisk_hashTableIterator_first(proxy_sub_list,&iter);
  iterValue=peisk_hashTableIterator_next(&iter);
  for(;iterValue!=0;){
    if(peisk_hashTableIterator_value(&iter, (void**)(void*) &key, (void**) (void*) &handle)) {
      if (handle->iface_no==iface->iface_no){
	/* Unsubscribe first and then delete it delete the the handle from the list. */
	peiskmt_unsubscribe(handle->subHandle);
	peiskmt_unregisterTupleCallback(handle->callbackHandle);
	/* Free the allocated memory for the handle.*/
	free(handle);
	/* Go to the next item first and then delete the current key. */
	iterValue=peisk_hashTableIterator_next(&iter);
	peisk_hashTable_remove(proxy_sub_list,(void*)key);
      } else {	
	iterValue=peisk_hashTableIterator_next(&iter);
      }
    } else {
      printf("for some reason, hashtable has failed\n");
      iterValue=peisk_hashTableIterator_next(&iter);
    }
  }

  if (subdebug){
    printf("after deleteion all for interface=%d:\n",iface->iface_no);
    printHashTable(proxy_sub_list);
  }
  
  return;
}

void proxy_unsubscribeAll(){
  PeisHashTableIterator iter;
  char *key;
  ProxySubInfo *handle;
  int iterValue=1;

  peisk_hashTableIterator_first(proxy_sub_list,&iter);
  iterValue=peisk_hashTableIterator_next(&iter);

  for(;iterValue!=0;){
    if(peisk_hashTableIterator_value(&iter, (void**)(void*) &key, (void**) (void*) &handle)) {
      /* Unsubscribe first and then delete it delete the the handle from the list. */
      peiskmt_unsubscribe(handle->subHandle);
      peiskmt_unregisterTupleCallback(handle->callbackHandle);
      /* Free the allocated memory for the handle.*/
      free(handle);
      /* Go to the next item first and then delete the current key. */
      iterValue=peisk_hashTableIterator_next(&iter);
      peisk_hashTable_remove(proxy_sub_list,(void*)key);
    }
  }

  return;
  
}


