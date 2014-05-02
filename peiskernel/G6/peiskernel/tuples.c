/** \File tuples.c
    \brief Contains all internal services implementing the tuplespaces
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

#define pmin(x,y) ((x)<(y)?(x):(y))


/*! \page CompileWarnings
   General note about compiling with GCC -Wall. 
   If you get warnings about type-prunning, especially while casting things to (void **) it is most likely a bug - if not on your system then atleast on 64bit systems. 
*/


/******************************************************************************/
/*                                                                            */
/* SERVICE: Distributed Tuple Space                                           */
/*                                                                            */
/* STATUS                                                                     */
/*                                                                            */
/* PORTS:                                                                     */
/* VARIABLES: debugTuples                                                     */
/*                                                                            */
/*                                                                            */
/******************************************************************************/

/** Used to temporarly allow settings special tuples 
    (used internally by the tuplespace maintaninence only) */
int peisk_override_setTuple_restrictions=0;
/** Enable debugging info */
int peisk_debugTuples=0;
int peisk_tuples_errno;

/** Hashtable containing all created concrete tuples indexed by their
    fully qualified name. Eg "42.foo.boo" The value is a pointer to
    the specific tuple. This hashtable is used when access to all
    tuples are needed.  */
PeisHashTable *peisk_tuples_primaryHT;

/** TODO: Add other hashtables creating alternative indices for parts
    of tuple keys. */

/** Hashtable containing all registered callbacks indexed by by
    handle. The value is a pointer to the callback structure. */
PeisHashTable *peisk_callbacks_primaryHT;

/** Enumerates all created callbacks - used as their handle */
PeisCallbackHandle peisk_nextCallbackHandle;

/** TODO: Add other hashtables creating alternative indices for parts
    of callbacks */

/** Hashtable containing all registered subscribers indexed by
    handle. The value is a pointer to the subscriber structure. */
PeisHashTable *peisk_subscribers_primaryHT;

/** Unique enumeration for all subscription handles on this host */
int peisk_nextSubscriberHandle;

/** TODO: Add other hashtables creating alternative indices for parts
    of subscribers */

/** TODO: Each tuple stored in the tuplespace should have a list of
    all the relevant subscribers/callbacks. Need to recompute only
    when a tuple is first inserted into tuplespace. Update when new
    subscribers/callbacks are inserted. */

/** List to keep track of tuples with an explicit expiry date that
    should be (eventually) deleted */
PeisExpireList *peisk_expireList;
/** Free expiry nodes that can be reused when generating the list of
    tuples that should be (eventually) deleted. */
PeisExpireList *peisk_expireListFree;

/** A routing table, showing for each known host (index by ID) 
    if we have sent it all subscription messages interesting for it. 
    The return value NULL corresponds to false and non-NULL to true. 

    Invariants: 
    0: Initially the array is FALSE for each host.
    1: Periodically, if any host has the value false, the array is set to TRUE and all 
       relevant subscriptions to him are sent. 
    2: If any subscription message is failed, then array is set to FALSE
    3: When a new subscription is made, if array was FALSE it will still be false
    4: When a new subsciriotion is made, if array was TRUE it will still be true, 
       unless the new subscription failes (see 2). 
 */
PeisHashTable *peisk_hostGivenSubscriptionMessages;

/*                                               */
/* State variables - affects next created tuple. */
/*                                               */
int ts_user[2]={0,0};  

/** A failed <tuple,destination> pair that should be attempted to be retransmitted 
    at regular intervals */
typedef struct PeisFailedTuple {
  char key[PEISK_KEYLENGTH];
  int tries;
  int destination;
  struct PeisFailedTuple *next;
} PeisFailedTuple;

/** List of free struct PeisFailedTuple allocated since earlier. */
PeisFailedTuple *peisk_freeFailedTuples;
/** List of current struct PeisFailedTuple which should be attempted to be resent, or rejected */
PeisFailedTuple *peisk_failedTuples;

/*                                               */
/*         Other temporary variables             */
/*                                               */

/** Memory to temporarily store tuples when sending them over the
   network. Dynamically (re)allocated to fit the largest tuple sent so
   far. Warning - this might lead to memory fragmentation - change
   implementation if neccessary */
char *peisk_tupleBuffer=NULL;
/** Reflects the size of the temporary buffer peisk_tupleBuffer */
int peisk_tupleBufferSize=0;

PeisHashTable *peisk_mimetypes;

/*                                               */
/*                  Constants                    */
/*                                               */

/** An array of all readable strings corresponding to all valid error
    codes */
const char *peisk_tuple_errorStrings[PEISK_TUPLE_LAST_ERROR]={
  "Generic tuple error", "Invalid key name", "Buffer overflow", 
  "Tuple is abstract", "Out of memory", "Hashtable error", "Invalid handle", 
  "Invalid index", "Bad argument", "Invalid meta tuple", 
};

/*                                               */
/*                  Functions                    */
/*                                               */

char *peisk_getTupleBuffer(int size) {
  if(size <= peisk_tupleBufferSize) return peisk_tupleBuffer;
  else {
    free(peisk_tupleBuffer);
    peisk_tupleBuffer = (char*) malloc(size+1024);
    peisk_tupleBufferSize = size + 1024;
    return peisk_tupleBuffer;
  }
}

void peisk_tuples_initialize() {
  /* Setup hooks and periodic functions */
  peisk_registerHook(PEISK_PORT_SUBSCRIBE,peisk_hook_subscribe);
  peisk_registerHook(PEISK_PORT_UNSUBSCRIBE,peisk_hook_unsubscribe);
  peisk_registerHook(PEISK_PORT_PUSH_TUPLE,peisk_hook_pushTuple);
  peisk_registerHook(PEISK_PORT_SET_REMOTE_TUPLE,peisk_hook_setTuple);
  peisk_registerHook(PEISK_PORT_PUSH_APPENDED_TUPLE,peisk_hook_pushAppendedTuple);
  peisk_registerHook(PEISK_PORT_SET_APPEND_TUPLE,peisk_hook_setAppendTuple);

  peisk_registerPeriodic(PEISK_MANAGE_TUPLES_PERIOD,NULL,peisk_periodic_tuples);
  //peisk_registerPeriodic(PEISK_RESEND_SUBSCRIPTIONS_PERIOD,NULL,peisk_periodic_resendSubscriptions);
  peisk_registerPeriodic(PEISK_DEBUG_TUPLES_PERIOD,NULL,peisk_periodic_debugInfoTuples);
  peisk_registerPeriodic(PEISK_EXPIRE_TUPLES_PERIOD,NULL,peisk_periodic_expireTuples);

  /* Create all hashtables */
  peisk_tuples_primaryHT      = peisk_hashTable_create(PeisHashTableKey_String);
  peisk_callbacks_primaryHT   = peisk_hashTable_create(PeisHashTableKey_String);
  peisk_subscribers_primaryHT = peisk_hashTable_create(PeisHashTableKey_String);
  peisk_hostGivenSubscriptionMessages = peisk_hashTable_create(PeisHashTableKey_Integer);
  peisk_mimetypes             = peisk_hashTable_create(PeisHashTableKey_String);

  /* Setup list of expiring tuples */
  peisk_expireList = NULL;
  peisk_expireListFree = NULL;

  peisk_freeFailedTuples = NULL;
  peisk_failedTuples = NULL;

  /* Initialize default values for some variables */
  peisk_nextCallbackHandle=1;
  peisk_nextSubscriberHandle=1;
  peisk_tuples_errno=0;
}

const char *peisk_tuple_strerror(int error) { 
  if(error >= 1 && error <= PEISK_TUPLE_LAST_ERROR) return peisk_tuple_errorStrings[error-1];
  return "Unknown error code - not a valid tuple error?";
}

char *peisk_cloneMimetype(const char *mimetype) {
  void *val;
  if(peisk_hashTable_getValue(peisk_mimetypes, (void*)mimetype, &val) == 0)
    return (char*) val;
  else {
    char *d = strdup(mimetype);
    peisk_hashTable_insert(peisk_mimetypes, (void*) mimetype, d);
    return d;
  }
}

void peisk_appendLocalTuple(PeisTuple *proto,int difflen,const void *diff) {
  //printf("Appending %d bytes to local tuple\n",difflen);

  /* Go through ALL tuples that match the given prototype and do the
     following steps 

     1. Create the corresponding append message
     2. Send message to all hosts with matching subscriptions
     3. Update the tuple inside our tuplespace
     4. Trigger any local callbacks 

     Note. this is slightly inefficient in the strange case that we
     make an append update that updates many tuples that are sent to
     the same receivers. But this is an unusual use-case and we can
     live with it for now. 
  */
  char *fullname;
  PeisHashTableIterator primaryIter;
  PeisTuple *tuple;
  int timenow[2];
  peisk_gettime2(&timenow[0],&timenow[1]);

  PeisAppendTupleMessage *message;
  int len;
  char *key;

  /** \todo - optimize the appending of tuple so we only create the space for the
      message if it is actually needed to be sent. */
  len=sizeof(PeisAppendTupleMessage)+difflen;
  message = (PeisAppendTupleMessage*) peisk_getTupleBuffer(len);
  message->difflen = htonl(difflen);
  memcpy((void*)(message+1),diff,difflen);

  /*printf("\nAppend prototype:\n");
  peisk_printTuple(proto);
  printf("\n");
  */

  /* Iterate over each tuple that matches the prototype */
  peisk_hashTableIterator_first(peisk_tuples_primaryHT,&primaryIter);
  for(;peisk_hashTableIterator_next(&primaryIter);) {
    peisk_hashTableIterator_value_generic(&primaryIter,&fullname,&tuple);

    /*printf("Compare to:\n");
    peisk_printTuple(tuple);
    printf("=> %d\n",peisk_isGeneralization(proto,tuple));
    */
    if(peisk_isGeneralization(proto,tuple) != 0 
       /* This second test is to filter out expired tuples pending to
	  be deleted */
       ) {
      /*(tuple->ts_expire[0] == 0 || tuple->ts_expire[0] > timenow[0] ||
	(tuple->ts_expire[0] == timenow[0] && tuple->ts_expire[1] > timenow[1]))) {*/
      /*printf("Matches: \n");
      peisk_printTuple(tuple);
      */

      /* Only propagate information for tuples that we own */
      if(tuple->owner == peiskernel.id) {

	/* Create the append message to send to hosts */
	peisk_tuple_hton(tuple,&message->tuple);
	peisk_getTupleName(proto,message->tuple.keybuffer,sizeof(message->tuple.keybuffer));
	message->tuple.keyDepth=0;
	message->tuple.alloclen=0;
	message->difflen = htonl(difflen);

	/** \todo Allow sending push append tuple requests to include the mimetype? Or perhaps this is redundant? */

	/* Send message to all hosts that are still subscribed to the tuple */      
	PeisHashTableIterator subIter;
	char *key;
	PeisSubscriber *subscriber;
	for(peisk_hashTableIterator_first(peisk_subscribers_primaryHT,&subIter);
	    peisk_hashTableIterator_next(&subIter);)
	  if(peisk_hashTableIterator_value_generic(&subIter, &key, &subscriber)) {
	    if(subscriber->subscriber != peisk_id &&
	       peisk_compareTuples(tuple,subscriber->prototype) == 0) {
	      /* send message to this subscriber */
	      //printf("Sending update to %d\n",subscriber->subscriber);
	      peisk_sendMessage(PEISK_PORT_PUSH_APPENDED_TUPLE,subscriber->subscriber,
				len,(void*)message,PEISK_PACKAGE_RELIABLE);
	    }
	  }
      }    

      /* Update inside our tuplespace */
      int smartUpdate=tuple->encoding == PEISK_ENCODING_ASCII;
      //if(strlen(tuple->data)+1 == tuple->datalen) smartUpdate=1;

      /*printf("Updating inside our tuplespace. smartUpdate=%d\n",smartUpdate);
      printf("Data before update: %s\n",tuple->data);
      */
      if(tuple->alloclen < tuple->datalen + difflen) {
	tuple->data = realloc(tuple->data, tuple->datalen+difflen+(smartUpdate?-1:0));
	tuple->alloclen=tuple->datalen+difflen+(smartUpdate?-1:0);
      }
      memcpy(tuple->data+tuple->datalen+(smartUpdate?-1:0),diff,difflen);
      tuple->datalen += difflen +(smartUpdate?-1:0);
      tuple->appendSeqNo++;

      /*      printf("Data after update: %s\n",tuple->data); */

      /* Trigger any local callbacks that depend on this tuple */
      PeisHashTableIterator callIter;
      PeisCallback *callback;
      for(peisk_hashTableIterator_first(peisk_callbacks_primaryHT,&callIter);
	  peisk_hashTableIterator_next(&callIter);)
	if(peisk_hashTableIterator_value_generic(&callIter, &key, &callback)) {
	  /* See if this callback matches this tuple */
	  if(callback->type == PEISK_CALLBACK_CHANGED &&
	     peisk_compareTuples(tuple,callback->prototype) == 0) {
	    (callback->fn)(tuple,callback->userdata);
	  }
	}

    }
  }  
}

void peisk_sendAppendTupleRequest(PeisTuple *proto,int difflen,const void *diff) {
  PeisAppendTupleMessage *message;
  int len;
  PeisHashTableIterator hostIter;

  //printf("Sending append tuple request\n");

  len=sizeof(PeisAppendTupleMessage)+(proto->data?proto->datalen:0)+difflen;
  message = (PeisAppendTupleMessage*) peisk_getTupleBuffer(len);
  peisk_tuple_hton(proto,&message->tuple);
  /* We don't store any subkeys in the message to avoid any possible
     inconsistency problems, the full key is copied in the private
     part anyway (keybuffer). The same goes for keyDepth. */
  /* Copy the keyname from given tuple to the new message tuple */
  peisk_getTupleName(proto,message->tuple.keybuffer,sizeof(message->tuple.keybuffer));
  message->tuple.keyDepth=0;
  message->tuple.alloclen=0;
  message->difflen = htonl(difflen);

  /** \todo Allow send append tuple requests to set the mimetype */

  void *msgDiff;
  if(proto->data == NULL) {
    message->tuple.data=0;
    message->tuple.datalen=0;
    msgDiff=(void*)(message+1);
  } else {
    /* We use the data field to mark if there is data available or
       not, the actual data is concatenated to the end of the message */
    message->tuple.data=htonl(1);
    message->tuple.datalen=htonl(proto->datalen);
    memcpy((void*)(message+1),(void*)proto->data,proto->datalen);  
    msgDiff=((void*)(message+1))+proto->datalen;
  }
  /* Copy the data difference into the tuple */
  memcpy(msgDiff,diff,difflen);    

  if(proto->owner != -1) {
    //printf("Sending append tuple message to %d on port %d\n",proto->owner,PEISK_PORT_SET_REMOTE_TUPLE);
    peisk_sendMessage(PEISK_PORT_SET_APPEND_TUPLE,proto->owner,
		      len,(void*)message,PEISK_PACKAGE_RELIABLE);
  }
  else {
    peisk_hashTableIterator_first(peiskernel.hostInfoHT,&hostIter);
    for(;peisk_hashTableIterator_next(&hostIter);) {
      long long dest;
      PeisHostInfo *hostInfo;
      peisk_hashTableIterator_value_generic(&hostIter,&dest,&hostInfo);
      //printf("Sending append tuple message to %d\n",hostInfo->id);
      if(peisk_sendMessage(PEISK_PORT_SET_APPEND_TUPLE,hostInfo->id,
			   len,(void*)message,PEISK_PACKAGE_RELIABLE) != 0) {
	printf("Warning - failed to send append message to %d\n",hostInfo->id);
      }
    }
  }
  return;
}

int peisk_addToLocalSpace(PeisTuple *tuple) {
  char fullname[512];
  PeisTuple *oldTuple;

  PEISK_ASSERT(tuple != NULL, ("attempting to insert an empty tuple into local tuplespace\n"));

  //printf("AddToLocalSpace:"); peisk_printTuple(tuple); printf("\n");

  peisk_getTupleFullyQualifiedName(tuple,fullname,512);
  if(tuple->alloclen == 0 && tuple->datalen > 0) tuple->alloclen=tuple->datalen;

  //if(strcmp(tuple->keys[0],"kernel") != 0)
  //  printf("%.3f Adding %s to local space\n",peisk_gettimef(),fullname);
  
  if(!peisk_hashTable_getValue(peisk_tuples_primaryHT, 
			       fullname, (void**) (void*) &oldTuple)) {
    /* An old value existed for this tuple, reuse old tuple and memory if possible.
       Note that we _never_ free+allocate a tuple, this is since tuples 
       should never move in the memory space */
    if(oldTuple->alloclen < tuple->datalen) { 
      oldTuple->data = realloc(oldTuple->data, tuple->datalen);
      oldTuple->alloclen=tuple->datalen;
    }

    /* Increment sequence number whenever tuple is assigned a new value */
    if(tuple->seqno>0) oldTuple->seqno=tuple->seqno;
    else oldTuple->seqno++;
    oldTuple->appendSeqNo=0;

    /* No need to copy owner. 
       oldTuple->owner = tuple->owner; */
    oldTuple->creator = tuple->creator;
    oldTuple->ts_write[0] = tuple->ts_write[0];
    oldTuple->ts_write[1] = tuple->ts_write[1];
    oldTuple->ts_user[0] = tuple->ts_user[0];
    oldTuple->ts_user[1] = tuple->ts_user[1];

    /* Handle the update (if any) of the ts_expire field, and
       remove/insert it into expire list */
    if(oldTuple->ts_expire[0] != tuple->ts_expire[0] ||
       oldTuple->ts_expire[1] != tuple->ts_expire[1]) {
      if(oldTuple->ts_expire[0] != 0) 
	peisk_expireListRemove(oldTuple);

      /* Update expire part must be done before new insertion into
	 expire list */
      oldTuple->ts_expire[0] = tuple->ts_expire[0];
      oldTuple->ts_expire[1] = tuple->ts_expire[1];

      if(tuple->ts_expire[0] != 0) 
	peisk_expireListAdd(oldTuple);
    }

    /* No need to copy the "keys" part */
    oldTuple->isNew=1;
    memcpy(oldTuple->data, tuple->data, tuple->datalen);   
    oldTuple->datalen=tuple->datalen;   
    oldTuple->mimetype = peisk_cloneMimetype(tuple->mimetype);
    tuple=oldTuple;    
  } else {
    /* No previous tuple existed, allocate a new one and enter it */
    tuple = peisk_cloneTuple(tuple);

    if(!tuple) return PEISK_TUPLE_OUT_OF_MEMORY;
    tuple->isNew=1;

    //tuple->seqno = 0;
    //tuple->appendSeqNo = 0;

    /* Inserts the tuple to the primary key hash table */
    peisk_hashTable_insert(peisk_tuples_primaryHT, fullname, (void*) tuple);

    /* Add to list of expiring tuples if neccessary */
    if(tuple->ts_expire[0] != 0) {
      peisk_expireListAdd(tuple);
    }

    /* TODO - add to other hashes here, update pointers to callbacks/subscribers etc. */

    /* If the inserted tuple belongs to us, update the "all-keys" tuple */
    if(tuple->owner == peiskernel.id) {
      peisk_generateAllKeysTuple();
    }
  }

  if(tuple->owner == peiskernel.id) peisk_alertSubscribers(tuple);
  peisk_alertCallbacks(tuple);
  return 0;
}

void peisk_alertSubscribers(PeisTuple *tuple) {
  PeisHashTableIterator iter;
  char *key;
  PeisSubscriber *subscriber;

  //printf("Alerting subscribers for: "); peisk_printTuple(tuple); printf("\n");

  /* Iterate over all SUBSCRIBERS */
  for(peisk_hashTableIterator_first(peisk_subscribers_primaryHT,&iter);
      peisk_hashTableIterator_next(&iter);)
    if(peisk_hashTableIterator_value_generic(&iter, &key, &subscriber)) {
      /*printf("stepping over subscriber\n");
      printf("tuple: \n"); peisk_printTuple(tuple); printf("\n");
      printf("prototype: \n"); peisk_printTuple(subscriber->prototype); printf("\n");
      */

      /* Unless it is one of our own subscriptions, 
	 see if this subscriber matches this tuple */
      if(subscriber->subscriber != peisk_id &&
	 peisk_compareTuples(tuple,subscriber->prototype) == 0) {
	/* Yes, this subscriber matched the tuple. Send message */
	/*printf("alertSubscribers: sending to %d, tuple:",subscriber->subscriber); 
	  peisk_printTuple(tuple); printf("\n");*/
	if(peisk_pushTuple(tuple,subscriber->subscriber) != 0)
	  peisk_insertFailedTuple(tuple,subscriber->subscriber);
      }
  }
}
void peisk_alertSubscriber(PeisSubscriber *subscriber) {
  PeisHashTableIterator iter;
  char *key;
  PeisTuple *tuple;

  /*printf("Alerting subscriber: %d\n",subscriber->subscriber);
  printf("Proto: "); peisk_printTuple(subscriber->prototype); printf("\n");
  */

  /* It is an error to try to alert ourselves */
  if(subscriber->subscriber == peisk_id) return;

  /* Iterate over all TUPLES */
  for(peisk_hashTableIterator_first(peisk_tuples_primaryHT,&iter);
      peisk_hashTableIterator_next(&iter);)
    if(peisk_hashTableIterator_value_generic(&iter, &key, &tuple)) {
      /* See if this subscriber matches this tuple */
      //printf("testing tuple: "); peisk_printTuple(tuple); printf("\n");

      if(tuple->owner == peiskernel.id &&         
	 peisk_compareTuples(tuple,subscriber->prototype) == 0) {
	/* Yes, this subscriber matched the tuple. Send message */
	//printf("tuple '%s' matched a subscriber %d\n",key,subscriber->subscriber);

	if(peisk_pushTuple(tuple,subscriber->subscriber) != 0)
	  peisk_insertFailedTuple(tuple,subscriber->subscriber);
      }
    }
}

void peisk_pushTupleAckHook(int success,int datalen,PeisPackage *package,void *userdata) {
  PeisPushTupleMessage *message;
  int dest;
  PeisHostInfo *hostInfo;
  PeisTuple sendTuple;
  char key[PEISK_KEYLENGTH];
  PeisTuple prototype;
  PeisTuple *tuple;

  dest = ntohl(package->header.destination);
  message = (PeisPushTupleMessage*) package->data;
  peisk_tuple_ntoh(&message->tuple,&sendTuple); 
  peisk_setTupleName(&sendTuple,message->tuple.keybuffer);
  peisk_getTupleName(&sendTuple,key,sizeof(key));

  if(sendTuple.owner != peisk_id) {
    PEISK_ASSERT(sendTuple.owner == peisk_id,
		 ("Attempting to push a tuple %d.%s that does not belong to us\n",sendTuple.owner,key));
  }

  if(success) {
    /*printf("Successfully sent %s to %d\n",key,dest);*/
  } else {
    /*printf("Failed to send %s to %d ...",key,dest);*/
    
    /* We are exploiting that the tuple pointer never changes, so this is a fast index into it */
    /*tuple = (PeisTuple*) userdata; */
    
    peisk_initAbstractTuple(&prototype);
    peisk_setTupleName(&prototype,key);
    prototype.isNew=-1;
    prototype.owner = peisk_id; 
    tuple = peisk_getTupleByAbstract(&prototype);

    if(!tuple) { 
      /*printf("failed, tuple does not exist any more\n");*/
      return;
    }
    /* If transmitted tuple has changed, then we do not need to retransmitt it again.
       (it will anyway be triggered by a later ackHook when the changed value has failed, if it fails).
    */
    if(tuple->seqno > sendTuple.seqno) {
      /*printf("ignore, there is a newer tuple (current is %d vs. old was %d)\n",tuple->seqno,sendTuple.seqno);*/
      return;
    }

    hostInfo = peisk_lookupHostInfo(dest);
    if(!hostInfo) {
      /*printf("failed, host is not known\n");*/
      peisk_insertFailedTuple(tuple,dest);
      return;
    }
    
    if(peisk_pushTuple(tuple, ntohl(package->header.destination)) != 0) {
      /*printf("inserting into failed tuples list\n");*/
      peisk_insertFailedTuple(tuple,dest);
    } /*else
	printf("Resend ok\n");*/
  }
}

int peisk_pushTuple(PeisTuple *tuple,int destination) {
  PeisPushTupleMessage message;
  char *buffer;
  int len,ret;
  
  char tmp[256];
  peisk_getTupleFullyQualifiedName(tuple,tmp,sizeof(tmp));

  /* If destination is self - nothing to do */
  /* Otherwise, send a PeisPushTupleMessage */
  if(destination == peiskernel.id) {
    printf("Warning, ignoring pushtuple message to ourselves\n");
    return -1; 
  }

  /*printf("pushTuple: Sending to %d, tuple:",destination); peisk_printTuple(tuple); printf("\n");*/

  /* Only the kernel which owns a tuple is suppose to propagate it. */
  if(tuple->owner != peiskernel.id) {
    printf("Attempting to propagate a tuple we shouldn't\n");
    return -1;
  }
  /*printf("Pushing a tuple\n");
  peisk_printTuple(tuple);
  */

  message.tuple.isNew = htonl(tuple->isNew);
  message.tuple.seqno = htonl(tuple->seqno);
  message.tuple.appendSeqNo = htonl(tuple->appendSeqNo);
  peisk_tuple_hton(tuple,&message.tuple);

  //printf("Pushing tuple w/ MT %s, len=%d, htonl=%d\n",tuple->mimetype,tuple->mimetype?strlen(tuple->mimetype):-1,message.tuple.mimetypeLength);

  /* Copy the keyname from given tuple to the new message tuple */
  ret=peisk_getTupleName(tuple,message.tuple.keybuffer,sizeof(message.tuple.keybuffer));
  PEISK_ASSERT(ret == 0, ("error setting name for PushTupleMessage. '%s'\n",peisk_tuple_strerror(ret)));

  int mimelength=tuple->mimetype?strlen(tuple->mimetype):0;
  int suffixLength = tuple->datalen + mimelength;
  len=sizeof(message)+suffixLength;
  buffer = peisk_getTupleBuffer(len);
  memcpy((void*)buffer, (void*) &message, sizeof(message));
  if(tuple->mimetype) memcpy((void*)buffer+sizeof(message), (void*) tuple->mimetype, mimelength);
  memcpy((void*)buffer+sizeof(message)+mimelength, (void*) tuple->data, tuple->datalen);

  /*printf("sending push tuple message to %d: ",destination);
  peisk_printTuple(tuple); printf("\n");
  printf("message: \n");
  peisk_hexDump(buffer,len);
  */

 
  with_ack_hook(peisk_pushTupleAckHook,(void*)tuple,{
      ret=peisk_sendMessage(PEISK_PORT_PUSH_TUPLE,destination, len, (void*) buffer, PEISK_PACKAGE_RELIABLE);  
    });
  return ret;
}

void peisk_alertCallbacks(PeisTuple *tuple) {
  PeisHashTableIterator iter;
  char *key;
  PeisCallback *callback;

  for(peisk_hashTableIterator_first(peisk_callbacks_primaryHT,&iter);
      peisk_hashTableIterator_next(&iter);)
    if(peisk_hashTableIterator_value_generic(&iter, &key, &callback)) {
      /* See if this callback matches this tuple */
      if(callback->type == PEISK_CALLBACK_CHANGED &&
	 peisk_compareTuples(tuple,callback->prototype) == 0) {
	(callback->fn)(tuple,callback->userdata);
      }
    }
}


void peisk_generateAllKeysTuple() {
  PeisHashTableIterator iter;
  char *key;
  PeisTuple *tuple;
  
  int ret, restr;
  char str[256];
  static int maxKeylen=1024;
  char *allKeys;
  int pos;

  /* Use peisk_tupleBufferSize as the starting value for maxKeylen? */
  if(maxKeylen < peisk_tupleBufferSize) maxKeylen = peisk_tupleBufferSize;
 peisk_generateAllKeysTuple_restart:
  allKeys=peisk_getTupleBuffer(maxKeylen);
  snprintf(allKeys,maxKeylen,"(");
  pos=strlen(allKeys);

  for(peisk_hashTableIterator_first(peisk_tuples_primaryHT,&iter);
      peisk_hashTableIterator_next(&iter);)
    if(peisk_hashTableIterator_value_generic(&iter, &key, &tuple)) {
      if(tuple->owner != peisk_id) continue;
      ret=peisk_getTupleName(tuple,str,sizeof(str)); str[255]=0;
      PEISK_ASSERT(ret == 0, ("Error getting name of tuple '%s', %s\n", str, peisk_tuple_strerror(ret)));
      snprintf(allKeys+pos,maxKeylen-pos,"%s ", str);
      pos += strlen(allKeys+pos);
      if(pos >= maxKeylen) {
	maxKeylen += 1024;
	/* TODO, we could just use a realloc here, atleast if we had
	   explicit control over the buffer which we don't */
	/* Yes, i know what i'm doing. This is one of those few rare
	   occasions when a GOTO is acceptable to use. The altnerative
	   here would be a recursive call, but C doesn't have tail
	   recursion... so that would not be any good idea */
	goto peisk_generateAllKeysTuple_restart;
      }
    }
  if(pos == 1) pos=2;
  allKeys[pos-1]=')';
  allKeys[pos]=0;  
  

  /* Before adding tuple to local tuplespace we must enable modifying
     internal tuples with the override variable. */
  restr=peisk_override_setTuple_restrictions;
  peisk_override_setTuple_restrictions=1;
  peisk_setStringTuple("kernel.all-keys",allKeys);  
  peisk_override_setTuple_restrictions=restr;
}

PeisCallbackHandle peisk_registerTupleCallbackByAbstract(PeisTuple *tuple,void *userdata,PeisTupleCallback *fn) {
  return peisk_registerAbstractTupleGenericCallback(tuple,userdata,fn,PEISK_CALLBACK_CHANGED);
}
PeisCallbackHandle peisk_registerTupleDeletedCallbackByAbstract(PeisTuple *tuple,void *userdata,PeisTupleCallback *fn) {
  return peisk_registerAbstractTupleGenericCallback(tuple,userdata,fn,PEISK_CALLBACK_DELETED);
}

PeisCallbackHandle peisk_registerAbstractTupleGenericCallback(PeisTuple *tuple,void *userdata,PeisTupleCallback *fn,int type) {
  PeisCallback *callback;
  callback=(PeisCallback*) malloc(sizeof(PeisCallback));
  if(!callback) {
    printf("PEISK Error - out of memory when registering new callback\n");
    exit(0);
  }
  callback->fn = fn;
  callback->userdata = userdata;
  callback->handle = peisk_nextCallbackHandle++;
  callback->prototype = peisk_cloneTuple(tuple);
  if(callback->prototype == NULL) return 0;  
  callback->type = type;

  /** TODO - set indexed key in another callbacks HT to be the (fully)
      qualified name. The hashtable must be changed to allow multiple
      entries with the same key. */
  char key[256];
  snprintf(key,sizeof(key),"%d",callback->handle);
  if(peisk_hashTable_insert(peisk_callbacks_primaryHT,key,(void*)callback)) {
    peisk_tuples_errno=PEISK_TUPLE_HASHTABLE_ERROR;
    return 0;
  }
  return callback->handle;
}


PeisCallbackHandle peisk_registerTupleGenericCallback(int owner,const char *key,void *userdata,PeisTupleCallback *fn,int type) {
  /* We create a new template tuple and initalize it with the given
  parameter values. It is safe to allocate it on the stack since it will
  be copied anyway before inserted into the callbacks hashtable. */
  PeisTuple tuple;
  peisk_initAbstractTuple(&tuple);
  tuple.owner=owner;
  peisk_setTupleName(&tuple,key);
  return peisk_registerAbstractTupleGenericCallback(&tuple,userdata,fn,type);
}

PeisCallback *peisk_findCallbackHandle(PeisCallbackHandle handle) {
  PeisCallback *callback;
  char key[256];
  int errno;

  snprintf(key,sizeof(key),"%d",handle);
  errno=peisk_hashTable_getValue(peisk_callbacks_primaryHT,key,
				 (void**) (void*) &callback);
  if(errno) {
    peisk_tuples_errno=PEISK_TUPLE_BAD_HANDLE;
    return NULL;
  }
  return callback;
}

PeisSubscriber *peisk_findSubscriberHandle(PeisSubscriberHandle handle) {
  PeisSubscriber *subscriber;
  char key[256];
  int errno;

  snprintf(key,sizeof(key),"%d",handle);
  errno=peisk_hashTable_getValue(peisk_subscribers_primaryHT,key,
				 (void**) (void*) &subscriber);
  if(errno) {
    peisk_tuples_errno=PEISK_TUPLE_BAD_HANDLE;
    return NULL;
  }
  return subscriber;
}

void peisk_insertFailedTuple(PeisTuple *tuple,int destination) {
  PeisFailedTuple *failed;
  char key[PEISK_KEYLENGTH];

  peisk_getTupleName(tuple,key,sizeof(key));

  PEISK_ASSERT(tuple->owner == peiskernel.id,
	       ("Attempting to mark as failed a tuple %d.%s not belonging to us\n",
		tuple->owner,key));

  /* First, check that this tuple does not already exist in list of failed tuples */
  for(failed=peisk_failedTuples;failed;failed=failed->next) 
    if(strcasecmp(key,failed->key) == 0) return;
     
  if(peisk_freeFailedTuples) {
    failed = peisk_freeFailedTuples;
    peisk_freeFailedTuples = peisk_freeFailedTuples->next; 
  } else {
    failed = (PeisFailedTuple*) malloc(sizeof(PeisFailedTuple));    
  }
  if(!failed) {
    fprintf(stderr,"peisk::Out of memory error\n");
  } else {
    failed->tries = 15;
    failed->destination = destination;
    strcpy(failed->key,key);
    failed->next = peisk_failedTuples;
    peisk_failedTuples = failed;
  }
}

void peisk_retransmitTuples() {
  PeisHostInfo *hostInfo;
  PeisFailedTuple *failed;
  PeisFailedTuple **prev;
  PeisTuple *tuple, prototype;
  int remove;

  for(failed=peisk_failedTuples,prev=&peisk_failedTuples;failed;) {
    hostInfo = peisk_lookupHostInfo(failed->destination);
    failed->tries--;
    remove = (failed->tries<=0?1:0);
    if(hostInfo) {
      peisk_initAbstractTuple(&prototype);
      peisk_setTupleName(&prototype,failed->key);
      prototype.isNew=-1;
      prototype.owner = peisk_id; 
      tuple = peisk_getTupleByAbstract(&prototype);
      if(tuple && peisk_pushTuple(tuple,failed->destination) == 0) {
	/*printf("resending %s -> %d seems ok\n",failed->key,failed->destination);*/
	remove=1;
      } else if(tuple) {
	/*printf("resending %s -> %d failed...\n",failed->key,failed->destination);*/
      }
    }
    if(failed->tries<=0) {
      /*printf("Pruning %s -> %d from list of failed tuples\n",failed->key,failed->destination);*/
      remove = 1;
    }
    if(remove) {
      *prev = failed->next;
      failed->next = peisk_freeFailedTuples;
      peisk_freeFailedTuples = failed;
      failed = *prev;
    } else {
      prev=&failed->next;
      failed=failed->next;
    }
  }
}


void peisk_subscriptionAckHook(int success,int datalen,PeisPackage *package,void *data) {
  long long id = (long long) data;

  if(success) {
    /* If we succeeded with this subscription, we do not need to touch the
       array since it anyway "should" be marked as received if we have sent all 
       messages to this host. */
    /*printf("Subscriptions to %d ack'ed\n",id);*/
  } else {
    /* After a single failure, we will resend _all_ subscriptions to this host (eventually). */
    /*printf("Subscriptions to %d failed\n",id);*/
    peisk_hashTable_insert(peisk_hostGivenSubscriptionMessages,(void*)id,NULL);
  }
}

void peisk_periodic_tuples(void *data) {
  int len; 
  long long destination;
  void *value;
  int tupleBufferSize;
  char *buffer;
  char *key;
  PeisHashTableIterator iter,hostIter;
  PeisSubscriber *subscriber;
  PeisHostInfo *hostInfo;
  double t0;
  char tmp[256];
  static int tick=0;

  t0=peisk_gettimef();

  /* Retransmit any queued failed tuples every X seconds */
  if(++tick % 2 == 0)
    peisk_retransmitTuples();

  /* Iterate over hosts and resend subscriptions to those not given subscription messages */
  peisk_hashTableIterator_first(peiskernel.hostInfoHT,&hostIter);
  for(;peisk_hashTableIterator_next(&hostIter);) {
    /** \todo is it a bug to use int's as arguments to fetching values
	from hash tables key? */
    peisk_hashTableIterator_value_generic(&hostIter,&destination,&hostInfo);

    if(destination == peiskernel.id) continue;
    /* See if this host have been given all it's subscriptions */
    value = (void*) 0xFFAA;
    if(peisk_hashTable_getValue(peisk_hostGivenSubscriptionMessages,(void*)destination,&value)!=0 ||
       value == NULL) {
      int peis=destination;
      /* Mark array as TRUE for this peis (according to statement (1) in the peisk_hostGivenSubscriptionMessages array */
      peisk_hashTable_insert(peisk_hostGivenSubscriptionMessages,(void*)destination,(void*)1);
      /*printf("resending subscriptions to %d\n",peis);*/

      /* Find and send all subscription messages that are interesting
	 for this peis. */
      peisk_hashTableIterator_first(peisk_subscribers_primaryHT,&iter);
      for(;peisk_hashTableIterator_next(&iter);) {
	peisk_hashTableIterator_value_generic(&iter,&key,&subscriber);      
	if(subscriber->subscriber == peiskernel.id && 
	   (subscriber->prototype->owner == -1 || subscriber->prototype->owner == peis)) {
	  with_ack_hook(peisk_subscriptionAckHook,(void*)(long)destination,{
	      peisk_sendSubscribeMessageTo(subscriber,0,peis);
	    });	  
	}
      }
    }
  }
  


  /* Create the special "subscriber" tuples, can be monitored by the
     tupleviewer and usefull to see what is going on in the ecology */
  tupleBufferSize = peisk_tupleBufferSize;
 periodic_tuple_restartGen:
  buffer = peisk_getTupleBuffer(tupleBufferSize);
  buffer[0]=0; len=0;
  sprintf(buffer+len,"(\n"); { 
    while(buffer[len]) len++;
    if(len > tupleBufferSize - 256) { tupleBufferSize += 2048; goto periodic_tuple_restartGen; }
  }
  peisk_hashTableIterator_first(peisk_subscribers_primaryHT,&iter);
  for(;peisk_hashTableIterator_next(&iter);) {
    peisk_hashTableIterator_value_generic(&iter,&key,&subscriber);      
    if(subscriber->prototype->owner == peisk_id || 
       subscriber->prototype->owner == -1) {
      peisk_getTupleFullyQualifiedName(subscriber->prototype,tmp,sizeof(tmp));
      sprintf(buffer+len,"  ( %d %s )\n",subscriber->subscriber,tmp);
      while(buffer[len]) { 
	len++;
	if(len > tupleBufferSize - 256) { tupleBufferSize += 2048; goto periodic_tuple_restartGen; }
      }
    }
  }
  sprintf(buffer+len,")\n"); while(buffer[len]) len++;
  PEISK_ASSERT(len<tupleBufferSize,("Mathias is stupid"));
  
  /* Check that the new subscribers tuple is different from the old
     one, if so then set it to the tuplespace. */
  PeisTuple *oldSubscTuple;
  oldSubscTuple = peisk_getTuple(peisk_id,"kernel.subscribers",PEISK_KEEP_OLD);  

  /*  printf("old subscribers:\n'%s'",oldSubscribers);
  printf("new subscribers:\n'%s'",buffer);
  */
  if((!oldSubscTuple) || strcmp(buffer,oldSubscTuple->data) != 0) {
    peisk_setTuple("kernel.subscribers",len+1,buffer,"text/plain",PEISK_ENCODING_ASCII);
  }

}

void peisk_sendSubscribeMessage(PeisSubscriber *subscriber,int forceResend) {
  peisk_sendSubscribeMessageTo(subscriber,forceResend,-1);
}
void peisk_sendSubscribeMessageTo(PeisSubscriber *subscriber,int forceResend,int destination) {
  PeisSubscribeMessage *message;
  PeisTuple *tuple;
  long long len,dest;
  PeisHashTableIterator hostIter;
  PeisHostInfo *hostInfo;

  tuple=subscriber->prototype;

  int mimelength=tuple->mimetype?strlen(tuple->mimetype):0;
  int suffixLength = (tuple->data?tuple->datalen:0) + mimelength;
  len=sizeof(PeisSubscribeMessage)+suffixLength;
  message=(PeisSubscribeMessage*) peisk_getTupleBuffer(len);

  peisk_tuple_hton(tuple,&message->prototype);
  /* We don't store any subkeys in the message to avoid any possible
     inconsistency problems, the full key is copied in the private
     part anyway (keybuffer). The same goes for keyDepth. */
  /*for(i=0;i<7;i++) message->prototype.keys[i]=NULL;*/
  message->prototype.keyDepth=0;
  message->prototype.alloclen=0;
  message->prototype.isNew=htonl(subscriber->prototype->isNew);
  peisk_getTupleName(tuple,message->prototype.keybuffer,sizeof(message->prototype.keybuffer));

  if(tuple->mimetype) memcpy((void*)message+sizeof(PeisSubscribeMessage), (void*) tuple->mimetype, mimelength);
  memcpy((void*)message+sizeof(PeisSubscribeMessage)+mimelength, (void*) tuple->data, tuple->datalen);

  message->forceResend=forceResend;

  if(tuple->data) {
    /* We use the data field to mark if there is data available or
       not, the actual data is concatenated to the end of the message */
    message->prototype.data=htonl(1);
    message->prototype.datalen=htonl(tuple->datalen);
  } else {
    message->prototype.data=0;
    message->prototype.datalen=0;
  }

  /*printf("sending subscribe message to %d\n",subscriber->prototype->owner);
  printf("prototype: \n"); peisk_printTuple(subscriber->prototype); printf("\n");
  */

  if(destination == -1) {
    if(subscriber->prototype->owner == -1) {
      /* New, more reliable method is to send subscription to every
	 known host one by one... using reliable transmissions */
      peisk_hashTableIterator_first(peiskernel.hostInfoHT,&hostIter);
      for(;peisk_hashTableIterator_next(&hostIter);) {
	peisk_hashTableIterator_value_generic(&hostIter,&dest,&hostInfo);
	with_ack_hook(peisk_subscriptionAckHook,(void*)(long) hostInfo->id,{
	    if(peisk_sendMessage(PEISK_PORT_SUBSCRIBE,hostInfo->id,
				 len,(void*)message,PEISK_PACKAGE_RELIABLE) != 0)
	      /* Failed to send message towards target. Mark as failed. */
	      peisk_subscriptionAckHook(0,0,NULL,(void*)(long) hostInfo->id);
	  });
      }
    }
    else 
      with_ack_hook(peisk_subscriptionAckHook,(void*)(long)subscriber->prototype->owner,{
	  if(peisk_sendMessage(PEISK_PORT_SUBSCRIBE,subscriber->prototype->owner,
			       len,(void*)message,PEISK_PACKAGE_RELIABLE) != 0)
	    /* Failed to send message towards target. Mark as failed. */
	    peisk_subscriptionAckHook(0,0,NULL,(void*)(long)subscriber->prototype->owner);
	});
  } else {
    if(subscriber->prototype->owner == -1 || subscriber->prototype->owner == destination) {
      with_ack_hook(peisk_subscriptionAckHook,(void*)(long)destination,{
	  if(peisk_sendMessage(PEISK_PORT_SUBSCRIBE,destination,len,(void*)message,PEISK_PACKAGE_RELIABLE) != 0) 
	    /* Failed to send message towards target. Mark as failed. */
	    peisk_subscriptionAckHook(0,0,NULL,(void*)(long)destination);	    
	});
    } else {}
  }
}


void peisk_initSubscriber(PeisSubscriber *subscriber) {
  subscriber->handle=0;
  subscriber->isMeta=0;
}

PeisSubscriber *peisk_insertSubscriber(PeisSubscriber *subscriber) {
  PeisHashTableIterator iter;
  PeisSubscriber *subscriber2;
  char *key;
  char name[128];

  /*
  printf("inserting subscriber %d to: ",subscriber->subscriber); 
  peisk_printTuple(subscriber->prototype); printf("\n");
  */

  /* Iterate over all subscribers and see if we can find an
   _identical_ one. If so, update it's expiry time */
  /* \todo If the subscriber handle is nonzero, then just look it up
     and update the expiry date immediatly */
  for(peisk_hashTableIterator_first(peisk_subscribers_primaryHT,&iter);
      peisk_hashTableIterator_next(&iter);)
    if(peisk_hashTableIterator_value_generic(&iter, &key, &subscriber2)) {
      /* See if they are the same subscriber */
      if(subscriber->subscriber == subscriber2->subscriber && 
	 peisk_isEqual(subscriber->prototype,subscriber2->prototype)) {

	/*printf("Found old matching subscriber: "); peisk_printTuple(subscriber2->prototype); printf("\n");*/

	subscriber2->expire = subscriber->expire;
	return subscriber2;
      }      
    }

  /* Otherwise, clone this subscriber (including the tuple) and add
     as a new subscriber to the subscribers hashtable */
  /* \todo Save a list of free subscriber structures to reuse */
  subscriber2 = (PeisSubscriber*) malloc(sizeof(PeisSubscriber));
  if(!subscriber2) {
    /* Out of memory! */
    peisk_tuples_errno=PEISK_TUPLE_OUT_OF_MEMORY;
    return NULL;
  }
  /* Get a unique subscriber handle */
  subscriber2->handle = peisk_nextSubscriberHandle++; 
  /* Clone the prototype tuple */
  subscriber2->prototype = peisk_cloneTuple(subscriber->prototype);
  if(!subscriber2->prototype) return NULL;
  subscriber2->prototype->isNew = subscriber->prototype->isNew;

  subscriber2->expire = subscriber->expire;
  subscriber2->subscriber = subscriber->subscriber;
  /* When inserted, a subscriber is always non-meta. It must be modified 
     afterwards by inserting eg. the callback function etc. */
  subscriber2->isMeta = 0;
  subscriber2->metaCallback = 0;
  subscriber2->metaSubscription = 0;


  /* Do the actual insertion into the hashtable */
  snprintf(name,sizeof(name),"%d",subscriber2->handle);
  if(peisk_hashTable_insert(peisk_subscribers_primaryHT,name,(void*)subscriber2)) {
    peisk_tuples_errno=PEISK_TUPLE_HASHTABLE_ERROR;
    return 0;
  }

  /** TODO - set indexed key in another callbacks HT to be the (fully)
      qualified name. The hashtable must be changed to allow multiple
      entries with the same key. */

  /* Since this was a newly inserted subscriber, send copy of existing
     data to it */
  if(subscriber2->subscriber != peisk_id &&
     (subscriber2->prototype->owner == peiskernel.id ||
      subscriber2->prototype->owner == -1)) 
    peisk_alertSubscriber(subscriber2);

  /* Since this is a newly inserted subscriber we should send out the
     subscription message immediatly if it is us subscribing to
     something... */
  if(subscriber->subscriber == peiskernel.id) {
    peisk_sendSubscribeMessage(subscriber,1);
  }
  return subscriber2;
}

int peisk_rawUnsubscribe(PeisSubscriberHandle handle,int propagate) {

  PeisSubscriber *subscriber;
  char key[256];
  int errno,len,i;
  PeisUnsubscribeMessage *message;
  PeisTuple *tuple;
  

  if(!handle) {
    peisk_tuples_errno=PEISK_TUPLE_BAD_ARGUMENT;
    return 0;
  }

  snprintf(key,sizeof(key),"%d",handle);
  errno=peisk_hashTable_getValue(peisk_subscribers_primaryHT,key,
				 (void**)(void*)&subscriber);
  if(errno) {
    printf("WARNING - failed to delete subscriber: %d\n",handle);
    peisk_tuples_errno=PEISK_TUPLE_BAD_HANDLE;
    return peisk_tuples_errno;
  }

  /*
  char buf[256];
  peisk_getTupleName(subscriber->prototype,buf,255);
  printf("unsubscribing to %d.%s\n",subscriber->prototype->owner,buf);
  */

  if(subscriber->isMeta) {
    /* If this was a meta subscription then free also callback and indirect subscription */
    if(subscriber->metaCallback) peisk_unregisterTupleCallback(subscriber->metaCallback);
    if(subscriber->metaSubscription) peisk_unsubscribe(subscriber->metaSubscription);
  } else if(propagate && subscriber->prototype->owner != peisk_id) {    
    /* For normal subscriptions, send unsubscription message if owner
	 is another peis. */
    tuple=subscriber->prototype;
    len=sizeof(PeisUnsubscribeMessage)+(tuple->data ? tuple->datalen : 0);
    message=(PeisUnsubscribeMessage*) peisk_getTupleBuffer(len);
    /*message->prototype = *tuple;*/
   
    message->prototype.owner = htonl(tuple->owner);
    message->prototype.creator = htonl(tuple->creator);
    for(i=0;i<2;i++) {
      message->prototype.ts_write[i] = htonl(tuple->ts_write[i]);
      message->prototype.ts_user[i] = htonl(tuple->ts_user[i]);
      message->prototype.ts_expire[i] = htonl(tuple->ts_expire[i]);
    }

    /* We don't store any subkeys in the message to avoid any possible
       inconsistency problems, the full key is copied in the private
       part anyway (keybuffer). The same goes for keyDepth. */    
    //for(i=0;i<7;i++) message->prototype.keys[i]=NULL;
    message->prototype.keyDepth=0;
    message->prototype.alloclen=0;
    peisk_getTupleName(tuple,message->prototype.keybuffer,sizeof(message->prototype.keybuffer));
    
    if(tuple->data) {
      /* We use the data field to mark if there is data available or
	 not, the actual data is concatenated to the end of the message */
      message->prototype.data=ntohl(1);
      message->prototype.datalen=htonl(tuple->datalen);
      memcpy((void*)(message+1),tuple->data,tuple->datalen);
    } else {
      message->prototype.data=0;
      message->prototype.datalen=0;
    }
    
    if(tuple->owner == -1) {
      peisk_broadcast(PEISK_PORT_UNSUBSCRIBE,len,(void*)message);
    } else {
      peisk_sendMessage(PEISK_PORT_UNSUBSCRIBE,tuple->owner,
			len,(void*)message,PEISK_PACKAGE_RELIABLE);
    }
  }

  peisk_freeTuple(subscriber->prototype);
  free(subscriber);
  errno=peisk_hashTable_remove(peisk_subscribers_primaryHT,key);
  if(errno) {
    printf("WARNIGN - failed to remove subscriber from hashtable: %d\n",handle);
    peisk_tuples_errno=PEISK_TUPLE_HASHTABLE_ERROR;
    return peisk_tuples_errno;
  }
  return 0;
}

void peisk_tuple_ntoh(PeisNetworkTuple *from,PeisTuple *to) {
  int i;

  to->owner = ntohl(from->owner);
  to->creator = ntohl(from->creator);
  for(i=0;i<2;i++) {
    to->ts_write[i]= ntohl(from->ts_write[i]);
    to->ts_user[i] = ntohl(from->ts_user[i]);
    to->ts_expire[i] = ntohl(from->ts_expire[i]);
  }
  to->isNew = ntohl(from->isNew);
  to->datalen = ntohl(from->datalen);  
  to->seqno = ntohl(from->seqno);
  to->appendSeqNo = ntohl(from->appendSeqNo);
  to->encoding = ntohl(from->encoding);

  /* The name part must be manually/parsed copied anyway */
}
void peisk_tuple_hton(PeisTuple *from,PeisNetworkTuple *to) {
  int i; 

  to->owner = htonl(from->owner);
  to->creator = htonl(from->creator);
  for(i=0;i<2;i++) {
    to->ts_write[i]= htonl(from->ts_write[i]);
    to->ts_user[i] = htonl(from->ts_user[i]);
    to->ts_expire[i] = htonl(from->ts_expire[i]);
  }
  to->isNew = htonl(from->isNew);
  to->datalen = htonl(from->datalen);
  to->data = from->datalen?1:0;
  to->seqno = htonl(from->seqno);
  to->appendSeqNo = htonl(from->appendSeqNo);
  to->encoding = htonl(from->encoding);
  to->mimetypeLength = from->mimetype?strlen(from->mimetype):0;
  /* The name part must be manually/parsed copied anyway */
}



int peisk_hook_subscribe(int port,int destination,int sender,int datalen,void *data) {
  PeisSubscribeMessage *message;
  PeisTuple prototype;
  PeisNetworkTuple netPrototype;

  /*printf("peisk_hook_subscribe triggered. dest=%d, sender=%d\n",destination,sender);*/


  /* If we catch a (broadcasted) subscription message from ourself
     then ignore it. */
  if(sender == peiskernel.id) return 0;
  /* If it's not a broadcasted message and not aimed at us, ignore it */
  if(destination != -1 && destination != peiskernel.id) return 0;

  message = (PeisSubscribeMessage*) data;
  /* We make a copy of the message prototype tuple (just the tuple
  header) since we are destructively modifying it - and - it might
  still be sitting in an output buffer to be further propagated in
  case it was a broadcasted package */
  netPrototype = message->prototype;
  
  /* Convert from network to host byte order */
  peisk_tuple_ntoh(&netPrototype,&prototype);

  /*printf("Subscription has datapart with %d bytes\n",prototype.datalen);*/

  /* Correct the key since the transmitted subkeys are zero */
  peisk_setTupleName(&prototype,netPrototype.keybuffer);

  int mimelength=netPrototype.mimetypeLength;
  if(mimelength == 255) mimelength=0;

  /* Get the data */
  if(netPrototype.data == 0) {
    prototype.alloclen=0;
    prototype.data=NULL;
  } else {
    prototype.data = data + sizeof(PeisSubscribeMessage) + mimelength;
    /*PEISK_ASSERT(ntohl(prototype.datalen) == datalen - sizeof(PeisSubscribeMessage),
      ("bad length of (remote) subscriber tuple"));*/
    prototype.datalen = datalen - sizeof(PeisSubscribeMessage) - mimelength;
    if(prototype.datalen <= 0) { 
      fprintf(stderr,"Bad data part of (remote) subscribe tuple\n"); 

      prototype.data = NULL;
    }
  }
  char mimetype[256];
  if(netPrototype.mimetypeLength == 0)
    prototype.mimetype=NULL;
  else {
    memcpy(mimetype,data+sizeof(PeisSubscribeMessage),mimelength);
    mimetype[mimelength]=0;
    prototype.mimetype=mimetype;
  }

  /* Subscribers cannot realy on the isNew field in the current implementation on how they are alerted. Otherwise we need to keep track on all subscribers if they have been notified or not... */
  prototype.isNew=-1;

  //printf("subscription received from %d: ",sender); peisk_printTuple(&prototype); printf("\n");

  /*
  printf("connection: %d ",peisk_lastConnection);
  printf("from neighbour %d\n", peisk_lookupConnection(peisk_lastConnection)->neighbour.id);
  */

  /* Do the subscription */
  PeisSubscriber subscriber;
  PeisSubscriber *subscriber2;
  int numSubscribers;
  subscriber.prototype = &prototype;
  subscriber.handle = 0;
  subscriber.expire = peisk_gettimef()+PEISK_MAX_SUBSCRIPTION_TIME;
  subscriber.subscriber = sender;
  subscriber.isMeta=0;

  numSubscribers=peisk_nextSubscriberHandle;
  subscriber2 = peisk_insertSubscriber(&subscriber);

  /* If the message asked for a refresh send a new copy of all
     matching tuples to the subscriber, UNLESS a new subscriber
     structure was created. In that case the messages will have been
     sent anyway. */
  /* MB 090302: was if(message->forceResend && peisk_nextSubscriberHandle == numSubscribers)
     try treating all subscriptions as forced for now. Multiple receptions will anyway be caught
     using the seqnumbers and this fixes a problem with REBORN hosts and out-of-order reception
     of dead_host_reborn messages. */
  if(peisk_nextSubscriberHandle == numSubscribers)
    peisk_alertSubscriber(subscriber2);

  return 0;
}

int peisk_hook_unsubscribe(int port,int destination,int sender,int datalen,void *data) {
  PeisUnsubscribeMessage *message;
  PeisNetworkTuple netPrototype;
  PeisTuple prototype;
  PeisHashTableIterator iter;
  PeisSubscriber *subscriber;
  char *key;

  /* If it is not aimed at us or a broadcasted message then ignore it. */
  if(destination != peisk_id && destination != -1) return 0;
  PEISK_ASSERT(sender != peisk_id,("Caught an unsubscribe message sent by ourselves"));

  message = (PeisUnsubscribeMessage*) data;
  /* Make a local copy of the tuple header just like in the other hook functionalities */
  netPrototype = message->prototype;

  /* Convert from network to host byte order */
  peisk_tuple_ntoh(&netPrototype,&prototype);

  /* Correct the key since the transmitted subkeys are zero */
  peisk_setTupleName(&prototype,prototype.keybuffer);

  int mimelength=netPrototype.mimetypeLength;
  if(mimelength == 255) mimelength=0;

  /* Get the data */
  if(netPrototype.data != 0) {
    prototype.data = data + sizeof(PeisSubscribeMessage) + mimelength;
    PEISK_ASSERT(ntohl(prototype.datalen) == datalen - sizeof(PeisSubscribeMessage) - mimelength,
		 ("bad length of in (remote) subscriber tuple"));
    prototype.datalen = datalen - sizeof(PeisSubscribeMessage);
    if(prototype.datalen <= 0) { 
      fprintf(stderr,"Bad data part of (remote) subscribe tuple\n"); 
      prototype.data = NULL;
    }
  } else {
    prototype.data = NULL;
  }
  char mimetype[256];
  if(netPrototype.mimetypeLength == 0)
    prototype.mimetype=NULL;
  else {
    memcpy(mimetype,data+sizeof(PeisSubscribeMessage),mimelength);
    mimetype[mimelength]=0;
    prototype.mimetype=mimetype;
  }

  /* Do the unsubscription */
  for(peisk_hashTableIterator_first(peisk_subscribers_primaryHT,&iter);
      peisk_hashTableIterator_next(&iter);)
    if(peisk_hashTableIterator_value_generic(&iter, &key, &subscriber)) {

      /* See if this is the same subscription */
      if(subscriber->subscriber == sender && 
	 peisk_isEqual(subscriber->prototype,&prototype)) {
	peisk_rawUnsubscribe(subscriber->handle,0);
	return 0;
      }
    }

  /* Well, looks like the subscription was not found, just ignore it */
  /*printf("peisk warning: ignoring unsubscribe message for unknown subscription\n");*/
  return 0;
}

int peisk_hook_pushTuple(int port,int destination,int sender,int datalen,void *data) {
  PeisPushTupleMessage *message;
  PeisNetworkTuple netTuple;
  PeisTuple tuple;


  /* If it is not aimed at us, or is a broadcasted message then ignore
     it. (NOT legal to send broadcasted push'es for now) */     
  if(destination != peisk_id) return 0;
  PEISK_ASSERT(sender != peisk_id,("Caught a push message sent by ourselves"));

  message = (PeisPushTupleMessage*) data;

  if(datalen < sizeof(PeisPushTupleMessage)) {
    printf("peisk: warning, got invalid push tuple message (too short)\n");
    return 0;
  }

  /* Make a local copy of the tuple header just in case just like in the
     other hook functionalities */
  netTuple = message->tuple;

  /*
  printf("received data: \n");
  peisk_hexDump(data,datalen);
  */

  /* \todo In peisk_hook_pushTuple, make sure we are realy subscribed
     to the tuple beeing propagated to us */

  /* Convert from network to host byte order */
  peisk_tuple_ntoh(&netTuple,&tuple);

  /* Correct the key since the transmitted subkeys are zero */
  peisk_setTupleName(&tuple,netTuple.keybuffer);

  /*char buff[256];  
  peisk_getTupleName(&tuple,buff,255);
  printf("Received pushTuple from %d to %d. Tuple %s\n",sender,destination,buff);
  */

  if(tuple.owner != sender) {
    printf("peisk: warning, got a push tuple message with tuple->owner != sender\n");
    tuple.data=NULL;
    peisk_printTuple(&tuple); printf("\n");
    return 0;
  }

  int mimelength=netTuple.mimetypeLength;
  if(mimelength == 255) mimelength=0;

  /* Set the data part of the tuple */
  if(netTuple.data == 0) {
    tuple.alloclen=0;
    tuple.data = NULL;
    tuple.datalen = 0;
  } else {
    tuple.data = data + sizeof(PeisPushTupleMessage) + mimelength;
    tuple.datalen = datalen - sizeof(PeisPushTupleMessage) - mimelength;
    if(tuple.datalen <= 0) { 
      fprintf(stderr,"Bad data part of (remote) subscribe tuple\n"); 
      tuple.data = NULL;
    }
    tuple.alloclen = tuple.datalen;
  }

  char mimetype[256];
  if(netTuple.mimetypeLength == 0)
    tuple.mimetype=NULL;
  else {
    memcpy(mimetype,data+sizeof(PeisPushTupleMessage),mimelength);
    mimetype[mimelength]=0;
    tuple.mimetype=mimetype;
  }

  /* Make sure tuple is concrete before we add it to the tuplespace */
  if(peisk_tupleIsAbstract(&tuple)) {
    printf("peisk: warning, got a push tuple message with an abstract tuple\n");
    peisk_printTuple(&tuple); printf("\n");
    return 0;
  }

  /* See if tuple exists previously, if so, verify that seqno is higher than previously */
  tuple.isNew = -1;
  PeisTuple *oldTuple = peisk_getTupleByAbstract(&tuple);
  if(oldTuple && oldTuple->seqno >= tuple.seqno) {    
    /** \todo Save out-of-order tuples an update them when the missing tuple update have been found */
    /*
    printf("Ignoring duplicate or out-of-order (?) tuple\n");
    printf("old seqno: %d new seqno: %d\n",oldTuple->seqno,tuple.seqno);*/
    return 0;
  }

  /*printf("received push from %d: ",sender); peisk_printTuple(tuple); printf("\n");*/
  peisk_addToLocalSpace(&tuple);

  /*PeisTuple *tuple2;
  char tmpName[256];
  peisk_getTupleName(&tuple,tmpName,256);
  tuple2=peisk_getTuple(tuple.owner,tmpName,PEISK_KEEP_OLD|PEISK_NON_BLOCKING);
  printf("Inserted tuple, seqno=%d\n",tuple2->seqno);
  */

  return 0;
}
int peisk_hook_setTuple(int port,int destination,int sender,int datalen,void *data) {
  PeisPushTupleMessage *message;
  PeisTuple tuple;
  PeisNetworkTuple netTuple;
  
  /* If it is not aimed at us, or is a broadcasted message then ignore
     it. (NOT legal to send broadcasted setTuple messages for now) */     
  if(destination != peisk_id) return 0;
  PEISK_ASSERT(sender != peisk_id,("Caught a setTuple message sent by ourselves"));

  message = (PeisPushTupleMessage*) data;
  /* Make a local copy of the tuple header just in case just like in the
     other hook functionalities */
  netTuple = message->tuple;

  /*printf("Net tuple: \n");
  peisk_hexDump((void*)&netTuple,sizeof(netTuple));
  printf("Offset of mimelength: %d\n",((char*)&netTuple.mimetypeLength)-((char*)&netTuple));
  */

  /* Convert from network to host byte order */
  peisk_tuple_ntoh(&netTuple,&tuple);

  /* Correct the key since the transmitted subkeys are zero */
  peisk_setTupleName(&tuple,netTuple.keybuffer);

  if(tuple.owner != peiskernel.id) {
    printf("peisk: warning, got a set tuple message with tuple->owner != peiskernel.id\n");
    return 0;
  }

  /*printf("Got a setRemote tuple message\n");
  peisk_hexDump(data,datalen);
  */

  int mimelength=netTuple.mimetypeLength;
  if(mimelength>254 || mimelength < 0) {
    fprintf(stderr,"Invalid mimelength of incomming setTuple message\n");
    return 0;
  }


  /* Set the data part of the tuple */
  if(netTuple.data == 0) {
    tuple.alloclen=0;
  } else {
    tuple.data = data + sizeof(PeisPushTupleMessage) + mimelength;
    tuple.datalen = datalen - sizeof(PeisPushTupleMessage) - mimelength;
    if(tuple.datalen <= 0) { 
      fprintf(stderr,"Bad data part of (remote) set tuple tuple\n"); 
      printf("Got %d byte package, message size: %ld mimelen %d\n",datalen,sizeof(PeisPushTupleMessage),mimelength);
      tuple.data = NULL;
    }
    tuple.alloclen = tuple.datalen;
  }

  char mimetype[256];
  if(netTuple.mimetypeLength == 0)
    tuple.mimetype=NULL;
  else {
    memcpy(mimetype,data+sizeof(PeisPushTupleMessage),mimelength);
    mimetype[mimelength]=0;
    tuple.mimetype=mimetype;
    //printf("Setting a tuple with a new mimetype: '%s' (%d)\n",mimetype,mimelength);
  }

  /* Make sure tuple is concrete before we add it to the tuplespace */
  if(peisk_tupleIsAbstract(&tuple)) {
    printf("peisk: warning, got a setTuple message with an abstract tuple. Tell Mathias!\n");
    peisk_printTuple(&tuple); 
    /*printf("\nRecieved data was:");
      peisk_hexDump(data,datalen);*/
    return 0;
  }

  peisk_gettime2(&tuple.ts_write[0],&tuple.ts_write[1]);
  peisk_addToLocalSpace(&tuple);

  return 0;
}

int peisk_hook_pushAppendedTuple(int port,int destination,int sender,int datalen,void *data) {
  PeisTuple proto;
  PeisTuple *tuple;
  PeisAppendTupleMessage *message;

  if(sender == peiskernel.id) return 0;
  if(destination != -1 && destination != peiskernel.id) return 0;
  if(datalen < sizeof(PeisAppendTupleMessage)) return 0;
  message = (PeisAppendTupleMessage *) data;

  peisk_tuple_ntoh(&message->tuple,&proto);
  peisk_setTupleName(&proto,message->tuple.keybuffer);

  char buff[256];  
  peisk_getTupleName(&proto,buff,255);
  //  printf("Received push appended tuple from %d to %d. Tuple %s\n",sender,destination,buff);
  if(proto.owner != sender) {
    printf("peisk: warning, got a push tuple message with tuple->owner != sender\n");
    proto.data=NULL;
    peisk_printTuple(&proto); printf("\n");
    return 0;
  }
  /* Find the corresponding tuple in our data base.
     Note: the "prototype" above should be a concrete tuple in all
     respects except the data field. */
  char fullname[PEISK_KEYLENGTH+5];
  peisk_getTupleFullyQualifiedName(&proto,fullname,sizeof(fullname));
  int errno;
  errno=peisk_hashTable_getValue(peisk_tuples_primaryHT,fullname,
				 (void**) (void*) &tuple);
  if(errno || !tuple) {
    printf("Got a push appended tuple for a tuple we do not know about\n");
    return 0;
  }
  //if(tuple->seqno == 0) tuple->seqno = proto.seqno; 
  if(tuple->seqno != proto.seqno) {
    printf("Ignoring append to old tuple. proto seqno was %d, tuple seqno is %d\n",proto.seqno,tuple->seqno);
    return 0;
  }
  /** \todo Make sure that append operations are performed in-order */
  if(tuple->appendSeqNo != 0 && tuple->appendSeqNo+1 != proto.appendSeqNo) {
    /*printf("Warning - receiving append operations out-of-order\n");
      return 0;*/
  }
  int difflen=ntohl(message->difflen);

  int smartUpdate=tuple->encoding == PEISK_ENCODING_ASCII;
  //if(strlen(tuple->data)+1 == tuple->datalen) smartUpdate=1;
  if(tuple->alloclen < tuple->datalen + difflen) {
    tuple->data = realloc(tuple->data, tuple->datalen+difflen+(smartUpdate?-1:0));
    tuple->alloclen=tuple->datalen+difflen+(smartUpdate?-1:0);
  }
  memcpy(tuple->data+tuple->datalen+(smartUpdate?-1:0),(void*) (message+1),difflen);
  tuple->datalen += difflen+(smartUpdate?-1:0);
  tuple->appendSeqNo = proto.appendSeqNo;
  tuple->isNew=1;

  /** \todo care about mimetype when appending to a tuple (checking if the append should realy be done or not) */

  /* Trigger any local callbacks that depend on this tuple */
  PeisHashTableIterator callIter;
  PeisCallback *callback;
  char *key;
  for(peisk_hashTableIterator_first(peisk_callbacks_primaryHT,&callIter);
      peisk_hashTableIterator_next(&callIter);)

    if(peisk_hashTableIterator_value_generic(&callIter, &key, &callback)) {
      /* See if this callback matches this tuple */
      if(callback->type == PEISK_CALLBACK_CHANGED &&
	 peisk_compareTuples(tuple,callback->prototype) == 0) {
	(callback->fn)(tuple,callback->userdata);
      }
    }
  return 0;
}
int peisk_hook_setAppendTuple(int port,int destination,int sender,int datalen,void *data) {
  printf("peisk_hook_setAppendTuple. sender=%d, dest=%d\n",sender,destination);

  PeisTuple proto;
  PeisAppendTupleMessage *message;

  if(sender == peiskernel.id) return 0;
  if(destination != -1 && destination != peiskernel.id) return 0;
  if(datalen < sizeof(PeisAppendTupleMessage)) return 0;
  message = (PeisAppendTupleMessage *) data;

  peisk_tuple_ntoh(&message->tuple,&proto);
  peisk_setTupleName(&proto,message->tuple.keybuffer);

  if(proto.owner != peiskernel.id) {
    printf("Ignoring setAppendTuple message not directed to us\n");
    return 0;
  }
  int difflen=ntohl(message->difflen);
  void *diff = ((void*)(message+1))+proto.datalen;
  /** \todo proper handling of append requests with non-empty mimepart */
  proto.mimetype=NULL;
  /** \todo proper handling of append requests with non-empty data part */
  proto.data = NULL; // ((void*)(message+1)); 
  proto.datalen=0;

  //printf("Calling appendLocalTuple: %d %s\n",difflen,(char*)diff);
  peisk_appendLocalTuple(&proto,difflen,diff);
  return 0;
}

void peisk_periodic_debugInfoTuples(void *data) {
  if(peisk_debugTuples) {
    /* TODO - add some debugging stuff here */
  }
}

void peisk_expireListRemove(PeisTuple *tuple) {
  PeisExpireList **prev;
  PeisExpireList *this;

  for(prev=&peisk_expireList;*prev;prev=&((*prev)->next)){
    if((*prev)->tuple == tuple) {
      /* Found tuple to delete */ 
      this=*prev;
      /* remove it from the list */
      *prev=(*prev)->next;
      /* insert into list of free expire nodes */
      this->next=peisk_expireListFree;
      peisk_expireListFree=this;
      return;
    }
  }
}

void peisk_expireListAdd(PeisTuple *tuple) {
  PeisExpireList *this;
  PeisExpireList **prev;

  /* Check that the expiry time is valid (non null) */
  if(tuple->ts_expire[0] == 0) return;

  /* Allocate an expiry entry for this tuple */
  if(peisk_expireListFree) {
    this=peisk_expireListFree;
    peisk_expireListFree=peisk_expireListFree->next;
  } else {
    this = (PeisExpireList*) malloc(sizeof(PeisExpireList));
  }
  this->tuple=tuple;
  
  /* Iterate over list to know where to insert this expiry node */
  for(prev=&peisk_expireList;*prev;prev=&((*prev)->next)){
    if(tuple->ts_expire[0] < (*prev)->tuple->ts_expire[0] ||
       (tuple->ts_expire[0] == (*prev)->tuple->ts_expire[0] &&
	tuple->ts_expire[1] < (*prev)->tuple->ts_expire[1])) break;
  }
  /* Insert it */
  this->next = *prev;
  *prev = this;

}

void peisk_periodic_expireTuples(void *data) {
  int timenow[2];
  PeisExpireList *this;
  PeisHashTableIterator iter;
  char *key;
  PeisCallback *callback;

  peisk_gettime2(&timenow[0],&timenow[1]);

  for(;peisk_expireList;) {
    if(timenow[0] < peisk_expireList->tuple->ts_expire[0] ||
       (timenow[0] == peisk_expireList->tuple->ts_expire[0] &&
	timenow[1] <= peisk_expireList->tuple->ts_expire[1])) break;

    /* Remove and insert into list of free expire nodes */
    this=peisk_expireList;
    peisk_expireList = peisk_expireList->next;
    this->next=peisk_expireListFree;
    peisk_expireListFree=this;
    
    /* Delete the tuple */
    char fullname[512];
    peisk_getTupleFullyQualifiedName(this->tuple,fullname,sizeof(fullname));
    /*    printf("Deleting tuple: %s\n",fullname);*/

    peisk_hashTable_remove(peisk_tuples_primaryHT,fullname);
    /* TODO: If we use alternative hashkeys then delete them too */

    /* Invoke all deletion callbacks for this tuple. */
    for(peisk_hashTableIterator_first(peisk_callbacks_primaryHT,&iter);
	peisk_hashTableIterator_next(&iter);)
      if(peisk_hashTableIterator_value_generic(&iter, &key, &callback)) {
	if(callback->type == PEISK_CALLBACK_DELETED &&
	   peisk_compareTuples(this->tuple,callback->prototype) == 0) {
	  (callback->fn)(this->tuple,callback->userdata);
	}
      }


    /* If the inserted tuple belongs to us, update the "all-keys" tuple */
    if(this->tuple->owner == peiskernel.id) {
      peisk_generateAllKeysTuple();
    }

    peisk_freeTuple(this->tuple);
    if(!peisk_expireList) break;
    /* TODO: perform the actual delete here... */
  }  

}


/* These functions are primarily used by the TinyGateway. They are
   here to ensure consistency with the other peisk_pushTuple etc. functions */
void peisk_pushTupleFrom(int from,PeisTuple *tuple,int destination) {
  PeisPushTupleMessage message;
  char *buffer;
  int len,ret,i;
  
  char tmp[256];
  peisk_getTupleFullyQualifiedName(tuple,tmp,sizeof(tmp));

  /* If destination is self - nothing to do */
  /* Otherwise, send a PeisPushTupleMessage */
  if(destination == peiskernel.id) {
    printf("Warning, ignoring pushtuple message to ourselves\n");
    return; 
  }

  /*printf("pushTuple: Sending to %d, tuple:",destination); peisk_printTuple(tuple); printf("\n");*/

  /* Only the kernel which owns a tuple is suppose to propagate it. */
  /*if(tuple->owner != peiskernel.id) return;*/

  message.tuple.owner = htonl(tuple->owner);
  message.tuple.creator = htonl(tuple->creator);
  for(i=0;i<2;i++) {
    message.tuple.ts_write[i] = htonl(tuple->ts_write[i]);
    message.tuple.ts_user[i] = htonl(tuple->ts_user[i]);
    message.tuple.ts_expire[i] = htonl(tuple->ts_expire[i]);
  }
  message.tuple.isNew = htonl(tuple->isNew);
  message.tuple.seqno = htonl(tuple->seqno);
  message.tuple.appendSeqNo = htonl(tuple->appendSeqNo);

  /* Copy the keyname from given tuple to the new message tuple */
  ret=peisk_getTupleName(tuple,message.tuple.keybuffer,sizeof(message.tuple.keybuffer));
  PEISK_ASSERT(ret == 0, ("error setting name for PushTupleMessage. '%s'\n",peisk_tuple_strerror(ret)));


  int mimelength=tuple->mimetype?strlen(tuple->mimetype):0;
  int suffixLength = tuple->datalen + mimelength;
  len=sizeof(message)+suffixLength;
  buffer = peisk_getTupleBuffer(len);
  memcpy((void*)buffer, (void*) &message, sizeof(message));
  if(tuple->mimetype) memcpy((void*)buffer+sizeof(message), (void*) tuple->mimetype, mimelength);
  memcpy((void*)buffer+sizeof(message)+mimelength, (void*) tuple->data, tuple->datalen);

  if(tuple->data == NULL) {
    message.tuple.data=0;
    message.tuple.datalen=htonl(-1);
  } else {
    message.tuple.data=htonl(1);
    message.tuple.datalen = htonl(tuple->datalen);
  }

  /*
  printf("sending push tuple message to %d: ",destination);
  peisk_printTuple(tuple); printf("\n");
  */


  /*printf("message: \n");
    peisk_hexDump(buffer,len);*/

  peisk_sendMessageFrom(from,PEISK_PORT_PUSH_TUPLE,destination, len, (void*) buffer, PEISK_PACKAGE_RELIABLE);  
}


/* Used only by Tiny Gateway */
void peisk_sendSubscribeMessageFrom(int from,PeisSubscriber *subscriber,int forceResend) {
  PeisSubscribeMessage *message;
  int len,i;
  PeisTuple *tuple;

  tuple=subscriber->prototype;
  int mimelength=tuple->mimetype?strlen(tuple->mimetype):0;
  int suffixLength = (tuple->data?tuple->datalen:0) + mimelength;
  len=sizeof(PeisSubscribeMessage)+suffixLength;
  message=(PeisSubscribeMessage*) peisk_getTupleBuffer(len);

  message->prototype.owner = htonl(tuple->owner);
  message->prototype.creator = htonl(tuple->creator);
  for(i=0;i<2;i++) {
    message->prototype.ts_write[i] = htonl(tuple->ts_write[i]);
    message->prototype.ts_user[i] = htonl(tuple->ts_user[i]);
    message->prototype.ts_expire[i] = htonl(tuple->ts_expire[i]);
  }
  /* We don't store any subkeys in the message to avoid any possible
     inconsistency problems, the full key is copied in the private
     part anyway (keybuffer). The same goes for keyDepth. */

  /*for(i=0;i<7;i++) message->prototype.keys[i]=NULL;*/
  message->prototype.keyDepth=0;
  message->prototype.alloclen=0;
  message->prototype.isNew=htonl(subscriber->prototype->isNew);
  peisk_getTupleName(tuple,message->prototype.keybuffer,sizeof(message->prototype.keybuffer));

  if(tuple->mimetype) memcpy((void*)message+sizeof(PeisSubscribeMessage), (void*) tuple->mimetype, mimelength);
  memcpy((void*)message+sizeof(PeisSubscribeMessage)+mimelength, (void*) tuple->data, tuple->datalen);


  if(tuple->data) {
    /* We use the data field to mark if there is data available or
       not, the actual data is concatenated to the end of the message */
    message->prototype.data=ntohl(1);
    message->prototype.datalen=htonl(tuple->datalen);
  } else {
    message->prototype.data=0;
    message->prototype.datalen=0;
  }
  message->forceResend=forceResend;

  /*
  printf("sending subscribe message to %d\n",subscriber->prototype->owner);
  printf("prototype: \n"); peisk_printTuple(subscriber->prototype); printf("\n");
  */



  if(subscriber->prototype->owner == -1) 
    peisk_broadcast(PEISK_PORT_SUBSCRIBE,len,(void*)message);
  /* DEBUG - USE RELIABLE FOR NOW */
  else {
    if(subscriber->prototype->owner < 0) {
      printf("peisk: WARNING, attempting to send subscription to invalid destination %d\n",subscriber->prototype->owner);
      return;
    }
    peisk_sendMessageFrom(from,PEISK_PORT_SUBSCRIBE,subscriber->prototype->owner,
		      len,(void*)message,PEISK_PACKAGE_RELIABLE);
  }
}

/*********************************************************/
/*                                                       */
/* Meta Tuples                                           */
/*                                                       */
/*********************************************************/


void peisk_metaSubscriptionCallback(PeisTuple *tuple,void *userdata) {
  char key[256];
  int owner=-1;
  PeisSubscriber *subscriber = (PeisSubscriber*) userdata;
  
  /*printf("peisk_metaSubscriptionCallback: ");
  peisk_printTuple(tuple);
  printf("\n");
  */

  /* Unsubscribe to any old value of meta tuple */
  /** \todo First check if this new meta tuple is different 
      from the old subscription, otherwise do not make an 
      unsubscribe to avoid unneccessary network trafic. */
  if(subscriber->metaSubscription) {
    //printf("performing an unsubscribe!\n");
    peisk_unsubscribe(subscriber->metaSubscription);
    subscriber->metaSubscription = 0;
  }

  /* do the parsing and establishing of a subscription here... */
  if(!peisk_parseMetaTuple(tuple,&owner,key) || owner == -1) {
    /* Invalid meta tuple */
    if(owner != -1) {
      char fullName[256];
      peisk_getTupleFullyQualifiedName(tuple,fullName,sizeof(fullName));      
      fprintf(stderr,"Warning, invalid meta tuple: %s = %s\n",fullName,tuple->data);
    }
  } else {
    subscriber->metaSubscription = peisk_subscribe(owner,key);
  }
}

void peisk_deleteHostFromSentSubscriptions(int id) {
  long long lid=id;
  peisk_hashTable_remove(peisk_hostGivenSubscriptionMessages,(void*)lid);
}

void peisk_deleteHostFromTuplespace(int id) {
  PeisHashTableIterator iter;
  char *key;
  PeisTuple *tuple;
  PeisSubscriber *subscriber;

  /*printf("tuples.c::deleteHostFromTuplespace(%d)\n",id);*/

  /* Remove host from those given broadcased subscription messages (so that he
     will receive them again when reappearing) 
  */


  /* Expire all tuples with this host as owner.  */
  /* Note that actual removal of tuples will happen on next periodic
     call */
  for(peisk_hashTableIterator_first(peisk_tuples_primaryHT,&iter);
      peisk_hashTableIterator_next(&iter);)
    if(peisk_hashTableIterator_value_generic(&iter, &key, &tuple))
      if(tuple->owner == id) {
	if(tuple->ts_expire[0] != 0)
	  peisk_expireListRemove(tuple);	
	tuple->ts_expire[0] = PEISK_TUPLE_EXPIRE_NOW;
	tuple->ts_expire[1] = PEISK_TUPLE_EXPIRE_NOW;
	peisk_expireListAdd(tuple);	
      }

  /* Delete all subscriptions *from* this host. */
  for(peisk_hashTableIterator_first(peisk_subscribers_primaryHT,&iter);
      peisk_hashTableIterator_next(&iter);)
    if(peisk_hashTableIterator_value_generic(&iter, &key, &subscriber))
      if(subscriber->subscriber == id) {
	/* Remove the subscriber */
	peisk_rawUnsubscribe(subscriber->handle,0);
	/* restart iteration from beginning since we have deleted an
	   item from the hash table. Not so efficient... */
	peisk_hashTableIterator_first(peisk_subscribers_primaryHT,&iter);
      }
}
