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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include "pthread.h"
#include <peiskernel/peiskernel_mt.h>

/* This is the last tuple which we got when doing a getTuple operation */
PeisTuple *lastTuple=NULL;
int lastLen=0, lastOwner=0, lastHandle=0;
char *lastData=NULL;
double nextExpire=0.0;
double nextUser=0.0;
char tmpKey[256];

typedef void *(*Fun)();

/* Function to help the initialization */
void peislisp_initialize(char *arg) {
  char *args[1024];
  int nargs=0;
  char *workstring=strdup(arg);
  /* First, tokenize the string */
  while(1) {    
    char *token=strtok((nargs==0)?workstring:NULL," \n\t"); 
    if(token) args[nargs++]=strdup(token);
    else break;
    if(nargs >= 1024) break;    
  }
  free(workstring);
  /* Then initialize peiskernel with those args */
  peiskmt_initialize(&nargs,args);
}



/* Attempts to get a tuple, returns nonzero if successfull and updates
   the global lastTuple variable */
int peislisp_getTuple(char *key,int owner,int flags) {
  lastTuple = peiskmt_getTuple(key,owner,&lastLen,(void**) &lastData,flags);
  lastOwner=owner;
  return lastTuple ? 1 : 0;
}
char *peislisp_tupleValue() {
  /* Make sure the string is null terminated */
  if(!lastTuple) return "(null)";
  if(strlen(lastTuple->data) >= lastTuple->datalen) return "(null)";
  return lastTuple->data;
}
int peislisp_tupleOwner() { return lastTuple?lastTuple->owner:0; }
int peislisp_tupleCreator() { return lastTuple?lastTuple&&lastTuple->creator:0; }
/** Returns the key of the last tuple */
char *peislisp_tupleKey() {
  /* Make sure the string is null terminated */
  if(!lastTuple) return NULL;
  peiskmt_getTupleName(lastTuple,tmpKey,sizeof(tmpKey));
  return tmpKey;
}
/** Rewrites a normal string key into a LISP list, using NIL for wildcards. */
char *peislisp_key2list(char *key) {
  char *c1;
  c1=tmpKey;
  *(c1++) = '(';
  for(;*key;key++) 
    if(*key == '.') *(c1++)=' ';
    else if(*key == '*') { *(c1++)='('; *(c1++) = ')'; }
    else if(*key == '(') return NULL;
    else if(*key == ')') return NULL;
    else *(c1++) = *key;
  *(c1++) = ')';
  *(c1++) = 0;
  return tmpKey;
}


/* Attempts to get another tuple, note the use of the "handle" */
int peislisp_getTuples(char *key,int flags) {
  lastTuple = peiskmt_getTuples(key,&lastOwner,&lastLen,(void**) (&lastData),&lastHandle,flags);
  return lastTuple ? 1 : 0;
} 
int peislisp_getTuples_reset () { lastHandle=0; }

/***************** New RESULTSET based access to multiple tuples */
PeisTupleResultSet *resultset=NULL;
void peislisp_resetResultset() {
  if(!resultset) resultset = peiskmt_createResultSet();
  peiskmt_resultSetReset(resultset);
}
/** Returns number of found tuples */
int peislisp_findTuples(char *key,int owner,int old) {
  PeisTuple prototype;
  if(!resultset) resultset = peiskmt_createResultSet();
  peiskmt_resultSetReset(resultset);
  peiskmt_initAbstractTuple(&prototype);
  peiskmt_setTupleName(&prototype,key);
  prototype.owner = owner;
  if(nextExpire >= 0.0) {
    prototype.ts_expire[0] = (int) nextExpire;
    prototype.ts_expire[1] = (int) (fmod(nextExpire,1.0)*1e6);  
  }
  if(nextUser >= 0.0) {
    prototype.ts_user[0] = (int) nextUser;
    prototype.ts_user[1] = (int) (fmod(nextUser,1.0)*1e6);
  }
  nextExpire=-1.0;
  nextUser=-1.0;
  if(old) prototype.isNew=-1;
  else prototype.isNew=1;
  prototype.data=NULL;
  return peiskmt_findTuples(&prototype,resultset);
}
/** Steps to the next result, returns zero if no more results left. */
int peislisp_nextResult() {
  if(peiskmt_resultSetNext(resultset)) {
    lastTuple = peiskmt_resultSetValue(resultset);
    return 1;
  } else {
    lastTuple = NULL;
    return 0;
  }
}



double peislisp_tupleWriteTS() {
  if(!lastTuple) return 0.0;
  return lastTuple->ts_write[0] + 1e-6*lastTuple->ts_write[1];
}
double peislisp_tupleUserTS() {
  if(!lastTuple) return 0.0;
  return lastTuple->ts_user[0] + 1e-6*lastTuple->ts_user[1];
}
double peislisp_tupleExpireTS() {
  if(!lastTuple) return 0.0;
  return lastTuple->ts_expire[0] + 1e-6*lastTuple->ts_expire[1];
}

/* Sets the expiry timepoint for the next tuple that is
   created or searched for. Deleted after one use! */
void peislisp_nextExpire(double t) {
  nextExpire=t;
}
/* Sets the user timepoint for the next tuple that is
   created or searched for. Deleted after one use! */
void peislisp_nextUser(double t) {
  nextUser=t;
}


void peislisp_setTuple(char *key,char *value,int owner) {
  PeisTuple tuple;
  peiskmt_initTuple(&tuple);
  peiskmt_setTupleName(&tuple,key);
  tuple.owner = owner;
  tuple.datalen=strlen(value)+1;
  tuple.data=value;
  if(nextExpire >= 0.0) {
    tuple.ts_expire[0] = (int) nextExpire;
    tuple.ts_expire[1] = (int) (fmod(nextExpire,1.0)*1e6);  
  }
  if(nextUser >= 0.0) {
    tuple.ts_user[0] = (int) nextUser;
    tuple.ts_user[1] = (int) (fmod(nextUser,1.0)*1e6);
  }
  peiskmt_insertTuple(&tuple);
  
  nextExpire=-1.0;
  nextUser=-1.0;
}


void peislisp_subscribeIndirectTuple(char *key,int owner) {
  PeisTuple tuple;
  peiskmt_initAbstractTuple(&tuple);
  peiskmt_setTupleName(&tuple,key);
  tuple.owner=owner;
  peiskmt_subscribeIndirectTuple(&tuple);
}

int peislisp_getIndirectTuple(char *key, int owner, int flags) {
  PeisTuple proto;
  peiskmt_initAbstractTuple(&proto);
  peiskmt_setTupleName(&proto,key);
  proto.owner=owner;
  proto.isNew = (flags&1?-1:1);
  lastTuple=NULL;

  do {
    lastTuple=peiskmt_findIndirectTuple(&proto);
    if(!lastTuple && (flags&2)) usleep(10000);
  } while(!lastTuple && (flags&2));
  lastOwner=owner;  

  return lastTuple ? 1 : 0;
}

void peislisp_setIndirectStringTuple(char *metaKey, int metaOwner, char *value) {
  PeisTuple metaProto;
  peiskmt_initAbstractTuple(&metaProto);
  peiskmt_setTupleName(&metaProto,metaKey);
  metaProto.owner=metaOwner;
  metaProto.isNew=-1;

  PeisTuple tuple;
  if(peisk_initIndirectTuple(&metaProto,&tuple)) {
    /*printf("Failed to initizlie meta tuple..\n");*/
    return;
  }
  tuple.datalen=strlen(value)+1;
  tuple.data=value;

  if(nextExpire >= 0.0) {
    tuple.ts_expire[0] = (int) nextExpire;
    tuple.ts_expire[1] = (int) (fmod(nextExpire,1.0)*1e6);  
  }
  if(nextUser >= 0.0) {
    tuple.ts_user[0] = (int) nextUser;
    tuple.ts_user[1] = (int) (fmod(nextUser,1.0)*1e6);
  }
  peiskmt_insertTuple(&tuple);
  
  nextExpire=-1.0;
  nextUser=-1.0;
}


/* This function is only here to help the lisp foreign functions link correctly... */
void peiskernel_mt_dummy() {
  Fun fun;
  /*fun=(Fun) peiskmt_initialize;*/
  fun=(Fun) peiskmt_shutdown;
  fun=(Fun) peiskmt_gettime;
  fun=(Fun) peiskmt_gettimef;
  fun=(Fun) peiskmt_connect;
  fun=(Fun) peiskmt_autoConnect;
  fun=(Fun) peiskmt_peisid;

  fun=(Fun) peiskmt_getTuple;
  fun=(Fun) peiskmt_getTuples;
  fun=(Fun) peiskmt_subscribe;
  fun=(Fun) peiskmt_unsubscribe;
  fun=(Fun) peiskmt_registerTupleCallback;
  fun=(Fun) peiskmt_unregisterTupleCallback;

  fun=(Fun) peiskmt_subscribeIndirectTuple;
  fun=(Fun) peiskmt_getIndirectTuple;
  fun=(Fun) peiskmt_isIndirectTuple;
  fun=(Fun) peiskmt_writeMetaTuple;
}
