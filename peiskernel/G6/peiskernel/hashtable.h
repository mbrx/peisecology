/** \file hashtable.h
    Generic routines for manipulating hashtables and other datatypes. 
    Although intended to be private to peiskernel it can also be used by applications
    if these functionalitites are required.
*/
/*
    Copyright (C) 2005 - 2011  Mathias Broxvall

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

/** \ingroup peisk */
/*\{@*/
/** \defgroup Hashtable Generic hashtables in C

    This is a module containing fairly generic support (in C) for
    hashtables with keys using either integer or string data. Although
    intended to be private to peiskernel it can also be used by
    applications if these functionalitites are required. 

    It can store any data in the values field. Currently this module
    have severe problems under non 32-bit architectures. In the future
    the API interface to these function will change, so end-users
    should beware!
*/ 
/*\{@*/

#ifndef HASHTABLE_H
#define HASHTABLE_H

typedef enum { PeisHashTableKey_String=0, PeisHashTableKey_Integer=1 } PeisHashTableKeyType;

/** Container for (key,value) pairs in the hashtables. */
typedef struct PeisHashTableLink {
  int hash;
  union {
    char *str; /**< A copy (strdup) of string keys */
    long val;   /**< Integer valued keys */
  } key;       /**< The key for this link */
  void *value;        /** The value for this link, this is not copied by the hashtable routines. User must ensure that values are persistent */

  struct PeisHashTableLink *next;
} PeisHashTableLink;

/** Datatype for generic hashtable indexed by C-strings. */
typedef struct PeisHashTable {
  int nBuckets;
  struct PeisHashTableLink **buckets; /** Array of pointer to links (lists) */
  PeisHashTableKeyType keyType;
} PeisHashTable;

typedef struct PeisHashTableIterator { 
  int bucket;
  struct PeisHashTableLink *link;
  struct PeisHashTable *hashTable;
} PeisHashTableIterator;

/** Creates a new hashtable. */
PeisHashTable *peisk_hashTable_create(PeisHashTableKeyType keyType);
/** Deletes a hashtable and deallocates used memory. */
void peisk_hashTable_delete(PeisHashTable *);
/** Removes all content from a hashTable */
void peisk_hashTable_clear(PeisHashTable *);
/** Counts how many entries have in a hashTable */
int peisk_hashTable_count(PeisHashTable *);
/** Gets the value associated to given key in hashtable (need to
    typecast correct key into void*). 
    Returns zero on success and updates value of given void**, returns
    error code on failure. */
int peisk_hashTable_getValue(PeisHashTable *,void *key,void **value);
/** Removes the given key from hash table. Returns zero on success, error code on failure. */
int peisk_hashTable_remove(PeisHashTable *,void *key);  
/** Inserts another value to hashtable, possibly resizing number of buckets if neccessary. Returns zero on success, error code on failure. */
int peisk_hashTable_insert(PeisHashTable *,void *key,void *value);

/** Initialize a hashTable iterator to point to just before the first entry in hashtable. 
    You must call the *_next function before asking for the first value. 
    Never change a hashtable when iterating over it. 
    Note: It is allowed to remove elements from a hashtable while
    iterating over it *as long as the the element currently pointed to
    by the iterator is not removed*. Ie. if you want to delete the
    current element then do a _next call before deleting it. */
void peisk_hashTableIterator_first(PeisHashTable *hashtable, PeisHashTableIterator *iterator);
/** Steps a hashTable iterator to point to the next value. Returns non zero if successfull. */
int peisk_hashTableIterator_next(PeisHashTableIterator *iterator);
/** \brief Retrieves key/value pair currently pointed to by iterator. 

    Returns non zero if succcesfull. WARNING: This function should
    rarly be used, use the macro peisk_hashTableIterator_value_generic
    instead to make sure that you treat the size of double pointers
    correctly for the return value.  */
int peisk_hashTableIterator_value(PeisHashTableIterator *iterator,void **key,void **value);

/** \brief Convinience macro for retriving key/value from a given
    iterator into the two given variables. 

    Type of key should be a pointer to "long long" or "char *". 
    Type of value should be a pointer to "long long" integer or any kind of pointer. */
#define peisk_hashTableIterator_value_generic(iterator,keyp,valuep) ({void *_keyp, *_valuep; int _success=peisk_hashTableIterator_value(iterator,&_keyp,&_valuep); *keyp = (typeof(*keyp)) _keyp; *valuep = (typeof(*valuep)) _valuep; _success; })

/** Changes number of buckets in hashtable and rebuilds it. Returns non zero if succcesfull. */
int peisk_hashTable_resize(PeisHashTable *,int nBuckets);


/* These are the error codes that can be returned from the hashtable functions */
/** Generic error covering all kinds of failures */
#define PEISK_HASH_GENERIC_ERROR 1
/** Given key did not exist in hashtable */
#define PEISK_HASH_KEY_NOT_FOUND 2
/* Failed to allocate memory */
#define PEISK_HASH_OUT_OF_MEMORY 3
/* One of the arguments have an invalid value */
#define PEISK_HASH_INVALID_ARG   4
/* Total number of possible error codes */
#define PEISK_HASH_LAST_ERROR    4

/** Converts a hashtable error code to a printable string */
const char *peisk_hashTable_strerror(int error);

/** Ugly inline definition to be used only inside the peiskernel. It's fast but sacrificing safety and elegance. This requires the C compiler to have the typeof compile-time operator, all GCC derivatives have it. */
#define peisk_hashTableIterator_value_fast(_it,_k,_v) {*(_k)=(typeof(*_k))(_it)->link->key.val; *(_v)=(typeof(*_v))(_it)->link->value;}

/*\}@ Hashtables */
/*\}@ Ingroup  peisk */

#endif
