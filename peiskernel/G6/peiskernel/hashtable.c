/** \file  hashtable.h
    \brief Generic routines for manipulating hashtables and other datatypes.

    Although intended to be private to peiskernel it can also be used by components
    if these functionalitites are required.
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
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/time.h>
#include <ctype.h>
#include <string.h>
#include <sys/select.h>
#include <math.h>
#include <errno.h>

#define PEISK_PRIVATE
#include "peiskernel.h"
#include "hashtable.h"

int peisk_hashTable_hash(PeisHashTable *table,void *key) {
  switch(table->keyType) {
  case PeisHashTableKey_String: return peisk_hashString((char*)key);
  case PeisHashTableKey_Integer: return (((int)(long) key) ^ 0xdeadf00d) & 0x7fffffff;
  default: fprintf(stderr,"peisk:: error - Broken hashtable type\n"); exit(0); 
  }
}

/** \todo Let the hashtable operations save free'ed links so that they
    can be used later on. Would reduce memory fragmentation. */
PeisHashTable *peisk_hashTable_create(PeisHashTableKeyType keyType) {
  PeisHashTable *table=(PeisHashTable*)malloc(sizeof(PeisHashTable));
  table->nBuckets=0;
  table->buckets=NULL;
  table->keyType = keyType;
  return table;
}
void peisk_hashTable_delete(PeisHashTable *table) {
  int i;
  PeisHashTableLink *link;
  PeisHashTableLink *next;

  /* Free all links and their keys */
  for(i=0;i<table->nBuckets;i++)  {
    for(link=table->buckets[i];link;) {
      next=link->next;
      if(table->keyType == PeisHashTableKey_String)
	free(link->key.str);
      free(link);
      link=next;
    }
    table->buckets[i]=NULL;
  }

  /* Free the table */
  free(table);
}
void peisk_hashTable_clear(PeisHashTable *table) {
  int i;
  PeisHashTableLink *link;
  PeisHashTableLink *next;

  /* Free all links and their keys */
  for(i=0;i<table->nBuckets;i++)  {
    for(link=table->buckets[i];link;) {
      next=link->next;
      if(table->keyType == PeisHashTableKey_String)
	free(link->key.str);
      free(link);
      link=next;
    }
    table->buckets[i]=NULL;
  }
}
int peisk_hashTable_count(PeisHashTable *table) {
  int i,count;
  PeisHashTableLink *link;

  /* Free all links and their keys */
  for(i=0,count=0;i<table->nBuckets;i++)
    for(link=table->buckets[i];link;link=link->next) { count++; }
  return count;
}


int peisk_hashTable_getValue(PeisHashTable *table,void *key,void **value) {
  int hash;
  PeisHashTableLink *link;

  if(table->nBuckets==0) return PEISK_HASH_KEY_NOT_FOUND;
  hash=peisk_hashTable_hash(table,key);
  link=table->buckets[hash%table->nBuckets];

  if(table->keyType == PeisHashTableKey_String) {
    while(link) {
      if(hash == link->hash && strcasecmp(link->key.str,(char*)key) == 0) {
	*value = link->value; return 0;
      }
      link=link->next;
    }
  } else if(table->keyType == PeisHashTableKey_Integer) {
    while(link) {
      if(hash == link->hash && link->key.val == (int)(long) key) {
	*value = link->value; return 0;
      }
      link=link->next;
    }
  }
  return PEISK_HASH_KEY_NOT_FOUND;

  /*
  while(link) {
    if(hash == link->hash && 
       ((table->keyType == PeisHashTableKey_String && strcasecmp(link->key.str,(char*)key)==0)
	|| (table->keyType == PeisHashTableKey_Integer && link->key.val == (int)(long) key)
	)) {
      *value = link->value; return 0; 
    }
    link=link->next;
    }*/


}
int peisk_hashTable_remove(PeisHashTable *table,void *key) {
  int hash;
  PeisHashTableLink *link;
  PeisHashTableLink **prev;

  if(table->nBuckets==0) return PEISK_HASH_KEY_NOT_FOUND;
  hash=peisk_hashTable_hash(table,key);
  link=table->buckets[hash%table->nBuckets];
  prev=&table->buckets[hash%table->nBuckets];
  while(link) {
    if(hash == link->hash && 
       ((table->keyType == PeisHashTableKey_String && strcasecmp(link->key.str,(char*)key)==0)
	|| (table->keyType == PeisHashTableKey_Integer && link->key.val == (int)(long) key)
	)) {
      *prev=link->next;
      if(table->keyType == PeisHashTableKey_String)
	free(link->key.str);
      free(link);
      return 0;
    }
    prev=&link->next;
    link=link->next;
  }
  return PEISK_HASH_KEY_NOT_FOUND;
}

int peisk_hashTable_insert(PeisHashTable *table,void *key,void *value) {
  int hash, cost;
  PeisHashTableLink *link, *newLink;

  if(table->nBuckets==0) peisk_hashTable_resize(table,5);
  hash=peisk_hashTable_hash(table,key);
  /* First see if key already exits, if so just modify it. */
  link=table->buckets[hash%table->nBuckets];
  cost=0;
  while(link) {
    /* This also counts the "cost" of the hashtable to determine if we should resize it. The cost is the number of links that
       have different hash values but lie in the same bucket. 
    */
    if(hash != link->hash) cost++;
    if(hash == link->hash && 
       ((table->keyType == PeisHashTableKey_String && strcasecmp(link->key.str,(char*)key)==0)
	|| (table->keyType == PeisHashTableKey_Integer && link->key.val == (int)(long) key)
	)) {
      link->value = value; return 0; 
    }
    link=link->next;
  }  
  /* No previous value found, insert into hash table instead */
  newLink = (PeisHashTableLink*) malloc(sizeof(PeisHashTableLink));
  if(!newLink) return PEISK_HASH_OUT_OF_MEMORY;
  newLink->hash=hash;
  if(table->keyType == PeisHashTableKey_String)
    newLink->key.str = strdup((char*)key);
  else
    newLink->key.val = (int)(long) key;
  newLink->value = value;
  newLink->next = table->buckets[hash%table->nBuckets];
  table->buckets[hash%table->nBuckets] = newLink;
  /* Resize if new cost (links same bucket but with differnt hash) above threshold */
  if(cost + 1 > 3+table->nBuckets/10) peisk_hashTable_resize(table,table->nBuckets+table->nBuckets/2);
  return 0;
}

int peisk_hashTable_resize(PeisHashTable *table,int nNewBuckets) {
  int i,hash,nOldBuckets;
  PeisHashTableLink **oldBuckets;
  PeisHashTableLink *link, *next;

  if(nNewBuckets < 1) return PEISK_HASH_INVALID_ARG;
  nOldBuckets=table->nBuckets;
  oldBuckets=table->buckets;
  table->buckets = (PeisHashTableLink**) calloc(nNewBuckets,sizeof(PeisHashTableLink));  
  if(!table->buckets) { table->buckets=oldBuckets; return PEISK_HASH_OUT_OF_MEMORY; }
  table->nBuckets=nNewBuckets;

  /* Now iterate over all old links and put them in the new hashtable */
  for(i=0;i<nOldBuckets;i++)
    for(link=oldBuckets[i];link;) {
      next=link->next;
      hash = link->hash;
      link->next=table->buckets[hash%nNewBuckets];
      table->buckets[hash%nNewBuckets]=link;
      link=next;
    }
  /* Cleanup */
  if(oldBuckets) free(oldBuckets);
  return 0;
}

/** An array of all readble strings corresponding to all valid error codes */
const char *peisk_hashtable_errorStrings[PEISK_HASH_LAST_ERROR]={
  "Generic hashtable error", "Key not found", "Out of memory", "Invalid argument", 
};
const char *peisk_hashTable_strerror(int error) { 
  if(error >= 1 && error <= PEISK_HASH_LAST_ERROR) return peisk_hashtable_errorStrings[error-1];
  return "Unknown error code - not a valid hashtable error?";
}
void peisk_hashTableIterator_first(PeisHashTable *hashTable, PeisHashTableIterator *iterator) {
  iterator->bucket=0;
  iterator->link=NULL;
  iterator->hashTable=hashTable;
}
int peisk_hashTableIterator_next(PeisHashTableIterator *iterator) {
  if(iterator->link) iterator->link=iterator->link->next;
  while(iterator->link == NULL) {
    if(iterator->bucket >= iterator->hashTable->nBuckets) return 0;    
    iterator->link=iterator->hashTable->buckets[iterator->bucket];
    iterator->bucket++;
  }
  return 1;
}
int peisk_hashTableIterator_value(PeisHashTableIterator *iterator,void **key,void **value) {
  /*PEISK_ASSERT(iterator->link,("HashTable iterator pointing outside hashTable, not initialized or not called _next??"));*/
  if(!iterator->link) return 0;
  *key=(void*)(long)iterator->link->key.val;
  *value=iterator->link->value;
  return 1;
}
