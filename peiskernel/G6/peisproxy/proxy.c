#include <stdio.h>
#include <peiskernel/peiskernel_mt.h>
#include <peiskernel/hashtable.h>
#include <stdlib.h>
#include <string.h>
#include "peisproxy.h"
#include "peisproxy_private.h"
#include "peisproxy_sublist.h"

/***********Debugging purposes*****/
extern int debug;
extern int subdebug;
extern int ifacedebug;
/********Global variables used in this file.**********/
extern PeisHashTable *proxy_sub_list;
extern ProxyTracer *proxytracer;


/************functions*************************/
/** This is an internal function for the PeisProxy structure. It
    regenerates the tuple prefix from the given interface
    information. **/

int getInterfaceTuplePrefixFromTracerIfaceInfo(TracerIfaceInfo *ifaceInfo, char *tuple_prefix){
  char temp_prefix[128];
  int size=-1;
  
  if(strlen(ifaceInfo->type)==0) {
    return 1000;
  }

  sprintf(temp_prefix,"proxy.%s%d",ifaceInfo->type,ifaceInfo->iface_no);
  size=sprintf(tuple_prefix,"%s",temp_prefix);
  if( (size>0) && (strlen(temp_prefix)==size)) 
    return 0;
  else 
    return size;
}

void updateProxyLastSensedTime(){
  int t;
  char str[50];

  t=peiskmt_gettime();
  sprintf(str,"%d",t);
  peiskmt_setStringTuple("proxy.last-sensed-time",str);
  return;
}


int proxy_countTotalInterfaces(ProxyTracer *tracer, ProxyInterface *iface){
  int found=0;
  TracerIfaceInfo **iterator=&tracer->ifaceInfo;

  if(*iterator==NULL){
    found=0;
    tracer->ifaceInfo=(TracerIfaceInfo*) malloc(sizeof(TracerIfaceInfo));
    tracer->ifaceInfo->iface_no=iface->iface_no;
    sprintf(tracer->ifaceInfo->type,"%s",iface->type);
    tracer->ifaceInfo->next=NULL;
    tracer->totalIface=1;
    return tracer->totalIface;
  }else{
    for(;*iterator!=NULL; iterator=&(*iterator)->next){
      if((*iterator)->iface_no==iface->iface_no){
	found=1;
	break;
      }else found=0;
    }
  }
  
  if(found==0){
    /** add to the list and increase the number of interface.**/
    TracerIfaceInfo *temp;

    temp=(TracerIfaceInfo*) malloc (sizeof(TracerIfaceInfo));
    temp->next=NULL;
    temp->iface_no=iface->iface_no;
    *iterator=temp;
    tracer->totalIface++;
  }
  
  return tracer->totalIface;
}

void proxy_setProxyStatus(ProxyTracer *tracer, ProxyInterface *iface){
  int now;
  char strValue[33];
  int statusFlag=0;

  if (iface->status) tracer->activeIface++;
  else tracer->activeIface--;

  if((tracer->activeIface > 0) ){
    updateProxyLastSensedTime();
    if(*(tracer->proxy_status)==1) return;
  }

  now=peiskmt_gettime();
  sprintf(strValue,"%d",now);
  if(now-atoi(tracer->proxy->last_sensed_time) > tracer->proxy->my_inactivation_time){
    statusFlag=0; //inactivate
  }else{
    statusFlag=1; //activate
  }

  if((statusFlag==0) && (*(tracer->proxy_status)!=0) ){
    peiskmt_setStringTuple("proxy.status","0");
  }else if((statusFlag==1) && (*(tracer->proxy_status)!=1)){
    peiskmt_setStringTuple("proxy.status","1");
  }

  /*previously working code.*/
/*   if((tracer->activeIface <= 0) && (*(tracer->proxy_status)!=0) ){ */
/*     peiskmt_setStringTuple("proxy.status","0"); */
/*   }else if((tracer->activeIface > 0) && (*(tracer->proxy_status)!=1)){ */
/*     peiskmt_setStringTuple("proxy.status","1"); */
/*   } */

}

void proxy_proxyStatus(ProxyTracer *tracer, ProxyInterface *iface){
  int iface_count;

  if(debug) printf("iface_no=%d status=%d\n",iface->iface_no, iface->status);

  iface_count=proxy_countTotalInterfaces(tracer, iface);

  if((tracer->activeIface>tracer->totalIface)||(tracer->activeIface <0)){
    tracer->activeIface=0;
  }

  proxy_setProxyStatus(tracer,iface);

  if (debug)  printf("total_iface=%d active_iface=%d\n",tracer->totalIface,tracer->activeIface);

  return;
}



void proxy_activateAllInterfaces(ProxyTracer *tracer){
  char key[256];
  char prefix[256];
  TracerIfaceInfo *iterator=tracer->ifaceInfo;

  while(iterator!=NULL){
    getInterfaceTuplePrefixFromTracerIfaceInfo(iterator,prefix);
    sprintf(key,"%s.status",prefix);
    peiskmt_setStringTuple(key,"1");
    iterator=iterator->next;
  }
}


void initProxytracer(ProxyTracer *tracer, PeisProxy *proxy){
  proxytracer=(ProxyTracer*)malloc(sizeof(ProxyTracer));
  proxytracer->proxy=proxy;
  proxytracer->proxy_status=&proxy->status;
  proxytracer->ifaceInfo=NULL;
  proxytracer->totalIface=-1;
  proxytracer->activeIface=-1;
  return;
}

void proxyTuplesCallback(PeisTuple *tuple, void *userdata){
  char key[256];
  PeisProxy *proxy=(PeisProxy*)userdata;
  
  if(peiskmt_getTupleName(tuple,key,sizeof(key))) {
    printf("error getting name of tuple\n");
    return;
  }

  if(strcmp(key,"proxy.unique-id")==0){
    printf("proxy.signature=%s\n",proxy->signature);
    sprintf(proxy->signature,"%s",tuple->data);
  }else if(strcmp(key,"proxy.last-sensed-time")==0){
    sprintf(proxy->last_sensed_time,"%s",tuple->data);
  }else if(strcmp(key,"proxy.inactivation-duration")==0){
    proxy->my_inactivation_time=atoi(tuple->data);
  }else{
    /** do nothing.**/
  }
  return;
}


void proxyStatusCallback(PeisTuple *tuple, void *userdata){
  ProxyTracer *tracer=(ProxyTracer *)userdata;
  
  if(atoi(tuple->data)==1){
    tracer->proxy->status=1;
    proxy_activateAllInterfaces(tracer);
    updateProxyLastSensedTime();
  }else if(atoi(tuple->data)==0){
    tracer->proxy->status=0;
    peiskmt_setStringTuple("proxy.last-sensed-time","not observed anymore");
    /** TODO: Think whether we should do it or not.**/
    //proxy_unsubscribeAll(tracer);
  }else{
  }
}

/************************** User accessable functions ***********/

/** initialize the PeisProxy structure used for this proxy. */
void proxy_init(PeisProxy *proxy){

  initProxytracer(proxytracer,proxy);
  proxy_sub_list = peisk_hashTable_create(PeisHashTableKey_String);
  
  peiskmt_setStringTuple("proxy.a-prior","Sensor:Figaro, Name:TGS 2600");
  peiskmt_setStringTuple("proxy.myjob","Get readings from Figaro gass sensor,analyze and publish as a tuple.");

  peiskmt_registerTupleCallback("proxy.status",peiskmt_peisid(),(void*)proxytracer,&proxyStatusCallback);
  peiskmt_setStringTuple("proxy.status","1");

  peiskmt_registerTupleCallback("proxy.inactivation-duration",
				peiskmt_peisid(),(void*)proxytracer,&proxyTuplesCallback);
  peiskmt_setStringTuple("proxy.inactivation-duration","10");

  peiskmt_registerTupleCallback("proxy.last-sensed-time",peiskmt_peisid(),(void*)proxy,&proxyTuplesCallback);
  //peiskmt_setStringTuple("proxy.last-sensed-time",PROXY_UNKNOWN);
  updateProxyLastSensedTime();


  //proxy->signature= (char *) malloc(sizeof("not assigned yet")+1);
  sprintf(proxy->signature,"%s","not assigned yet");
  peiskmt_registerTupleCallback("proxy.unique-id",peiskmt_peisid(),(void*)proxy,&proxyTuplesCallback);
  peiskmt_setStringTuple("proxy.unique-id", proxy->signature);

  return;
}

/** returns the status of the proxy.*/
int proxy_getProxyStatus(PeisProxy *proxy){
  return proxy->status;
}

/** usages for the peisproxy */

void proxy_usages(int argc, char **args){
  int i;
  debug=0;
  subdebug=0;
  ifacedebug=0;

  for (i=0;i<argc;i++){
    if(strcmp(args[i],"--ifacedebug")==0){
      ifacedebug=1;
    }else if(strcmp(args[i],"--subdebug")==0){
      subdebug=1;
    }else if(strcmp(args[i],"--debug")==0){
      debug=1;
    }else if(strcmp(args[i],"--help")==0){
      fprintf(stdout,"--debug: for running the libpeisproxy with debug printout. This option debugs proxy.c file mainly. See the followings for different purposes.\n");
      fprintf(stdout,"--subdebug: debugging su/unsubscription related i.e., sublist.c file.\n");
      fprintf(stdout,"--ifacedebug: interface related debugging i.e., interface.c file.\n");
    }
  }
  return;
}
