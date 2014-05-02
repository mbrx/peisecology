/** \file tuplesAPI.c
    \brief All public API functions for accessing and maniulating the distributed tuplespace. 
*/
/**
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
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <string.h>

#define PEISK_PRIVATE
#include "peiskernel.h"
#include "hashtable.h"

/* PEIS_TUPLE field constructors and accessors */
int peisk_getTupleName(PeisTuple *tuple,char *buffer,int buflen) {
  int len,i;

  /* Wildcards on number of keys gives an empty string as tuplename */
  len=0;
  if(tuple->keyDepth == -1) { buffer[0]=0; return 0; }
  len+=snprintf(buffer+len,buflen-len,"%s",tuple->keys[0]?tuple->keys[0]:"*" );
  if(len > buflen) return PEISK_TUPLE_BUFFER_OVERFLOW; 
  for(i=1;i<tuple->keyDepth;i++) {
    len+=snprintf(buffer+len,buflen-len,".%s",tuple->keys[i]?tuple->keys[i]:"*" );
    if(len > buflen) return PEISK_TUPLE_BUFFER_OVERFLOW; 
  }
  return 0;
}
int peisk_getTupleFullyQualifiedName(PeisTuple *tuple,char *buffer,int buflen) {
  int len;
  len=snprintf(buffer,buflen,"%d.",tuple->owner);
  if(len >= buflen) return PEISK_TUPLE_BADKEY;
  return peisk_getTupleName(tuple,buffer+len,buflen-len);
}

int peisk_setTupleName(PeisTuple *tuple,const char *fullname) {
  int nparts,i;
  char *c;

  if(!fullname) return PEISK_TUPLE_BADKEY;
  /* Copy full key name into key buffer, but only if it is not already
     a pointer to the keybuffer since this is used by internal
     functions. */
  if(strlen(fullname)+1 > PEISK_KEYLENGTH) return PEISK_TUPLE_BADKEY;    
  if(tuple->keybuffer != fullname) {
    strcpy(tuple->keybuffer,fullname);
  }
  /* Clear remaining data in keybuffer to zero for general
     consistency and for cleaner data when transmitted over the
     network. */
  i=strlen(tuple->keybuffer)+1;
  if(sizeof(tuple->keybuffer)-i > 0)
    memset(tuple->keybuffer+i,0,sizeof(tuple->keybuffer)-i);


  for(nparts=0;nparts<7;nparts++) tuple->keys[nparts]=NULL;
  /* Insert NULL at separation points '.' and let subkeys point to begining
     of all these new NULL terminated strings */
  for(nparts=0, c=tuple->keybuffer;nparts<7&&*c;nparts++) {
    tuple->keys[nparts]=c;
    /* Handle wildcards '*' specially, error with eg. foo.*kalle.boo 
       Should rather be foo.*.boo */
    if(c[0] == '*' && c[1] != 0 && c[1] != '.') return PEISK_TUPLE_BADKEY;
    if(c[0] == '*') tuple->keys[nparts]=NULL;
    while(*c != '.' && *c != 0) c++;
    if(*c != 0) { *c = 0; c++; }
  }
  /* Treat empty string specially, this is the tuple with any key depth */
  if(nparts == 0) tuple->keyDepth = -1;
  /* Save key depth into tuple structure */
  else tuple->keyDepth=nparts;

  return 0;
}
void peisk_initTuple(PeisTuple *tuple) {
  int i;

  /* Owner, defaults to ourselves */
  tuple->owner=peiskernel.id;
  /* Creator, defaults to ourselves */
  tuple->creator=peiskernel.id;
  /* ts_write, ignored since it will be overwritten when inserted */
  tuple->ts_write[0]=0; tuple->ts_write[1]=0;
  /* ts_user, set to ts_user upon insert. */
  tuple->ts_user[0]=ts_user[0]; tuple->ts_user[1]=ts_user[1];
  /* ts_expire, set to never */
  tuple->ts_expire[0]=0;  tuple->ts_expire[1]=0;

  /* Encoding, defaults to plain */
  tuple->encoding=PEISK_ENCODING_ASCII;

  /* MIME-type, defaults to text/plain */
  tuple->mimetype="text/plain";

  /* keys, all wildcards */
  for(i=0;i<7;i++) tuple->keys[i]=NULL; tuple->keyDepth=-1;
  /* data, NULL (wildcard) since user need to initialize this. */
  tuple->data=NULL;
  tuple->datalen=0;
  tuple->alloclen=0;
  tuple->isNew=1;

  tuple->seqno=0;
  /* internal values, do not need to initialized */
}
void peisk_initAbstractTuple(PeisTuple *tuple) {
  int i;

  /* Owner, defaults to ourselves */
  tuple->owner=-1;
  /* Creator, defaults to ourselves */
  tuple->creator=-1;
  /* ts_write, ignored since it will be overwritten when inserted */
  tuple->ts_write[0]=-1; tuple->ts_write[1]=-1;
  /* ts_user, set to ts_user upon insert. */
  tuple->ts_user[0]=-1; tuple->ts_user[1]=-1;
  /* ts_expire, wildcard for now */
  tuple->ts_expire[0]=-1;  tuple->ts_expire[1]=-1;

  /* Encoding, defaults to wildcard */
  tuple->encoding=PEISK_ENCODING_WILDCARD;

  /* MIME-type, defaults to wildcard */
  tuple->mimetype=NULL;

  /* keys, all wildcards */
  for(i=0;i<7;i++) tuple->keys[i]=NULL; tuple->keyDepth=-1;
  /* data, NULL */
  tuple->data=NULL;
  tuple->datalen=0;
  tuple->alloclen=0;
  tuple->isNew=-1;

  tuple->seqno=-1;
  /* internal values, do not need to initialized */
}
PeisTuple *peisk_cloneTuple(PeisTuple *original) {
  PeisTuple *tuple;
  int i;

  tuple=(PeisTuple*)malloc(sizeof(PeisTuple));
  if(!tuple) { 
    /* Out of memory! */
    peisk_tuples_errno=PEISK_TUPLE_OUT_OF_MEMORY;
    return NULL; 
  }
  *tuple=*original;
  tuple->mimetype = original->mimetype?peisk_cloneMimetype(original->mimetype):NULL;
  
  if(original->data) {
    tuple->data=(void*)malloc(original->datalen);
    memcpy(tuple->data,original->data,original->datalen);
    if(!tuple->data) { free(tuple); return NULL; } /* Out of memory */
  }
  /* Preserve the offsets used for the subkeys */
  for(i=0;i<7;i++) {
    if(original->keys[i]) 
      tuple->keys[i] = original->keys[i]-original->keybuffer+tuple->keybuffer;
  }
  tuple->isNew=1;
  return tuple;
}
void peisk_freeTuple(PeisTuple *tuple) {
  if(!tuple) return;
  if(tuple->data) free(tuple->data);
  free(tuple);
}


/*                                 */
/* STATEMACHINE changing functions */
/*                                 */

void peisk_tsUser(int ts_user_sec,int ts_user_usec) {
  ts_user[0]=ts_user_sec; ts_user[1]=ts_user_usec;
}

/*                          */
/* INSERTING/SETTING TUPLES */
/*                          */

void peisk_setStringTuple(const char *key,const char *data) {
  //printf("setting %s to '%s' of length %d+1 = %d\n",key,data,strlen(data),strlen(data)+1); fflush(stdout);
  peisk_setTuple(key,strlen(data)+1,(const void*)data,"text/plain",PEISK_ENCODING_ASCII);
}
void peisk_setTuple(const char *key,int len,const void *data,const char *mimetype,int encoding) {
  PeisTuple tuple;
  peisk_initTuple(&tuple);
  peisk_setTupleName(&tuple,key);
  tuple.mimetype=peisk_cloneMimetype(mimetype);
  tuple.encoding=encoding;
  tuple.data = (void*) data;
  tuple.datalen = len;
  peisk_insertTuple(&tuple);
}
void peisk_appendStringTuple(int owner,const char *key,const char *str) {
  peisk_appendTuple(owner,key,strlen(str)+1,str);
}
void peisk_appendTuple(int owner,const char *key,int difflen,const void *diffdata) {
  PeisTuple tuple;
  peisk_initAbstractTuple(&tuple);
  peisk_setTupleName(&tuple,key);
  tuple.owner = owner;
  /*printf("Appending tuple.. %d %s += %s\n",owner,key,(char*)diffdata);*/
  peisk_appendTupleByAbstract(&tuple,difflen,diffdata);
}


void peisk_setRemoteStringTuple(int owner,const char *key,const char *value) {
  peisk_setRemoteTuple(owner,key,strlen(value)+1,value,"text/plain",PEISK_ENCODING_ASCII);
}
void peisk_setRemoteTuple(int owner,const char *key,int len,const void *data,const char *mimetype,int encoding) {
  PeisTuple tuple;
  peisk_initTuple(&tuple);
  peisk_setTupleName(&tuple,key);
  tuple.mimetype=peisk_cloneMimetype(mimetype);
  tuple.encoding=encoding;
  tuple.owner = owner;
  tuple.data = (void*) data;
  tuple.datalen = len;
  peisk_insertTuple(&tuple);
}

/****  Blocking version of set/insert tuple API *****/
int peisk_setRemoteStringTupleBlocking(int owner,const char *key,const char *value) {
  return peisk_setRemoteTupleBlocking(owner,key,strlen(value)+1,value,"text/plain",PEISK_ENCODING_ASCII);
}
int peisk_setRemoteTupleBlocking(int owner,const char *key,int len,const void *data,const char *mimetype,int encoding) {
  PeisTuple tuple;
  peisk_initTuple(&tuple);
  peisk_setTupleName(&tuple,key);
  tuple.mimetype=peisk_cloneMimetype(mimetype);
  tuple.encoding=encoding;
  tuple.owner = owner;
  tuple.data = (void*) data;
  tuple.datalen = len;
  return peisk_insertTupleBlocking(&tuple);
}

void peisk_insertTupleBlockingCallback(int success, int datalen, PeisPackage *package, void *userdata) {
  int *result = (int*) userdata;
  if(success) *result=0;
  else *result=1;
}
int peisk_insertTupleBlocking(PeisTuple *tuple) {
  int success=0;

  with_ack_hook(peisk_insertTupleBlockingCallback,
		((void*)&success),
		{success=peisk_insertTuple(tuple);}
		);
  if(success != 0) {
    return success;    
  }
  if(tuple->owner == peiskernel.id) {
    return success;
  }
  /* No immediate success/failure, wait until setRemoteTuple message have been acknowledged */
  success=-1;
  while(success == -1) {
    peisk_wait(10000);
  }
  return success;
}
/**** ---- ****/

int peisk_insertTuple(PeisTuple *tuple) {
  PeisPushTupleMessage *message;
  int len;

  if(peisk_tupleIsAbstract(tuple)) return PEISK_TUPLE_IS_ABSTRACT;
  /** \todo Check that noone is trying to change the "all-keys" tuple */
  /* Update timestamp of writing */
  peisk_gettime2(&tuple->ts_write[0],&tuple->ts_write[1]);
  if(tuple->owner == peiskernel.id) {
    return peisk_addToLocalSpace(tuple);
  } else {
    int mimelength=tuple->mimetype?strlen(tuple->mimetype):0;
    int suffixLength = tuple->datalen + mimelength;
    len=sizeof(PeisPushTupleMessage)+suffixLength;
    message = (PeisPushTupleMessage*) peisk_getTupleBuffer(len);
    peisk_tuple_hton(tuple,&message->tuple);

    message->tuple.isNew = htonl(tuple->isNew);
    /* We don't store any subkeys in the message to avoid any possible
       inconsistency problems, the full key is copied in the private
       part anyway (keybuffer). The same goes for keyDepth. */
    /* Copy the keyname from given tuple to the new message tuple */
    peisk_getTupleName(tuple,message->tuple.keybuffer,sizeof(message->tuple.keybuffer));
    message->tuple.keyDepth=0;
    message->tuple.alloclen=0;

    if(tuple->mimetype) memcpy((void*)message+sizeof(PeisPushTupleMessage), (void*) tuple->mimetype, mimelength);
    memcpy((void*)message+sizeof(PeisPushTupleMessage)+mimelength, (void*) tuple->data, tuple->datalen);

    if(tuple->data == NULL) {
      /* This case should not be possible to happen since the tuple is
	 concrete */
      PEISK_ASSERT(0,("Impossible case"));
      message->tuple.data=0;
      message->tuple.datalen=0;
    } else {
      /* We use the data field to mark if there is data available or
	 not, the actual data is concatenated to the end of the message */
      message->tuple.data=htonl(1);
      message->tuple.datalen=htonl(tuple->datalen);
      //memcpy((void*)(message+1),(void*)tuple->data,tuple->datalen);
    }
    /*printf("sending set remote tuple command to %d\n",tuple->owner);
    printf("Tuple->data: %s\n",tuple->data);
    printf("ML: %d\n",mimelength);
    */
    /*printf("Tuple that is pushed: \n");
      peisk_printTuple(tuple);*/

    peisk_getTupleName(tuple,message->tuple.keybuffer,sizeof(message->tuple.keybuffer));
    message->tuple.keyDepth=0;
    message->tuple.alloclen=0;

    //peisk_hexDump(message,len);
    //printf("%.3f Sending setRemoteTuple %s call\n",peisk_gettimef(),message->tuple.keybuffer);
    peisk_sendMessage(PEISK_PORT_SET_REMOTE_TUPLE,tuple->owner,
		      len,(void*)message,PEISK_PACKAGE_RELIABLE);
    return 0;
  }
}
void peisk_deleteTuple(int owner,const char *key) {
  PeisTuple tuple;
  peisk_initTuple(&tuple);
  peisk_setTupleName(&tuple,key);
  tuple.owner = owner;
  tuple.ts_expire[0] = PEISK_TUPLE_EXPIRE_NOW;
  tuple.ts_expire[1] = 0;
  tuple.data = "";
  tuple.datalen = 1;
  peisk_insertTuple(&tuple);
}

/*                  */
/* APPENDING TUPLES */
/*                  */
void peisk_appendTupleByAbstract(PeisTuple *proto,int difflen,const void *diff) {
  if(proto->owner == peiskernel.id || proto->owner == -1) peisk_appendLocalTuple(proto,difflen,diff);
  if(proto->owner != peiskernel.id || proto->owner == -1) peisk_sendAppendTupleRequest(proto,difflen,diff);
}
void peisk_appendStringTupleByAbstract(PeisTuple *proto,const char *value) {
  peisk_appendTupleByAbstract(proto,strlen(value)+1,value);
}

/*                */
/* GETTING TUPLES */
/*                */
PeisTuple *peisk_getTuple(int owner,const char *key,int flags) {
  PeisTuple prototype;
  PeisTuple *tuple;
  peisk_initAbstractTuple(&prototype);
  peisk_setTupleName(&prototype,key);

  if(flags & PEISK_KEEP_OLD) prototype.isNew=-1;
  else prototype.isNew=1;

  //printf("Trying to get tuple: %d.%s\n",owner,key);
  prototype.owner = owner;
  while(1) {
     tuple = peisk_getTupleByAbstract(&prototype);
     if(tuple || (!(flags&PEISK_BLOCKING))) break;
     /* \todo make the wait more efficient by not waiting if the tuple was updated. */
     peisk_waitOneCycle(1000);
     peisk_step();
  }
  //printf("getTuple %s -> %s\n",key,tuple?tuple->data:"<NULL pointer>"); fflush(stdout);
  return tuple;
}
PeisTuple *peisk_getTupleByAbstract(PeisTuple *prototype) {
  char fullname[PEISK_KEYLENGTH+5];
  PeisTuple *tuple;
  int errno;
  int timenow[2];

  int i;
  for(i=0;i<prototype->keyDepth;i++)
    if(prototype->keys[i] == NULL) break; 
  if(prototype->owner == -1 || prototype->keyDepth == -1 || i != prototype->keyDepth) {
    fprintf(stderr,"Attempting to use peisk_getTupleByAbstract (SINGULAR!) with a tuple with abstract value for key and/or owner. Prototype was:\n");
    peisk_printTuple(prototype);
    printf("\nbad programmer! this is not allowed!!!\n");
    exit(0);    
  }

  peisk_getTupleFullyQualifiedName(prototype,fullname,sizeof(fullname));
  errno=peisk_hashTable_getValue(peisk_tuples_primaryHT,fullname,
				 (void**) (void*) &tuple);
  if(errno) {
    peisk_tuples_errno=PEISK_TUPLE_BADKEY;
    return NULL;
  }

  if(tuple) {
    /* This test is to filter out expired tuples pending to
       be deleted */
    peisk_gettime2(&timenow[0],&timenow[1]);
    if(tuple->ts_expire[0] != 0 &&
       (tuple->ts_expire[0] < timenow[0] ||
	(tuple->ts_expire[0] == timenow[0] &&
	 tuple->ts_expire[1] < timenow[1])))
      return NULL;

    if(prototype->isNew != -1 && prototype->isNew != tuple->isNew) 
      return NULL;
    tuple->isNew=0;
    return tuple;
  } else
    return NULL;
}

int peisk_getTuples(int owner, const char *key,PeisTupleResultSet *rs) {
  PeisTuple prototype;
  peisk_initAbstractTuple(&prototype);
  peisk_setTupleName(&prototype,key);
  prototype.isNew=-1;
  prototype.owner = owner;
  return peisk_getTuplesByAbstract(&prototype,rs);
}
int peisk_getTuplesByAbstract(PeisTuple *prototype,PeisTupleResultSet *rs) {
  /* \todo TODO - make a more efficient implementation that does not loop
     through all tuples. Need to use multiple hashes using different
     subkeys */
  PeisHashTableIterator primaryIter;
  PeisTuple *tuple;
  char *fullname;
  int cnt=0;
  int timenow[2];
  peisk_gettime2(&timenow[0],&timenow[1]);

  /* Loop through all tuples in primary tuple hashtable and test if
     they match the prototype, if so then add to result set */
  peisk_hashTableIterator_first(peisk_tuples_primaryHT,&primaryIter);
  for(;peisk_hashTableIterator_next(&primaryIter);) {
    peisk_hashTableIterator_value_generic(&primaryIter,&fullname,&tuple);
    if(peisk_compareTuples(tuple,prototype) == 0 &&
       /* This second test is to filter out expired tuples pending to
	  be deleted */
       (tuple->ts_expire[0] == 0 || tuple->ts_expire[0] > timenow[0] ||
	(tuple->ts_expire[0] == timenow[0] && tuple->ts_expire[1] > timenow[1]))) {
      peisk_resultSetAdd(rs,tuple);
      tuple->isNew = 0;
      cnt++;
    } 
  }
  /* Finally, return number of found matching tuples */
  return cnt;
}



/* PREDICATES */
int peisk_tupleExists(const char *key) {
  PeisTuple prototype;
  PeisTuple *tuple;
  peisk_initAbstractTuple(&prototype);
  peisk_setTupleName(&prototype,key);
  prototype.isNew=-1;
  prototype.owner = peisk_id;
  tuple = peisk_getTupleByAbstract(&prototype);
  if(tuple) return 1;
  else return 0;
}
int peisk_hasSubscriberByAbstract(PeisTuple *prototype) {
  PeisHashTableIterator iter;
  char *key;
  PeisSubscriber *subscriber;

  for(peisk_hashTableIterator_first(peisk_subscribers_primaryHT,&iter);
      peisk_hashTableIterator_next(&iter);)
    if(peisk_hashTableIterator_value_generic(&iter, &key, &subscriber)) {
      /* See if this subscriber matches this tuple */
      if(peisk_compareTuples(subscriber->prototype,prototype) == 0) {
	/* Yes, this subscriber is more general */
	return 1;
      }
    }
  return 0;
}

/** Wrapper to tests if anyone is currently subscribed to the given
    (full) key in our local tuplespace. Wrapper for backwards
    compatability. */
int peisk_hasSubscriber(const char *key) {
  PeisTuple prototype;
  peisk_initTuple(&prototype);
  peisk_setTupleName(&prototype,key);
  return peisk_hasSubscriberByAbstract(&prototype);
}
int peisk_tupleIsAbstract(PeisTuple *tuple) {
  int i;

  if(tuple->owner == -1) return 1;
  if(tuple->creator == -1) return 1;
  /* We don't care for ts_write, it is by definition uninitialized,
     abstract or an _outputed_ concrete tuple */
  if(tuple->ts_user[0] == -1 || tuple->ts_user[1] == -1) return 1;
  if(tuple->ts_expire[0] == -1 || tuple->ts_expire[1] == -1) return 1;
  if(tuple->keyDepth == -1) return 1;
  if(tuple->encoding == PEISK_ENCODING_WILDCARD) return 1;
  if(tuple->mimetype == NULL) return 1;
  for(i=0;i<tuple->keyDepth;i++) 
    if(tuple->keys[i] == NULL) return 1;
  if(tuple->data == NULL) return 1;
  return 0;
}

#define PK_CMP(A,B,W) {if((A) != W && (B) != W) { if((A) < (B)) return -1; if((A) > (B)) return 1; }}
int peisk_compareTuples(PeisTuple *tuple1,PeisTuple *tuple2) {
  int i,ret;
  PK_CMP(tuple1->owner,tuple2->owner,-1);
  PK_CMP(tuple1->creator,tuple2->creator,-1);
  PK_CMP(tuple1->ts_write[0],tuple2->ts_write[0],-1);
  PK_CMP(tuple1->ts_write[1],tuple2->ts_write[1],-1);
  PK_CMP(tuple1->ts_user[0],tuple2->ts_user[0],-1);
  PK_CMP(tuple1->ts_user[1],tuple2->ts_user[1],-1);
  PK_CMP(tuple1->ts_expire[0],tuple2->ts_expire[0],-1);
  PK_CMP(tuple1->ts_expire[1],tuple2->ts_expire[1],-1);
  PK_CMP(tuple1->encoding,tuple2->encoding,PEISK_ENCODING_WILDCARD);
  PK_CMP(tuple1->keyDepth,tuple2->keyDepth,-1);
  /** \todo think about how the isNew part of a tuple should be treated
     when doing comparisions */
  PK_CMP(tuple1->isNew, tuple2->isNew, -1);
  if(tuple1->mimetype && tuple2->mimetype && strcasecmp(tuple1->mimetype,tuple2->mimetype) != 0) return 0;

  for(i=0;i<7;i++)
    if(tuple1->keys[i] != NULL && tuple2->keys[i] != NULL) {
      ret=strcasecmp(tuple1->keys[i],tuple2->keys[i]);
      if(ret != 0) return ret;
    }
  if(tuple1->data != NULL && tuple2->data != NULL) {
    if(tuple1->datalen < tuple2->datalen) return -1;
    if(tuple1->datalen > tuple2->datalen) return 1;
    ret = strncasecmp(tuple1->data,tuple2->data,tuple1->datalen);
    if(ret != 0) return ret;
  }
  return 0;
}

#define PK_GEN(A,B,W) {if((A) != W) { if((A) != (B)) return 0; }}
int peisk_isGeneralization(PeisTuple *tuple1, PeisTuple *tuple2) {
  int i,ret;
  PK_GEN(tuple1->owner,tuple2->owner,-1);
  PK_GEN(tuple1->creator,tuple2->creator,-1);
  PK_GEN(tuple1->ts_write[0],tuple2->ts_write[0],-1);
  PK_GEN(tuple1->ts_write[1],tuple2->ts_write[1],-1);
  PK_GEN(tuple1->ts_user[0],tuple2->ts_user[0],-1);
  PK_GEN(tuple1->ts_user[1],tuple2->ts_user[1],-1);
  PK_GEN(tuple1->ts_expire[0],tuple2->ts_expire[0],-1);
  PK_GEN(tuple1->ts_expire[1],tuple2->ts_expire[1],-1);
  PK_GEN(tuple1->encoding,tuple2->encoding,PEISK_ENCODING_WILDCARD);
  PK_GEN(tuple1->keyDepth,tuple2->keyDepth,-1);
  /** \todo think about how the isNew part of a tuple should be treated
     when doing comparisions */
  PK_GEN(tuple1->isNew, tuple2->isNew, -1);
  if(tuple1->mimetype && (!tuple2->mimetype || strcasecmp(tuple1->mimetype,tuple2->mimetype) != 0)) return 0;

  for(i=0;i<7;i++)
    if(tuple1->keys[i] != NULL) {
      if(tuple2->keys[i] == NULL) return 0;
      else {
	ret = strcasecmp(tuple1->keys[i],tuple2->keys[i]);
	if(ret != 0) return 0;
      }
    }
  if(tuple1->data != NULL) {
    if(tuple2->data == NULL) return 0;
    else {
      if(tuple1->datalen < tuple2->datalen) return 0;
      if(tuple1->datalen > tuple2->datalen) return 0;
      ret=strncasecmp(tuple1->data,tuple2->data,tuple1->datalen);
      if(ret != 0) return 0;	
    }
  }
  return 1;  
}
#define PK_EQ(A,B) {if((A) != (B)) return 0;}
int peisk_isEqual(PeisTuple *tuple1, PeisTuple *tuple2) {
  int i,ret;
  PK_EQ(tuple1->owner,tuple2->owner);
  PK_EQ(tuple1->creator,tuple2->creator);
  PK_EQ(tuple1->ts_write[0],tuple2->ts_write[0]);
  PK_EQ(tuple1->ts_write[1],tuple2->ts_write[1]);
  PK_EQ(tuple1->ts_user[0],tuple2->ts_user[0]);
  PK_EQ(tuple1->ts_user[1],tuple2->ts_user[1]);
  PK_EQ(tuple1->ts_expire[0],tuple2->ts_expire[0]);
  PK_EQ(tuple1->ts_expire[1],tuple2->ts_expire[1]);
  PK_EQ(tuple1->encoding,tuple2->encoding);  
  PK_EQ(tuple1->keyDepth,tuple2->keyDepth);
  if(!tuple1->mimetype || !tuple2->mimetype || strcasecmp(tuple1->mimetype,tuple2->mimetype) != 0) return 0;

  /** \todo think about how the isNew part of a tuple should be treated
     when doing comparisions */
  PK_EQ(tuple1->isNew, tuple2->isNew);
  for(i=0;i<7;i++)
    if(tuple1->keys[i] == NULL) { if(tuple2->keys[i] != NULL) return 0; }
    else if(tuple2->keys[i] == NULL) { if(tuple1->keys[i] == NULL) return 0; }
    else {
      ret=strcasecmp(tuple1->keys[i],tuple2->keys[i]);
      if(ret != 0) return 0;	
    }
  if(tuple1->data == NULL) { if(tuple2->data != NULL) return 0; }
  else if(tuple2->data == NULL) { if(tuple1->data != NULL) return 0; }
  else {
    if(tuple1->datalen < tuple2->datalen) return 0;
    if(tuple1->datalen > tuple2->datalen) return 0;
    ret=strncasecmp(tuple1->data,tuple2->data,tuple1->datalen);
    if(ret != 0) return 0;
  }
  return 1;    
}

/*                             */
/* SUBSCRIPTIONS AND CALLBACKS */
/*                             */
PeisCallbackHandle peisk_registerTupleCallback(int owner,const char *key,void *userdata,PeisTupleCallback *fn) {
  return peisk_registerTupleGenericCallback(owner,key,userdata,fn,PEISK_CALLBACK_CHANGED);
}
PeisCallbackHandle peisk_registerTupleDeletedCallback(int owner,const char *key,void *userdata,PeisTupleCallback *fn) {
  return peisk_registerTupleGenericCallback(owner,key,userdata,fn,PEISK_CALLBACK_DELETED);
}
int peisk_unregisterTupleCallback(PeisCallbackHandle handle) {
  PeisCallback *callback;
  int errno;
  char key[256];
  
  callback = peisk_findCallbackHandle(handle);
  if(!callback) return peisk_tuples_errno;

  peisk_freeTuple(callback->prototype);
  free(callback);
  snprintf(key,sizeof(key),"%d",handle);
  errno=peisk_hashTable_remove(peisk_callbacks_primaryHT,key);
  if(errno) {
    peisk_tuples_errno=PEISK_TUPLE_HASHTABLE_ERROR;
    return peisk_tuples_errno;
  }
  return 0;
}
PeisSubscriberHandle peisk_subscribe(int owner,const char *key) {
  /* We create a new template tuple and initalize it with the given
  parameter values. It is safe to allocate it on the stack since it will
  be copied anyway before inserted into the subscribers hashtable. */
  PeisTuple tuple;
  peisk_initAbstractTuple(&tuple);
  tuple.owner=owner;
  peisk_setTupleName(&tuple,key);
  /*printf("performing old-style subscribe on: "); peisk_printTuple(&tuple); printf("\n");*/
  return peisk_subscribeByAbstract(&tuple);
}
PeisSubscriberHandle peisk_subscribeByAbstract(PeisTuple *tuple) {
  PeisSubscriber subscriber;

  /* \todo - create a version of subscribeByAbstract that has an
     explicit timeout parameter? */
  /* Refuse subscription if directed to ourselves (not neccessary to
     subscribe to get the latest data) */
  /*if(tuple->owner == peiskernel.id) return 0;*/
  /* Note subscriptions to ourselves are now allowed in order to use
     meta tuples (the callback functions need some data to operate
     on!) */

  subscriber.prototype = tuple;
  subscriber.handle = peisk_nextSubscriberHandle++;
  subscriber.expire = -1.0;
  subscriber.subscriber = peiskernel.id;
  subscriber.isMeta = 0;

  PeisSubscriber *insertedSubscriber;
  insertedSubscriber = peisk_insertSubscriber(&subscriber);
  if(insertedSubscriber) return insertedSubscriber->handle;
  else return 0;
}

int peisk_unsubscribe(PeisSubscriberHandle handle) {
  return peisk_rawUnsubscribe(handle,1);
}
void peisk_periodic_resendSubscriptions(void *data) {
  PeisSubscriber *subscriber;
  PeisHashTableIterator iter;
  char *key;

  /* Send updated request for all local subscriptions */
  peisk_hashTableIterator_first(peisk_subscribers_primaryHT,&iter);
  for(;peisk_hashTableIterator_next(&iter);) {
    peisk_hashTableIterator_value_generic(&iter,&key,&subscriber);      
    if(subscriber->subscriber == peiskernel.id) {
      /* Send subscription notification to others */
      peisk_sendSubscribeMessage(subscriber,0);
    }
  }
}
int peisk_reloadSubscription(PeisSubscriberHandle handle) {
  PeisSubscriber *subscriber;
  char key[256];
  int errno;
  
  snprintf(key,sizeof(key),"%d",handle);
  errno=peisk_hashTable_getValue(peisk_subscribers_primaryHT,key,
				 (void**)(void*)&subscriber);
  if(errno) {
    peisk_tuples_errno = PEISK_TUPLE_BAD_HANDLE;
    return peisk_tuples_errno;
  }
  peisk_sendSubscribeMessage(subscriber,1);
  return 0;
}

/*            */
/* RESULTSETS */
/*            */
PeisTupleResultSet *peisk_createResultSet() {
  /* TODO - add a backup list of resultSets that can be used instead of
     allocating/freeing lots of tuple */
  PeisTupleResultSet *rs=(PeisTupleResultSet*)malloc(sizeof(PeisTupleResultSet));
  if(!rs) { 
    peisk_tuples_errno=PEISK_TUPLE_OUT_OF_MEMORY;
    return NULL;
  }
  rs->maxTuples=PEISK_RESULTSET_DEFAULT_MAX_TUPLES;
  rs->index=-1;
  rs->nTuples=0;
  rs->tuples=malloc(sizeof(PeisTuple*)*rs->maxTuples);
  if(!rs->tuples) { 
    free(rs);
    peisk_tuples_errno=PEISK_TUPLE_OUT_OF_MEMORY;
    return NULL;
  }
  return rs;
}
void peisk_deleteResultSet(PeisTupleResultSet *rs) {
  PEISK_ASSERT(rs&&rs->maxTuples>0,("Broken resultSet\n")); 
  if(!rs||rs->maxTuples<=0) return;
  if(rs->tuples) free(rs->tuples);
  rs->maxTuples=0;
  free(rs);
}
void peisk_resultSetFirst(PeisTupleResultSet *rs) { 
  PEISK_ASSERT(rs&&rs->maxTuples>0,("Broken resultSet\n"));
  if(!rs||rs->maxTuples<=0) return;
  rs->index=-1; 
}
void peisk_resultSetReset(PeisTupleResultSet *rs) { 
  PEISK_ASSERT(rs&&rs->maxTuples>0,("Broken resultSet\n"));
  if(!rs||rs->maxTuples<=0) return;
  rs->nTuples=0;
  rs->index=-1;
}
int peisk_resultSetNext(PeisTupleResultSet *rs) {
  if(rs->index >= rs->nTuples) return 0;
  rs->index++;
  return rs->index < rs->nTuples;
}
int peisk_resultSetIsEmpty(PeisTupleResultSet *rs) { return rs->index+1 >= rs->nTuples; }
PeisTuple *peisk_resultSetValue(PeisTupleResultSet *rs) {
  if(rs->index < 0) { 
    peisk_tuples_errno=PEISK_TUPLE_INVALID_INDEX; 
    fprintf(stderr,"Warning, invalid index when dereferencing resultset (too small).\nYou must use Next before accessing first value\n");
    return NULL;
  }
  if(rs->index >= rs->nTuples) {
    peisk_tuples_errno=PEISK_TUPLE_INVALID_INDEX; 
    return NULL;
  }
  return rs->tuples[rs->index];
}
int peisk_resultSetAdd(PeisTupleResultSet *rs,PeisTuple *tuple) {
  if(rs->nTuples >= rs->maxTuples) {
    rs->maxTuples += 64;
    PeisTuple **pnt = (PeisTuple**) realloc((void*)rs->tuples, sizeof(PeisTuple*)*rs->maxTuples);
    if(!pnt) return PEISK_TUPLE_OUT_OF_MEMORY;
    else rs->tuples=pnt;
  }
  rs->tuples[rs->nTuples++]=tuple;
  /*printf("added: "); peisk_printTuple(tuple);  printf("\n");*/
  return 0;
}

/************************/
/* DEFAULT TUPLE VALUES */
/************************/
void peisk_setDefaultStringTuple(const char *key,const char *value) {
  if(!peisk_tupleExists(key)) peisk_setStringTuple(key,value);
}
void peisk_setDefaultTuple(const char *key,int datalen, void *data,const char *mimetype, int encoding) {
  if(!peisk_tupleExists(key)) peisk_setTuple(key,datalen,data,mimetype,encoding);
}

void peisk_setDefaultMetaTuple(const char *key,int datalen,const void *data,const char *mimetype, int encoding) {
  char *keys[7], buf[256], buf2[256], *c;
  int i,j;
  if(!peisk_tupleExists(key)) peisk_setTuple(key,datalen,data,mimetype,encoding);
  /* Create the corresponding meta keyname */
  strcpy(buf,key);
  keys[0]=buf;
  for(i=0,c=buf;*(++c);) if(*c == '.') { *c=0; keys[++i]=c+1; }
  for(j=0,c=buf2;j<i;j++) {
    sprintf(c,"%s.",keys[j]);
    c+=strlen(c);
  }
  sprintf(c,"mi-%s",keys[i]);
  printf("%s becomes: %s\n",key,buf2); // MB
  sprintf(buf,"(META %d %s)",peisk_id,key);
  if(!peisk_tupleExists(buf2)) peisk_setStringTuple(buf2,buf);
}
void peisk_setDefaultMetaStringTuple(const char *key,const char *value) {
  peisk_setDefaultMetaTuple(key,strlen(value)+1,value,"text/plain",PEISK_ENCODING_ASCII);
}

/***************/
/* META TUPLES */
/***************/

int peisk_parseMetaTuple(PeisTuple *meta,int *owner,char *key) {
  int s;

  if(!meta) return 0;
  if(!meta->data) return 0;
  if(sscanf(meta->data,"( META %d %255s )",owner,key) == 2) {
    s=strlen(key);
    if(s>0 && key[s-1] == ')') key[s-1]=0; 
    return 1;
  }
  else {
    return 0;
  }
}

PeisSubscriberHandle peisk_subscribeIndirectlyByAbstract(PeisTuple *M) {
  PeisSubscriberHandle handle = peisk_subscribeByAbstract(M);
  if(!handle) return 0;
  PeisSubscriber *subscriber = peisk_findSubscriberHandle(handle);
  PEISK_ASSERT(subscriber,("Error in finding subscriber corresponding to handle %d",handle));
  subscriber->isMeta=1;
  subscriber->metaSubscription=0;
  subscriber->metaCallback = 
    peisk_registerTupleCallbackByAbstract(subscriber->prototype,(void*) subscriber,
					  peisk_metaSubscriptionCallback);					

  /* If the meta tuple already exists, then make the initial
     subscription to it */
  PeisTuple *meta =  peisk_getTupleByAbstract(M);
  if(meta && meta->data) peisk_metaSubscriptionCallback(meta,subscriber);
  return 0;
}

PeisTuple *peisk_getTupleIndirectlyByAbstract(PeisTuple *metaProto) {
  char key[256];
  int owner=-1;

  /*printf("findIndirect "); peisk_printTuple(metaProto);
    printf("\n"); */

  int isNew=metaProto->isNew;
  metaProto->isNew=-1;
  PeisTuple *meta;
  meta=peisk_getTupleByAbstract(metaProto);
  /*printf("found "); peisk_printTuple(meta); printf("\n"); */
  if(!meta) {
    peisk_tuples_errno=PEISK_TUPLE_INVALID_META;
    return NULL;
  }
  /* If the reference is new, then any old value of the tuple will do */
  if(meta->isNew) isNew=-1;

  if(peisk_parseMetaTuple(meta,&owner,key) && owner != -1) {
    /*printf("parsed ok\n");*/
    PeisTuple prototype;
    peisk_initAbstractTuple(&prototype);
    peisk_setTupleName(&prototype,key);
    /* Use the isNew flag given by input metaProto */
    prototype.isNew=isNew;
    prototype.owner = owner;
    return peisk_getTupleByAbstract(&prototype);
  } else {
    /*printf("failed to parse\n");*/
    peisk_tuples_errno=PEISK_TUPLE_INVALID_META;
    return NULL;
  }
}

int peisk_initIndirectTuple(PeisTuple *metaProto,PeisTuple *prototype) {
  char key[256];
  int owner=-1;
  PeisTuple *meta=peisk_getTupleByAbstract(metaProto);
  if(!meta) {
    peisk_tuples_errno=PEISK_TUPLE_INVALID_META; 
    return peisk_tuples_errno;
  }
  if(peisk_parseMetaTuple(meta,&owner,key) && owner != -1) {
    peisk_initTuple(prototype);
    peisk_setTupleName(prototype,key);
    prototype->owner = owner;    
    return 0;
  } else {
    peisk_tuples_errno=PEISK_TUPLE_INVALID_META;
    return peisk_tuples_errno;
  }
}

PeisSubscriberHandle peisk_subscribeIndirectly(int metaOwner,const char *metaKey) {
  PeisTuple prototype;
  if(metaOwner == -1) {
    fprintf(stderr,"You are trying to subscrbie *indirectly* to metaowner -1... please, don't do this!\n");
    exit(0);
  }
  peisk_initAbstractTuple(&prototype);
  peisk_setTupleName(&prototype,metaKey);  
  prototype.owner = metaOwner;
  return peisk_subscribeIndirectlyByAbstract(&prototype);
}

PeisTuple *peisk_getTupleIndirectly(int metaOwner,const char *metaKey,int flags) {
  char key[256];
  int owner=-1;

  while(1) {
    PeisTuple *meta = peisk_getTuple(metaOwner,metaKey,PEISK_KEEP_OLD|(flags&PEISK_BLOCKING));
    
    if(!meta) {
      /*printf("no such meta tuple found\n");*/
      peisk_tuples_errno=PEISK_TUPLE_INVALID_META;
      return NULL;
    }
    if(peisk_parseMetaTuple(meta,&owner,key) && owner != -1) {
      return peisk_getTuple(owner,key,flags);
    } else if(!(flags&PEISK_BLOCKING))  {
      peisk_tuples_errno=PEISK_TUPLE_INVALID_META;
      return NULL;
    }  
    peisk_waitOneCycle(1000);
  }
}

int peisk_setStringTupleIndirectly(int metaOwner,const char *metaKey,const char *value) {
  char key[256];
  int owner=-1;
  PeisTuple *meta = peisk_getTuple(metaOwner,metaKey,PEISK_KEEP_OLD);
  if(!meta) {
    peisk_tuples_errno=PEISK_TUPLE_INVALID_META;
    return peisk_tuples_errno;
  }
  if(peisk_parseMetaTuple(meta,&owner,key) && owner != -1) {
    peisk_setRemoteTuple(owner,key,strlen(value)+1,value,"text/plain",PEISK_ENCODING_ASCII);
    return 0;
  } else {
    peisk_tuples_errno=PEISK_TUPLE_INVALID_META;
    return peisk_tuples_errno;
  }
}

int peisk_isMetaTuple(int metaOwner,const char *metaKey) {
  PeisTuple *meta = peisk_getTuple(metaOwner,metaKey,PEISK_KEEP_OLD);
  char key[256]; 
  int owner=-1;
  if(meta && strcasecmp(meta->mimetype,"x-peis/metatuple") == 0 && peisk_parseMetaTuple(meta,&owner,key) && owner != -1)
    return 1;
  else return 0;  
}
void peisk_declareMetaTuple(int metaOwner,const char *metaKey) {
  if(!peisk_getTuple(metaOwner,metaKey,PEISK_KEEP_OLD)) peisk_setMetaTuple(metaOwner,metaKey,-1,"NULL");
}
void peisk_setMetaTuple(int metaOwner,const char *metaKey,int realOwner,const char *realKey) {
  char str[256];
  snprintf(str,sizeof(str),"(META %d %s)",realKey?realOwner:-1,realKey?realKey:"NULL");
  //printf("(META %d %s)",realKey?realOwner:-1,realKey?realKey:"NULL");
  peisk_setRemoteTuple(metaOwner,metaKey,strlen(str)+1,str,"x-peis/metatuple",PEISK_ENCODING_ASCII);
}

/***********************/
/*  UTILITY FUNCTIONS  */
/***********************/

int peisk_findOwner(char *key) {
  PeisTuple prototype;
  PeisTupleResultSet *rs = peisk_createResultSet();
  printf("find owner %s called\n",key);

  while(1) {
    peisk_initAbstractTuple(&prototype);
    peisk_setTupleName(&prototype,key);
    prototype.isNew=-1;
    prototype.owner=-1;
    if(peisk_getTuplesByAbstract(&prototype,rs) && peisk_resultSetNext(rs)) {      
      PeisTuple *tuple=peisk_resultSetValue(rs);
      peisk_deleteResultSet(rs);
      //printf("owner %d found\n",tuple->owner);
      return tuple->owner;
    }
    peisk_waitOneCycle(1000);
    peisk_step();
  }
  return -1; /* Impossible case */  
}
void peisk_printTuple(PeisTuple *tuple) {
  char fullname[256];

  peisk_getTupleFullyQualifiedName(tuple,fullname,512);
  printf("<%s, creator=%d ts_wr=(%d.%d) ts_us=(%d.%d) ts_ex=(%d.%d) new=%d mime=%s enc=%s data='%s'>",
	 fullname,tuple->creator,(int)tuple->ts_write[0],(int)tuple->ts_write[1],
	 (int)tuple->ts_user[0],(int)tuple->ts_user[1],
	 (int)tuple->ts_expire[0],(int)tuple->ts_expire[1],
	 tuple->isNew,tuple->mimetype?tuple->mimetype:"*",
	 tuple->encoding == PEISK_ENCODING_ASCII?"ascii":(tuple->encoding == PEISK_ENCODING_BINARY?"bin":"*"),
	 tuple->data);
}
void peisk_snprintTuple(char *buf,int len,PeisTuple *tuple) {
  char fullname[256];

  peisk_getTupleFullyQualifiedName(tuple,fullname,512);
  snprintf(buf,len,"<%s, creator=%d ts_wr=(%d.%d) ts_us=(%d.%d) ts_ex=(%d.%d) new=%d mime=%s data='%s'>",
	 fullname,tuple->creator,(int)tuple->ts_write[0],(int)tuple->ts_write[1],
	 (int)tuple->ts_user[0],(int)tuple->ts_user[1],
	 (int)tuple->ts_expire[0],(int)tuple->ts_expire[1],
	 tuple->isNew,tuple->mimetype?tuple->mimetype:"*",tuple->data);
}
