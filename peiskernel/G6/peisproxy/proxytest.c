/** \file proxytest.c
    An example proxy for a proxied peis that is proxied by two
    different communication channels such XBee and RFID communicatin
    channel. This program is used mainly for developing the
    libpeisproxy.
*/

/*
   Copyright (C) 2005  Mathias Broxvall

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/** \mainpage twointerfaceproxy twointerfaceproxy

  This is a doxygen page describing this project.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <peiskernel/peiskernel_mt.h>
#include <peiskernel/peiskernel.h>
#include <peisproxy/peisproxy.h>
#include <peisproxy/auxiliary.h>

/*                  */
/* Global variables */
/*                  */

/** Define all communication channels that are used for proxying the
    object. Note that initialization of these channels should be
    done in proxy_channelInit() function.*/
PeisProxy dummyProxy;
ProxyChannel xbee1, rfid1, xbee2,rfid2;


/******Function declaration******************/

void translateAndPublishXbee1DataAsTuple(PeisTuple *tuple, void *channelInfo);
void translateAndPublishRfid1DataAsTuple(PeisTuple *tuple, void *channelInfo);

void arbiterXBee1(PeisTuple *tuple, void *channelInfo);
void arbiterRfid1(PeisTuple *tuple, void *channelInfo);


/*******************/
/*                 */
/*      Code       */
/*                 */
/*******************/

void printUsage(FILE *stream,short argc,char **args) {
  fprintf(stream,"%s options:\n",args[0]);
  fprintf(stream," --help                 Print usage information\n");
  peiskmt_printUsage(stream,argc,args);
  exit(0);
}


void translateAndPublishXbee1DataAsTuple(PeisTuple *tuple, void *channelInfo){
  char key[128];
  char prefix[32];
  ProxyChannel *channel=(ProxyChannel *)channelInfo;

  proxy_getChannelTuplePrefix(channel,prefix);

  //printf("%s.data=%s\n",prefix,tuple->data);

  sprintf(key,"%s.signature-data",prefix);
  peiskmt_setStringTuple(key,tuple->data);
  return;
}

void translateAndPublishRfid1DataAsTuple(PeisTuple *tuple, void *channelInfo){
  char key[128];
  char prefix[32];
  ProxyChannel *channel=(ProxyChannel *)channelInfo;

  proxy_getChannelTuplePrefix(channel,prefix);

  //printf("%s.data=%s\n",prefix,tuple->data);

  //  sprintf(key,"proxy.iface%d.signature-data",rfid1.iface_no);
  sprintf(key,"%s.signature-data",prefix);
  peiskmt_setStringTuple(key,tuple->data);

  return;
}


void arbiterRfid1(PeisTuple *tuple, void *channelInfo){
  char prefix[32];
  char key[256];
  char value[33];
  ProxyChannel *channel=(ProxyChannel *)channelInfo;
  
  proxy_getChannelTuplePrefix(channel,prefix);

  sprintf(key,"%s.observer",prefix);
  sprintf(value,"%d",tuple->owner);
  peiskmt_setStringTuple(key,value);

  sprintf(key,"%s.RSSI",prefix);
  /** we don't need it here.**/

  return;
}

void arbiterXBee1(PeisTuple *tuple, void *channelInfo){
  char prefix[32];
  char key[256];
  char value[33];
  ProxyChannel *channel=(ProxyChannel *)channelInfo;

/*   printf("from xbee arbiter function.\n"); */
/*   proxy_printChannel(channel); */

  proxy_getChannelTuplePrefix(channel,prefix);

  sprintf(key,"%s.observer",prefix);
  sprintf(value,"%d",tuple->owner);
  peiskmt_setStringTuple(key,value);

  sprintf(key,"%s.RSSI",prefix);
  /** we don't need it here.**/

  return;
}


/* Initialize tuples for application. The tuple for proxy can override here. For example, "proxy.unique-id" has beed overridded here.*/
void myProxyTuplesInit(){
  peiskmt_setStringTuple("proxy.myposition","unknown");
  peiskmt_setStringTuple("proxy.unique-id","XBee 0013a20040089801 XBee 0013a20040089802 rfid r1 rfid r2");
  peiskmt_setStringTuple("proxy.my-activity","unknown");
}

void xbee1_channelInit(void *channelInfo){
  ProxyChannel *channel=(ProxyChannel*)channelInfo;

  channel->iface_no=1;
  sprintf(channel->type,"%s","XBee");
  proxy_setChannelInactivationTime(channel, 30);
  channel->datatranslator=&translateAndPublishXbee1DataAsTuple;
  channel->fnArbiter=&arbiterXBee1;
}

void xbee2_channelInit(void *channelInfo){
  ProxyChannel *channel=(ProxyChannel*)channelInfo;

  channel->iface_no=3;
  sprintf(channel->type,"%s","XBee");
  proxy_setChannelInactivationTime(channel, 30);
  channel->datatranslator=&translateAndPublishXbee1DataAsTuple;
  channel->fnArbiter=&arbiterXBee1;
}


void rfid1_channelInit(void *channelInfo){
  ProxyChannel *channel=(ProxyChannel*)channelInfo;

  channel->iface_no=2;
  sprintf(channel->type,"%s","rfid");
  /** Use the default inactivation time **/
  channel->datatranslator=&translateAndPublishRfid1DataAsTuple;
  channel->fnArbiter=&arbiterRfid1;
}


void rfid2_channelInit(void *channelInfo){
  ProxyChannel *channel=(ProxyChannel*)channelInfo;

  channel->iface_no=4;
  sprintf(channel->type,"%s","rfid");
  /** Use the default inactivation time **/
  channel->datatranslator=&translateAndPublishRfid1DataAsTuple;
  channel->fnArbiter=&arbiterRfid1;
}

void uniqueIdCallback(PeisTuple *tuple, void *userdata){
  ProxyChannel *channel;
  char prefix[64];
  char key[128];
  char value[100];

  if (strcmp(tuple->data,"not assigned yet")==0) return;
  
  /**TODO: parse the string and assign it to the different interface structure.**/

  /** xbee channel 1 **/
  channel=&xbee1;
  if(proxy_getChannelTuplePrefix(channel,prefix)==PEISPROXY_SUCCESS){
    sprintf(key,"%s.unique-id",prefix);
    peiskmt_setStringTuple(key,"x1");
  }


  /** xbee channel 2 **/
  channel=&xbee2;
  if(proxy_getChannelTuplePrefix(channel,prefix)==PEISPROXY_SUCCESS){
    sprintf(key,"%s.unique-id",prefix);
    peiskmt_setStringTuple(key,"x2");
  }

  /** rfid channel 1**/
  channel=&rfid1;
  if(proxy_getChannelTuplePrefix(channel,prefix)==PEISPROXY_SUCCESS){
    sprintf(key,"%s.unique-id",prefix);
    peiskmt_setStringTuple(key,"r1");
  }

  /** rfid channel 1**/
  channel=&rfid2;
  if(proxy_getChannelTuplePrefix(channel,prefix)==PEISPROXY_SUCCESS){
    sprintf(key,"%s.unique-id",prefix);
    peiskmt_setStringTuple(key,"r2");
  }

  return;
}

void indirect_myposCallback(PeisTuple *tuple, void *userdata){
  char key[256];
  char prefix[32];
  ProxyChannel *channel=(ProxyChannel *)userdata;
  
  
  proxy_getChannelTuplePrefix(channel,prefix);
  peisk_getTupleName(tuple,key,sizeof(key));

  printf("indirect_mypos: %s %d.%s=%s\n",prefix,tuple->owner,key,tuple->data);

  /** publish your local tuplespace */
  sprintf(key,"%s.myposition",prefix);
  peiskmt_setStringTuple(key,tuple->data);

  return;
}


/*************************************/

int main(int argc,char **args) {
  int len; char *value;
  int isRunning;
  char *hostname;
  int t1[2],t2[2];
  
  if(argc > 1 && strcmp(args[1],"--help") == 0) {
    printUsage(stderr,argc,args);
  }

  /* Initialize peiskernel */
  peiskmt_initialize(&argc,args);

  peiskmt_setStringTuple("name","peisproxy");
  /* See comment below */
  peiskmt_setStringTuple("do-quit","nil");


  /* setting parameter for debugging and etc. It is not complete yet.*/
  proxy_usages(argc, args);

  /* Initialization as a proxy component */
  proxy_init(&dummyProxy);
  
  proxy_channelInit(&xbee1,&xbee1_channelInit);
  proxy_channelInit(&rfid1,&rfid1_channelInit);
  proxy_channelInit(&xbee2,&xbee2_channelInit);
  proxy_channelInit(&rfid2,&rfid2_channelInit);

  /* My tuples and callbacks for this proxy.*/
  myProxyTuplesInit();
  peiskmt_registerTupleCallback("proxy.unique-id",peiskmt_peisid(),NULL,&uniqueIdCallback);
  
  isRunning=1;
  while(isRunning && peiskmt_isRunning()) {
    proxy_implementChannel(&xbee1);
    proxy_implementChannel(&rfid1);
    proxy_implementChannel(&xbee2);
    proxy_implementChannel(&rfid2);
    
    proxy_indirectInfo("myposition",&xbee1,(void *)&xbee1,&indirect_myposCallback); 
    proxy_indirectInfo("myposition",&rfid1,(void *)&rfid1,&indirect_myposCallback);

    if (proxy_getProxyStatus(&dummyProxy)){
      /** 
	  TODO: write your proxy related code here. If the proxy is
	  active only then this part of code should be executed.
       */
      
    }else{
      printf("proxy_status=%d\n",dummyProxy.status);
    }

    sleep(1);
    
  }

  peiskmt_shutdown();
}

