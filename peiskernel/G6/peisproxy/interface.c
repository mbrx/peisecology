#include <stdio.h>
#include <peiskernel/peiskernel_mt.h>
#include <peiskernel/hashtable.h>
#include <stdlib.h>
#include <string.h>
#include "peisproxy.h"
#include "peisproxy_sublist.h"
#include "peisproxy_private.h"

extern ProxyTracer *proxytrcer;
extern int debug;
extern int ifacedebug;

/*************************************/
/** Returns zero on success. Otherwise the number of bytes are copied.**/
int proxy_getInterfaceTuplePrefix(ProxyInterface *iface, char *tuple_prefix){
  char temp_prefix[128];
  int size=-1;

  if (strlen(iface->type)<=0) {
    return 1000;
  }

  sprintf(temp_prefix,"proxy.%s%d",iface->type,iface->iface_no);
  size=sprintf(tuple_prefix,"%s",temp_prefix);
  if( (size>0) && (strlen(temp_prefix)==size)) 
    return 0;
  else 
    return size;
}

void proxy_printInterface(ProxyInterface *iface){
  char prefix[56];

  proxy_getInterfaceTuplePrefix(iface, prefix);
  printf("%s.status=%d ",prefix,iface->status);
  printf("observer.RSSI:%d.%d ",iface->observer,iface->RSSI);
  printf("signature-data:%s ",iface->signature_data);
  printf("\n");
}


/*********** Init functions for interface ***************/
void proxy_publishInterfaceAsTuples(ProxyInterface *iface){
  char prefix[100];
  char key[128];
  char value[100];
  
  
  if(proxy_getInterfaceTuplePrefix(iface,prefix)){
    printf("Error:Tupleprefix cannot be generated. Not enough memory.\n ");
    return;
  }

  sprintf(key,"%s.type",prefix);
  peiskmt_setStringTuple(key,iface->type);

  sprintf(key,"%s.unique-id",prefix);
  peiskmt_setStringTuple(key,iface->unique_id);

  sprintf(key,"%s.signature-data",prefix);
  peiskmt_setStringTuple(key,"");

  sprintf(key,"%s.observer",prefix);
  sprintf(value,"%d",iface->observer);
  peiskmt_setStringTuple(key,value);

  sprintf(key,"%s.RSSI",prefix);
  sprintf(value,"%d",iface->RSSI);
  peiskmt_setStringTuple(key,value);

  sprintf(key,"%s.last-sensed-time",prefix);
  peiskmt_setStringTuple(key,iface->last_sensed_time);

  sprintf(key,"%s.inactivation-duration",prefix);
  sprintf(value,"%d",iface->my_inactivation_time);
  peiskmt_setStringTuple(key,value);

  sprintf(key,"%s.status",prefix);
  sprintf(value,"%d",iface->status);
  peiskmt_setStringTuple(key,value);
  
  sprintf(key,"%s.used-nsensors",prefix);
  sprintf(value,"%d",iface->used_sensors);
  peiskmt_setStringTuple(key,value);

}

void proxy_interfaceInitAbstract(ProxyInterface *iface){

  iface->iface_no=0;
  sprintf(iface->type,"%s",PROXY_UNKNOWN);
  sprintf(iface->unique_id,"%s",PROXY_UNKNOWN);
  iface->signature_data=NULL;
  iface->observer=-1;
  iface->RSSI=1000;
  sprintf(iface->last_sensed_time,"%s",PROXY_UNKNOWN);
  iface->my_inactivation_time=PEISPROXY_INACTIVATE_IFACE;
  iface->status=1;
  iface->used_sensors=1;

  /** user define functions.**/
  iface->datatranslator=NULL;
  iface->fnArbiter=NULL;

  /** meta information.**/
  sprintf(iface->metaInfo.oldSignature,"%s.%s",iface->type,iface->unique_id);
  iface->metaInfo.oldObserverData=-1;
  iface->metaInfo.oldObserverIface=-100;
}

void ifaceTypeCallback(PeisTuple *tuple, void *userdata){
  ProxyInterface *iface=(ProxyInterface *)userdata;
  proxy_unsubscribeAllForIface(iface);
  sprintf(iface->type,"%s",(char*)tuple->data);
}

void ifaceUniqueIdCallback(PeisTuple *tuple, void *userdata){
  ProxyInterface *iface=(ProxyInterface *)userdata;
  proxy_unsubscribeAllForIface(iface);
  sprintf(iface->unique_id,"%s",(char*)tuple->data);
}

void ifaceSignatureDataCallback(PeisTuple *tuple, void *userdata){
  ProxyInterface *iface=(ProxyInterface *)userdata;
  
  iface->signature_data=(char *)realloc((void*)iface->signature_data,tuple->datalen+1);
  sprintf(iface->signature_data,"%s",tuple->data);
}

void ifaceUsedSensorsCallback(PeisTuple *tuple, void *userdata){
  ProxyInterface *iface=(ProxyInterface *)userdata;
  iface->used_sensors=atoi(tuple->data);
}

void ifaceObserverCallback(PeisTuple *tuple, void *userdata){
  char prefix[256];
  char str[256];
  ProxyInterface *iface=(ProxyInterface *)userdata;
  
  proxy_getInterfaceTuplePrefix(iface,prefix);

  iface->observer=atoi(tuple->data);
  if(iface->observer==-1) {
    sprintf(str,"%s.RSSI",prefix);
    peiskmt_setStringTuple(str,"1000");
  }
}

void ifaceRSSICallback(PeisTuple *tuple, void *userdata){
  ProxyInterface *iface=(ProxyInterface *)userdata;
  iface->RSSI=atoi(tuple->data);
}

void ifaceStatusCallback(PeisTuple *tuple, void *userdata){
  ProxyInterface *iface=(ProxyInterface *)userdata;
  iface->status=atoi(tuple->data);
}

void ifaceInactivationTimeCallback(PeisTuple *tuple, void *userdata){
  ProxyInterface *iface=(ProxyInterface *)userdata;
  iface->my_inactivation_time=atoi(tuple->data);
}

void ifaceLastSensedTimeCallback(PeisTuple *tuple, void *userdata){
  char strTemp[128];
  char prefix[128];
  ProxyInterface *iface=(ProxyInterface *)userdata;
  
  sprintf(iface->last_sensed_time,"%s",tuple->data);
  if(strcmp(iface->last_sensed_time,"under observation")==0){
    proxy_getInterfaceTuplePrefix(iface,prefix);
    sprintf(strTemp,"%s.status",prefix);
    peiskmt_setStringTuple(strTemp,"1");
  }
  
}

/** This function sets callbacks for few basic tuples of each interface. The tuples are **/
void setInitCallbacks(ProxyInterface *iface){
  char prefix[64];
  char key[128];

  if(proxy_getInterfaceTuplePrefix(iface,prefix)){
    printf("setInitCallbacks Error:Tupleprefix generation error. Not enough memory.\n ");
    exit(0);
  }

  sprintf(key,"%s.type",prefix);
  peiskmt_registerTupleCallback(key,peiskmt_peisid(),(void*)iface,&ifaceTypeCallback);

  sprintf(key,"%s.unique-id",prefix);
  peiskmt_registerTupleCallback(key,peiskmt_peisid(),(void*)iface,&ifaceUniqueIdCallback);

  sprintf(key,"%s.signature-data",prefix);
  peiskmt_registerTupleCallback(key,peiskmt_peisid(),(void*)iface,&ifaceSignatureDataCallback);

  sprintf(key,"%s.observer",prefix);
  peiskmt_registerTupleCallback(key,peiskmt_peisid(),(void*)iface,&ifaceObserverCallback);

  sprintf(key,"%s.RSSI",prefix);
  peiskmt_registerTupleCallback(key,peiskmt_peisid(),(void*)iface,&ifaceRSSICallback);

  sprintf(key,"%s.status",prefix);
  peiskmt_registerTupleCallback(key,peiskmt_peisid(),(void*)iface,&ifaceStatusCallback);

  sprintf(key,"%s.inactivation-duration",prefix);
  peiskmt_registerTupleCallback(key,peiskmt_peisid(),(void*)iface,&ifaceInactivationTimeCallback);

  sprintf(key,"%s.last-sensed-time",prefix);
  peiskmt_registerTupleCallback(key,peiskmt_peisid(),(void*)iface,&ifaceLastSensedTimeCallback);

  sprintf(key,"%s.used-sensors",prefix);
  peiskmt_registerTupleCallback(key,peiskmt_peisid(),(void*)iface,&ifaceUsedSensorsCallback);

}

void proxy_interfaceInit(ProxyInterface *iface, ProxyInterfaceInit *fn){
  
  if (iface==NULL) {
    fprintf(stdout,"iface:no interface is assigned.\n");
    return;
  }
  proxy_interfaceInitAbstract(iface);
  if(fn!=NULL) fn((void *)iface);
  else {
    printf("Error: no user define init function has provided.\n");
    exit(0);
  }
  setInitCallbacks(iface);
  proxy_publishInterfaceAsTuples(iface);

  return;
}

/*********** Interface Implementation functions *****/
int subscribeForInterfaces(ProxyInterface *iface){
  char strTemp[256];
  char value[10];
  char hashkey[PEISPROXY_HASH_KEY_SIZE];
  ProxySubInfo handle;
  char prefix[128];

  sprintf(strTemp,"%s.%s",iface->type,iface->unique_id);

  if(debug || ifacedebug){
    printf("forIface: oldkey=%s newkey=%s\n",iface->metaInfo.oldSignature,strTemp);
  }

  if( (strcmp(strTemp,iface->metaInfo.oldSignature)!=0) 
      || (iface->observer!=iface->metaInfo.oldObserverIface)){

    proxy_generateHashKey(hashkey,iface->metaInfo.oldSignature,iface);
    proxy_unsubscribeTuple(iface->metaInfo.oldSignature,hashkey);

    handle.subHandle=peiskmt_subscribe(strTemp,iface->observer);
    handle.callbackHandle=peiskmt_registerTupleCallback(strTemp,iface->observer,(void *)iface,iface->fnArbiter);
    if (ifacedebug)
      printf("forIface: new subscription for interfacce: oldkey=%s newkey=%s observer(old->new):%d->%d \n",
	     iface->metaInfo.oldSignature,strTemp,iface->metaInfo.oldObserverIface,iface->observer);

    /** Save metaInfo and Sub info.**/
    proxy_generateHashKey(hashkey,strTemp,iface);
    proxy_saveSubInfo(hashkey,&handle,iface);
    sprintf(iface->metaInfo.oldSignature,"%s",strTemp);
    iface->metaInfo.oldObserverIface=iface->observer;

    /** make the observer -1. **/
    proxy_getInterfaceTuplePrefix(iface,prefix);
    sprintf(strTemp,"%s.observer",prefix);
    sprintf(value,"%d",iface->observer);
    peiskmt_setStringTuple(strTemp,value);
  }
  
  return PEISPROXY_SUCCESS;
}


void proxiedDataCallback(PeisTuple *tuple, void *userdata){
  char str[33];
  char key[256];
  //double t0=peiskmt_gettimef();
  int t[2];
  int flag=0;
  ProxyInterface *iface=(ProxyInterface *)userdata;

  if(peisk_getTupleName(tuple,key,sizeof(key))) 
    fprintf(stderr,"error getting name of tuple\n");

  if (tuple->owner!=iface->observer){
    fprintf(stdout,"%s:incorrect interface. owner=%d observer=%d \n",
	    key,tuple->owner,iface->observer);
    return;
  }

  /** TODO: what happen when the interface disappears/out of range from the ecology.*/
  peiskmt_gettime2(&t[0],&t[1]);
  sprintf(str,"%d",t[0]);

  if((tuple->ts_expire[0]+1e-6*tuple->ts_expire[1])){ //when the tuple is deleted.
    flag=0;
  }else{
    flag=1;
    //sprintf(str,"%s","under observation");
  }
  
  proxy_getInterfaceTuplePrefix(iface,key);
  sprintf(key,"%s.last-sensed-time",key);
  peiskmt_setStringTuple(key,str);

  /** translate the data if the hardware is under observation. **/
  if(flag) iface->datatranslator(tuple,(void*)iface);

  return;
}

int subscribeForProxiedData(ProxyInterface *iface){
  char tuplename[128];
  char strKey[256];
  char hashkey[PEISPROXY_HASH_KEY_SIZE];
  ProxySubInfo handle;


  sprintf(strKey,"%s.%s",iface->type,iface->unique_id);

  if(debug || ifacedebug){
    printf("subForProxiedData: oldkey->newkey:%s->%s observer(old->new):%d->%d \n",
	   iface->metaInfo.oldSignature,strKey,iface->metaInfo.oldObserverData,iface->observer);
  }

  if (
      (strcmp(strKey,iface->metaInfo.oldSignature)!=0) || (iface->observer!=iface->metaInfo.oldObserverData) 
      ){
    
    /**Unsubscribe the previously subscribed tuples. **/
    sprintf(tuplename,"%s.%d.data",iface->metaInfo.oldSignature,iface->metaInfo.oldObserverData);
    proxy_generateHashKey(hashkey,tuplename,iface);

    if(debug||ifacedebug) 
      printf("subForDtat: unsubscribe %s %s\n",tuplename,hashkey);
    proxy_unsubscribeTuple(tuplename,hashkey);

    /** Save the old state. It does not matter the observer exists or
	not i.e., iface->observer=-1 or anything else.**/
    sprintf(iface->metaInfo.oldSignature,"%s",strKey);
    iface->metaInfo.oldObserverData=iface->observer;
    
    /** Subscribe only when we know the observer in the vicinity for
	the signature data. **/ 
    if(iface->observer!=-1){
      if (debug || ifacedebug){
	printf("subscribe for proxied-data: \n");
	proxy_printInterface(iface);
      }

      handle.subHandle=peiskmt_subscribe(strKey,iface->observer);
      handle.callbackHandle=peiskmt_registerTupleCallback(strKey,iface->observer,
							  (void*)iface,&proxiedDataCallback);
      /** save the handles for unsubscribe later.**/
      sprintf(tuplename,"%s.%d.data",strKey,iface->observer);
      proxy_generateHashKey(hashkey,tuplename,iface);
      proxy_saveSubInfo(hashkey,&handle,iface);
    }
    
  }
  return PEISPROXY_SUCCESS;
}

void proxy_setInterfaceStatus(ProxyInterface *iface){
  int now;
  char prefix[32];
  char str[128];

  proxy_getInterfaceTuplePrefix(iface,prefix);

  now=peiskmt_gettime();
  if (now-atol(iface->last_sensed_time)>iface->my_inactivation_time){
    if (iface->status!=0){
      
      sprintf(str,"%s.status",prefix);
      peiskmt_setStringTuple(str,"0");
      
      sprintf(str,"%s.observer",prefix);
      peiskmt_setStringTuple(str,"-1");
      
      sprintf(str,"%s.last-sensed-time",prefix);
      peiskmt_setStringTuple(str,"not observed anymore");
    }
  }else{
    if (iface->status!=1){
      sprintf(str,"%s.status",prefix);
      peiskmt_setStringTuple(str,"1");
    }
  }
    
}

/**************************/
int proxy_implementInterface(ProxyInterface *iface){
  int ret;
  
  /* TODO: Think about return values.**/

  
  if(strcmp(iface->unique_id,PROXY_UNKNOWN)==0){
    printf("channel=%d unique-id=%s\n",iface->iface_no,iface->unique_id);
    return PEISPROXY_CONTINUE;
  }
  
  ret=subscribeForInterfaces(iface);
  //if(iface->observer!=-1) 
  subscribeForProxiedData(iface); 

  proxy_setInterfaceStatus(iface);

  proxy_proxyStatus(proxytracer, iface);

  return ret;
}

/************************/
void proxy_indirectInfo(char *tuplename, ProxyChannel *channel, void *userdata, PeisTupleCallback *fn){
  char prefix[PEISPROXY_PREFIX_SIZE];
  char hashkey[PEISPROXY_HASH_KEY_SIZE];
  char str[256];
  int now;
  ProxySubInfo handle;
  ProxyInterface *iface=(ProxyInterface *)channel;
  
  if((iface->status!=1) || (strcmp(iface->unique_id,PROXY_UNKNOWN)==0) ) return;

  /** Reset the indirect tuple if we dont receive any direct info more
      than this time. Remember that the chennel might be active.

      TODO: Modify this part with ProxyIndirectInfo structure after
      discussing with Mathias and Alessandro, if we all agree.
  */
  now=peiskmt_gettime();
  if(now-atoi(iface->last_sensed_time)>5){
    proxy_getInterfaceTuplePrefix(iface,prefix);
    sprintf(str,"%s.%s",prefix,tuplename);
    peiskmt_setStringTuple(str,"unknown");
  }

  proxy_generateHashKey(hashkey,tuplename,iface);
  if(iface->observer < 0){
    proxy_unsubscribeTuple(tuplename,hashkey);
    return;
  }

  /** Skip subscriptions if you are already subscribed **/
  if(proxy_getSubInfoFromList(hashkey)==NULL){ 
    if(ifacedebug || debug) printf("indirectInfo: %s new request to subscribe.\n",hashkey);
    handle.subHandle=peiskmt_subscribe(tuplename,iface->observer);
    handle.callbackHandle=peiskmt_registerTupleCallback(tuplename,iface->observer,userdata,fn);
    proxy_saveSubInfo(hashkey,&handle,iface);
  } 

}

void proxy_setInterfaceInactivationTime(ProxyInterface *iface, int second){
  iface->my_inactivation_time=second;
  return;
}


/*****Wrapper functions *******/
int proxy_implementChannel(ProxyChannel *channel){
  int ret;
  ret=proxy_implementInterface((ProxyInterface *) channel);
  return ret;
}


void proxy_channelInit(ProxyChannel *channel, ProxyChannelInit *fnInit){
  proxy_interfaceInit((ProxyInterface*) channel, fnInit);
}

void proxy_publishChannelAsTuples(ProxyChannel *channel){
  proxy_publishInterfaceAsTuples((ProxyInterface *)channel);
}

/** This function sets the inactivation time of the given
    channel. Time is in second. */
void proxy_setChannelInactivationTime(ProxyChannel *channel, int second){
  proxy_setInterfaceInactivationTime((ProxyInterface *)channel, second);
}

/** Function for debugging */
void proxy_printChannel(ProxyChannel *channel){
  proxy_printInterface((ProxyInterface *)channel);
}

int proxy_getChannelTuplePrefix(ProxyChannel *channel, char *tuple_prefix){
  return proxy_getInterfaceTuplePrefix((ProxyInterface*)channel,tuple_prefix);
}
