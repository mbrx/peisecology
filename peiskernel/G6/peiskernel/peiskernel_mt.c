/** \file peiskernel_mt.c
   A multithread (posix threads) wrapper for the peiskernel.
   If you use this wrapper library NO FUNCTION IN peiskernel.h SHOULD BE USED
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
#include <unistd.h>

#include <sys/select.h>

#include "pthread.h"
#define PEISK_PRIVATE
#include "peiskernel.h"
#include "tuples.h"
#include "tuples_private.h"
#include "peiskernel_mt.h"

pthread_t peiskmt_kernel_thread;
int peiskmt_kernel_is_running=0;

/** Mutex for exclusive access to all peiskernel functions (peisk_*) */
pthread_mutex_t peiskmt_kernel_mutex;   

/** Used to save errno from wrappers before releasing semaphor (which
   might destroy this value) */
int peiskmt_savedErrno=0;


/** Predeclared kernel thread fn. This is the thread which is continously running the kernel. For internal use only. */
typedef void *(*PosixThreadFn)(void*);
void peiskmt_kernel_thread_fn(void*);

void peiskmt_initialize(int *argc,char **args) {
  pthread_attr_t attr;

  if(peiskmt_kernel_is_running) { fprintf(stderr,"peiskmt: Error, attempting to start multiple peiskernels in the same process\n"); return; }
  peiskmt_kernel_is_running=1;

  peisk_initialize(argc,args);

  pthread_mutex_init(&peiskmt_kernel_mutex,NULL);

  pthread_attr_init(&attr);
  /*pthread_attr_setschedpolicy(&attr,SCHED_RR); */ /* NOTE - we can only do this is running as superuser... */
  /*pthread_attr_setschedparam(&attr,127);*/
  pthread_create(&peiskmt_kernel_thread,&attr,(PosixThreadFn) peiskmt_kernel_thread_fn, (void*) NULL);

}

void peiskmt_lock() {
  pthread_mutex_lock(&peiskmt_kernel_mutex);
}
void peiskmt_unlock() {
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
}


void peiskmt_shutdown() {
  /* First, signal peiskernel to shutdown */
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  peisk_shutdown();
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
  sleep(2);

  /* Join kernel thread and destroy all thread/mutex data */
  pthread_join(peiskmt_kernel_thread,NULL);
  pthread_mutex_destroy(&peiskmt_kernel_mutex);

  peiskmt_kernel_is_running=0;
}

int peiskmt_isRoutable(int id) {
  int ret;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  ret=peisk_isRoutable(id);
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
  return ret;
}

void peiskmt_kernel_thread_fn(void* data) {
  /*double t0,tNow;*/

#ifdef GUMSTIX
  struct timeval timeout;
#else
  struct timespec timeout;
#endif

  fd_set readSet, writeSet, excpSet;
  int n=0;
  int ret;

  FD_ZERO(&readSet);
  FD_ZERO(&writeSet);
  FD_ZERO(&excpSet);

  double laststep=0.0, timenow, avgStepTime=0.1;

  while(peisk_isRunning()) {

    timeout.tv_sec = 0;
#ifdef GUMSTIX
    timeout.tv_usec = (int) 1e4;
    ret=select(n,&readSet,&writeSet,&excpSet,&timeout);
#else
    timeout.tv_nsec = (int) 1e7; // once every 10ms
    ret=pselect(n,&readSet,&writeSet,&excpSet,&timeout,NULL); /* added last argument --AS 060818 */
#endif

    /* Prevent multithreaded kernel from running more than 200
       steps/second. This is to prevent from using up all CPU when the
       pselect call is wrongly triggered early. UGLY HACK! */
    timenow=peisk_getrawtimef();
    if(laststep!=0.0) avgStepTime = 0.99*avgStepTime+0.01*(timenow-laststep);
    laststep=timenow;
    if(avgStepTime < 5e-3) usleep(5000);

    pthread_mutex_lock(&peiskmt_kernel_mutex);  
    peisk_step();
    peisk_setSelectReadSignals(&n,&readSet,&writeSet,&excpSet);
    pthread_mutex_unlock(&peiskmt_kernel_mutex);    
  }
}

int peiskmt_peisid() { return peisk_id; }
int peiskmt_getTupleErrno() { return peiskmt_savedErrno; }
int peiskmt_isRunning() { return peisk_isRunning(); }

/******************************************************************/
/*                                                                */
/*                            NO WRAPPER NEEDED                   */
/*                                                                */
/* These functions are just passed along without using the        */
/* semaphors for now.                                             */
/******************************************************************/

int peiskmt_gettime() { return peisk_gettime(); }
double peiskmt_gettimef() { return peisk_gettimef(); }
void peiskmt_gettime2(int *t0,int *t1) { peisk_gettime2(t0,t1); }

/******************************************************************/
/*                                                                */
/*                            SIMPLE WRAPPERS                     */
/*                                                                */
/* Mutex controlled simple wrappers                               */
/******************************************************************/

void peiskmt_printTuple(PeisTuple *tuple) {
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  peisk_printTuple(tuple);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
}
void peiskmt_snprintTuple(char *buf,int len,PeisTuple *tuple) {
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  peisk_snprintTuple(buf,len,tuple);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
}

void peiskmt_printUsage(FILE *stream,short argc,char **args) { 
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  peisk_printUsage(stream,argc,args);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
}

PeisTuple *peiskmt_cloneTuple(PeisTuple *tuple) {
  PeisTuple *result;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result=peisk_cloneTuple(tuple);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
  return result;
}

void peiskmt_freeTuple(PeisTuple *tuple) {
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  peisk_freeTuple(tuple);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
}

PeisSubscriberHandle peiskmt_subscribeByAbstract(PeisTuple *tuple) {
  PeisSubscriberHandle result;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result=peisk_subscribeByAbstract(tuple);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
  return result;
}

int peiskmt_reloadSubscription(PeisSubscriberHandle subscriber) {
  int result;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result=peisk_reloadSubscription(subscriber);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
  return result;
}

int peiskmt_hasSubscriberByAbstract(PeisTuple *prototype) {
  int result;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result=peisk_hasSubscriberByAbstract(prototype);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
  return result;  
}

void peiskmt_initTuple(PeisTuple *tuple) {
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  peisk_initTuple(tuple);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
}

void peiskmt_initAbstractTuple(PeisTuple *tuple) {
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  peisk_initAbstractTuple(tuple);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
}

int peiskmt_setTupleName(PeisTuple *tuple,const char *fullname) {
  int result;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result=peisk_setTupleName(tuple,fullname);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
  return result;
}
int peiskmt_getTupleName( PeisTuple*tuple,char *buffer,int buflen) {
  int result;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result=peisk_getTupleName(tuple,buffer,buflen);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
  return result;
}


int peiskmt_tupleIsAbstract(PeisTuple *tuple) {
  int result;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result=peisk_tupleIsAbstract(tuple);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
  return result;
}

int peiskmt_insertTuple(PeisTuple *tuple) {
  int result;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result=peisk_insertTuple(tuple);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
  return result;
}
int peiskmt_insertTupleBlocking(PeisTuple *tuple) {
  int result;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result=peisk_insertTupleBlocking(tuple);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
  return result;
}

PeisTuple *peiskmt_getTupleByAbstract(PeisTuple *prototype) {
  PeisTuple *result;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result=peisk_getTupleByAbstract(prototype);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
  return result;  
}
int peiskmt_getTuplesByAbstract(PeisTuple *proto,PeisTupleResultSet *rs) {
  int result;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result=peisk_getTuplesByAbstract(proto,rs);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
  return result;  
}
PeisTupleResultSet *peiskmt_createResultSet() {
  PeisTupleResultSet *result;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result=peisk_createResultSet();
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
  return result;  
}
void peiskmt_resultSetReset(PeisTupleResultSet*rs) {
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  peisk_resultSetReset(rs);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
}
void peiskmt_deleteResultSet(PeisTupleResultSet *rs) {
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  peisk_deleteResultSet(rs);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
}
void peiskmt_resultSetFirst(PeisTupleResultSet *rs) {
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  peisk_resultSetFirst(rs);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);    
}
PeisTuple *peiskmt_resultSetValue(PeisTupleResultSet *rs) {
  PeisTuple *result;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result=peisk_resultSetValue(rs);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
  return result;    
}
int peiskmt_resultSetNext(PeisTupleResultSet *rs) {
  int result;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result=peisk_resultSetNext(rs);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
  return result;    
}
int peiskmt_resultSetIsEmpty(PeisTupleResultSet *rs) {
  int result;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result=peisk_resultSetIsEmpty(rs);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
  return result;    
}
int peiskmt_compareTuples(PeisTuple *tuple1, PeisTuple *tuple2) {
  int result;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result=peisk_compareTuples(tuple1, tuple2);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
  return result;    
}
int peiskmt_isGeneralization(PeisTuple *tuple1, PeisTuple *tuple2) {
  int result;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result=peisk_isGeneralization(tuple1, tuple2);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
  return result;    
}
int peiskmt_isEqual(PeisTuple *tuple1, PeisTuple *tuple2) {
  int result;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result=peisk_isEqual(tuple1, tuple2);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
  return result;    
}
const char *peiskmt_tuple_strerror(int error) {
  const char *result;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result=peisk_tuple_strerror(error);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
  return result;      
}

int peiskmt_connect(char *url,int flags) { 
  int result;

  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result=peisk_connect(url,flags) ? 1 : 0;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
  return result;
}
void peiskmt_autoConnect(char *url) {
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  peisk_autoConnect(url);
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
}
void peiskmt_setTuple(const char *key,int len,const void *data,const char *mimetype,int encoding) {
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  peisk_setTuple(key,len,data,mimetype,encoding);
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
}
void peiskmt_setStringTuple(const char *key,const char *data) {
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  peisk_setStringTuple(key,data);
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
}
int peiskmt_hasSubscriber(const char *key) {
  int result;

  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result=peisk_hasSubscriber(key);
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
  return result;
}
void peiskmt_setRemoteTuple(int owner,const char *key,int len,const void *data,const char *mimetype,int encoding) {
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  peisk_setRemoteTuple(owner,key,len,data,mimetype,encoding);
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
}
int peiskmt_setRemoteTupleBlocking(int owner,const char *key,int len,const void *data,const char *mimetype,int encoding) {
  int ret;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  ret = peisk_setRemoteTupleBlocking(owner,key,len,data,mimetype,encoding);
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
  return ret;
}
void peiskmt_setRemoteStringTuple(int owner,const char *key,const char *data) {
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  peisk_setRemoteStringTuple(owner,key,data);
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
}
int peiskmt_setRemoteStringTupleBlocking(int owner,const char *key,const char *data) {
  int ret;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  ret = peisk_setRemoteStringTupleBlocking(owner,key,data);
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
  return ret;
}
struct PeisTuple *peiskmt_getTuple(int owner,const char *key,int flags) {
  struct PeisTuple *result;

  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result=peisk_getTuple(owner,key,flags);
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
  return result;
}
int peiskmt_getTuples(int owner, const char *key,PeisTupleResultSet *rs) {
  int result;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result=peisk_getTuples(owner,key,rs);
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
  return result;  
}

PeisSubscriberHandle peiskmt_subscribe(int owner,const char *key) {
  PeisSubscriberHandle result;

  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result=peisk_subscribe(owner,key);
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
  return result;  
}
void peiskmt_unsubscribe(PeisSubscriberHandle handle) {
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  peisk_unsubscribe(handle);
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
}
void peiskmt_tsUser(int sec,int usec) {
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  peisk_tsUser(sec,usec);
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
}

/******************************************************************/
/*                                                                */
/*                            CALLBACK FUNCTIONS                  */
/*                                                                */
/* These functions need to exit the mutex lock when callback is   */
/* invoked and enter it again after callback...                   */
/******************************************************************/

/** Structure for housekeeping fn and userdata when intercepting a
    callback going out from the peiskernel (drop semaphore) and back
    in again (reacquire semaphore). */
struct peiskmt_protectCallbackInfo {
  PeisTupleCallback *realCallback;
  void *realUserdata;
};
void peiskmt_protectCallbacks(PeisTuple *tuple,void *infodata) {
  struct peiskmt_protectCallbackInfo *info=(struct peiskmt_protectCallbackInfo *) infodata;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
  (info->realCallback)(tuple,info->realUserdata);
  pthread_mutex_lock(&peiskmt_kernel_mutex);  
}

PeisCallbackHandle peiskmt_registerTupleCallbackByAbstract(PeisTuple *tuple,void *userdata,PeisTupleCallback *fn) {
  PeisCallbackHandle result;

  /* This malloc corresponds to the "free" in unregisterTupleCallback below */
  struct peiskmt_protectCallbackInfo *info=
    (struct peiskmt_protectCallbackInfo *) malloc(sizeof(struct peiskmt_protectCallbackInfo));
  if(!info) return 0;
  info->realCallback=fn;
  info->realUserdata=userdata;
  pthread_mutex_lock(&peiskmt_kernel_mutex);  
  result = (PeisCallbackHandle) peisk_registerTupleCallbackByAbstract(tuple,(void*)info,peiskmt_protectCallbacks);
  pthread_mutex_unlock(&peiskmt_kernel_mutex);    
  return result;
}

PeisCallbackHandle peiskmt_registerTupleCallback(int owner,const char *key,void *userdata,
						 PeisTupleCallback *fn) {
  PeisCallbackHandle result;
  /* This malloc corresponds to the "free" in unregisterTupleCallback below */
  struct peiskmt_protectCallbackInfo *info=
    (struct peiskmt_protectCallbackInfo *) malloc(sizeof(struct peiskmt_protectCallbackInfo));
  if(!info) return 0;
  info->realCallback=fn;
  info->realUserdata=userdata;
  pthread_mutex_lock(&peiskmt_kernel_mutex);  
  result = (PeisCallbackHandle) peisk_registerTupleCallback(owner,key,(void*)info,peiskmt_protectCallbacks);
  pthread_mutex_unlock(&peiskmt_kernel_mutex);    
  return result;
}
void peiskmt_unregisterTupleCallback(PeisCallbackHandle cbHandle) {
  PeisCallbackHandle handle=(PeisCallbackHandle) cbHandle;
  PeisCallback *callback;

  pthread_mutex_lock(&peiskmt_kernel_mutex);  
  /* This free corresponds to the "malloc" in registerTupleCallback
     above */
  callback = peisk_findCallbackHandle(handle);
  free(callback->userdata);
  peisk_unregisterTupleCallback(handle);
  pthread_mutex_unlock(&peiskmt_kernel_mutex);      
}

PeisSubscriberHandle peiskmt_subscribeIndirectlyByAbstract(PeisTuple *M) {
  PeisSubscriberHandle result;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result = peisk_subscribeIndirectlyByAbstract(M);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
  return result;
}

PeisTuple *peiskmt_getTupleIndirectlyByAbstract(PeisTuple *M) {
  PeisTuple *result;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result = peisk_getTupleIndirectlyByAbstract(M);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
  return result;
}

/*int peiskmt_initIndirectTuple(PeisTuple *M,PeisTuple *A) {
  int result;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result = peisk_initIndirectTuple(M,A);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
  return result;
  }*/

PeisSubscriberHandle peiskmt_subscribeIndirectly(int metaOwner,const char *metaKey) {
  PeisSubscriberHandle result;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result = peisk_subscribeIndirectly(metaOwner,metaKey);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
  return result;
}
PeisTuple *peiskmt_getTupleIndirectly(int metaOwner,const char *metaKey,int flags) {
  PeisTuple *result;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result = peisk_getTupleIndirectly(metaOwner,metaKey,flags);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
  return result;
}
int peiskmt_setStringTupleIndirectly(int metaOwner,const char *metaKey,const char *value) {
  int result;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result = peisk_setStringTupleIndirectly(metaOwner,metaKey,value);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
  return result;
}
int peiskmt_isMetaTuple(int metaOwner,const char *metaKey) {
  int result;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result = peisk_isMetaTuple(metaOwner,metaKey);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
  return result;
}
void peiskmt_declareMetaTuple(int metaOwner,const char *metaKey) {
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  peisk_declareMetaTuple(metaOwner,metaKey);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
}
void peiskmt_setMetaTuple(int metaOwner,const char *metaKey,int realKey,const char *realOwner) {
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  peisk_setMetaTuple(metaOwner,metaKey,realKey,realOwner);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
}

int peiskmt_findOwner(char *key) {
  int ret = -1;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  ret = peisk_findOwner(key);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
  return ret;
}

void peiskmt_setDefaultStringTuple(const char *key,const char *value) {
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  peisk_setDefaultStringTuple(key,value);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
}
void peiskmt_setDefaultTuple(const char *key,int datalen, void *data,const char *mimetype,int encoding) {
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  peisk_setDefaultTuple(key,datalen,data,mimetype,encoding);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
}
void peiskmt_setDefaultMetaTuple(const char *key,int datalen, void *data,const char *mimetype,int encoding) {
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  peisk_setDefaultMetaTuple(key,datalen,data,mimetype,encoding);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
}
void peiskmt_setDefaultMetaStringTuple(const char *key,const char *value) {
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  peisk_setDefaultMetaStringTuple(key,value);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
}

int peiskmt_tupleExists(const char *key) {
  int result;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result=peisk_tupleExists(key);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
  return result;
}

void peiskmt_deleteTuple(int owner,const char *key) {
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  peisk_deleteTuple(owner,key);
  peiskmt_savedErrno=peisk_tuples_errno;
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
}

PeisCallbackHandle peiskmt_registerTupleDeletedCallback(int owner,const char *key,void *userdata,PeisTupleCallback *fn) {
  PeisCallbackHandle result;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result=peisk_registerTupleDeletedCallback(owner,key,userdata,fn);
  pthread_mutex_unlock(&peiskmt_kernel_mutex);  
  return result;
}

PeisCallbackHandle peiskmt_registerTupleDeletedCallbackByAbstract(PeisTuple *t,void *userdata,PeisTupleCallback *fn) {
  PeisCallbackHandle result;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result=peisk_registerTupleDeletedCallbackByAbstract(t,userdata,fn);
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
  return result;
}

void peiskmt_appendTupleByAbstract(PeisTuple *abstractTuple,int difflen,const void *diff) {
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  peisk_appendTupleByAbstract(abstractTuple,difflen,diff);
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
}
void peiskmt_appendStringTupleByAbstract(PeisTuple *abstractTuple,const char *value) {
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  peisk_appendStringTupleByAbstract(abstractTuple,value);
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
}
void peiskmt_appendTuple(int owner,const char *key,int difflen,const void *diff) {
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  peisk_appendTuple(owner,key,difflen,diff);
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
}
void peiskmt_appendStringTuple(int owner,const char *key,const char *str) {
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  peisk_appendStringTuple(owner,key,str);
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
}
int peiskmt_parseMetaTuple(PeisTuple *meta,int *owner,char *key) {
  int result;
  pthread_mutex_lock(&peiskmt_kernel_mutex);
  result=peisk_parseMetaTuple(meta,owner,key);
  pthread_mutex_unlock(&peiskmt_kernel_mutex);
  return result;
}
