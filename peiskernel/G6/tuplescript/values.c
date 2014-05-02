/** \file values.c 
    \brief Implements the functionalities for manipulating values in tuplescript
*/
/*
   Copyright (C) 2011  Mathias Broxvall

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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include "peiskernel_mt.h"
#include "tokenizer.h"
#include "hashtable.h"
#include "values.h"
#include "tuplescript.h"

/** \brief Pointer to heap containing all values in the interpreter */
Value *heap;
/** \brief Allocated size of the heap */
int heapSize;
/** \brief Currently used size of the heap, points to next allocatable value */
int nextValue;
/** \brief Maximum depth to which recursive prints will be made */
int maxPrintDepth=3;

void initHeap() {
  /* Initial heap of values, for now quite small to help us debug GC'ing */
  heapSize=64;

  /* Allocate an initial heap of potential values */
  heap = malloc(sizeof(Value) * heapSize); 
  memset((void*)heap,0,sizeof(Value) * heapSize);
  nextValue=0;
}

void gc() {
  printf("TODO: implement a garbage collector\n");
}
extern Value *value, *value2, *value3, *key, *environment;

void dumpHeap() {
  int i;
  for(i=0;i<heapSize;i++)
    if(heap[i].refCnt>0) {
      printf("%d: %d-> ",i,heap[i].refCnt); fprintfValue(stdout,&heap[i]); printf("\n");
    }
}

Value *allocateValue() {
  int newHeapSize;
  Value *heap2;

  for(;nextValue<heapSize;nextValue++) if(heap[nextValue].refCnt == 0) {heap[nextValue].refCnt=1; return &heap[nextValue++]; }
  nextValue=0;
  for(;nextValue<heapSize;nextValue++) if(heap[nextValue].refCnt == 0) {heap[nextValue].refCnt=1; return &heap[nextValue++]; }
  printf("Heap is full, need to GC\n");
  gc();
  for(;nextValue<heapSize;nextValue++) if(heap[nextValue].refCnt == 0) {heap[nextValue].refCnt=1; return &heap[nextValue++]; }
  printf("Heap still full after a GC, need to reallocate\n");

  newHeapSize = heapSize + heapSize/2;
  heap2 = realloc(heap,sizeof(Value)*newHeapSize);
  if(heap2 != heap) {
    printf("New heap is in alternative memory area. Moving and updating pointers\n"); 
    long long heapDiff = heap2-heap;
    int i;
    /* Fix execution state variables */
    if(value) value = (value-heap) + heap2;
    if(value2) value2 = (value2-heap) + heap2;
    if(value3) value3 = (value3-heap) + heap2;
    if(key) key = (key-heap)+heap2;
    if(environment) environment = (environment-heap)+heap2;

    /* Go through value stack and update all references */
    for(i=0;i<valueDepth;i++) 
      if(valueStack[i]) valueStack[i] = (valueStack[i]-heap)+heap2;
    /* Go through control stack and update all refereces */
    for(i=0;i<controlDepth;i++) { 
      switch(control[i].kind) {
      case eForLoop:
      case eFunction:
	control[i].iterant = (control[i].iterant-heap)+heap2;
	break;
      }
    }
    /* Go through stack of program value objects and update references */
    for(i=0;i<=frameDepth;i++)
      programsVal[i] = (programsVal[i]-heap)+heap2;

    /* Go through the existing heap and move all references inside it */
    for(i=0;i<heapSize;i++) {
      Value *val=&heap2[i];
      switch(val->kind) {
      case eDictionary:
	{
	  PeisHashTableIterator iterator;
	  for(peisk_hashTableIterator_first(val->data.bindings,&iterator);
	      peisk_hashTableIterator_next(&iterator);) {
	    long long key;
	    Value *val2;
	    if(peisk_hashTableIterator_value_generic(&iterator,&key,&val2)) {
	      if(val2) {
		//printf("%s -> %p becomes %s -> %p\n",token2string(key),val2,token2string(key),val2+heapDiff);
		peisk_hashTable_insert(val->data.bindings,(void*)key,(val2-heap)+heap2);
	      } //else printf("%s -> nil is left unchanged\n",token2string(key));
	    }
	  }
	}
	break;
      case eCons:
	val->data.cons.first = (val->data.cons.first-heap)+heap2;
	val->data.cons.rest = (val->data.cons.rest-heap)+heap2;
	break;
      case eLambda:
	if(val->data.lambda) {
	  LambdaObject *lobj = val->data.lambda;
	  lobj->program = (lobj->program-heap)+heap2;
	  lobj->environment = (lobj->environment-heap)+heap2;
	}
	break;
      }
    }
  }
  memset((void*)&heap2[heapSize],0,sizeof(Value)*(newHeapSize-heapSize));
  heap=heap2;
  nextValue=heapSize;
  heapSize=newHeapSize;
  heap[nextValue].refCnt=1;
  return &heap[nextValue++];
}
void unreference(Value *value) {
  if(!value) return;
  //printf("Unref: %p -> %d (kind %d, val '",value,value->refCnt-1,value->kind); fprintfValue(stdout,value); printf("')\n");
  if(!value->refCnt) {
    fprintf(stderr,"Attempting to un-reference value with zero refCnt. This is a bug in the interpreter!\n");
  } else if(--value->refCnt == 0) freeValue(value);
}
void reference(Value *value) { 
  //printf("Ref: %p -> %d (kind %d, val '",value,value->refCnt+1,value->kind); fprintfValue(stdout,value); printf("')\n");
  if(!value) return;
  value->refCnt++; 
}

Value *atom2value(int atom) { Value *v=allocateValue(); v->kind=eAtom; v->data.token=atom; v->freeArea=NULL; return v; }
Value *cstring2value(char *string) { Value *v=allocateValue(); v->kind=eConstString; v->data.string=string; v->freeArea=NULL; return v; }
Value *dstring2value(char *string) { Value *v=allocateValue(); v->kind=eDynamicString; v->data.string=string; v->freeArea=(void*) string; return v; }
Value *int2value(int n) { Value *v=allocateValue(); v->kind=eInt; v->data.ivalue=n; return v; }
Value *boolean2value(int b) { return atom2value(b?TRUE:FALSE); }
Value *double2value(double d) { Value *v=allocateValue(); v->kind=eFloat; v->data.dvalue=d; return v; }
Value *tuple2value(PeisTuple *tuple) { Value *v=allocateValue(); v->kind=eTuple; v->data.tuple=tuple; v->freeArea=NULL; return v; }
Value *rs2value(PeisTupleResultSet *rs) { Value *v=allocateValue(); v->kind=eResultSet; v->data.rs=rs; v->freeArea=(void*) rs; return v; }
Value *error2value() { Value *v=allocateValue(); v->kind=eError; return v; }
Value *environment2value(Value *parent, PeisHashTable *ht) { 
  Value *v=allocateValue(); 
  v->kind=eDictionary;
  v->data.bindings=ht; 
  peisk_hashTable_insert(ht,(void*)_TOP,(void*)parent);
  v->freeArea=(void*)NULL; 
  return v; 
}
Value *cons2value() { 
  Value *v=allocateValue(); v->kind=eCons; 
  v->data.cons.first=NULL; v->data.cons.rest=NULL; 
  return v;
}
Value *program2value(struct Program *program) { Value *v=allocateValue(); v->kind=eProgram; v->data.program=program; return v; }
Value *lambda2value(struct LambdaObject *lambda) { Value *v=allocateValue(); v->kind=eLambda; v->data.lambda = lambda; return v; }

void freeValue(Value *value) {
  PeisHashTableIterator iterator;
  switch(value->kind) {
  case eError: case eAtom: case eConstString: break;
  case eInt: case eFloat: break;
  case eTuple: break;
  case eDynamicString:
    free(value->freeArea);
   break;
  case eResultSet:
    peiskmt_deleteResultSet(value->data.rs);
    break;
  case eDictionary:
    for(peisk_hashTableIterator_first(value->data.bindings,&iterator);
	peisk_hashTableIterator_next(&iterator);) {
      long long key;
      Value *val2;
      if(peisk_hashTableIterator_value_generic(&iterator,&key,&val2))
	unreference(val2);
    }    
    break;
  case eCons:
    if(value->data.cons.first) unreference(value->data.cons.first);
    if(value->data.cons.rest) unreference(value->data.cons.rest);
    break;
  case eProgram:
    if(value->data.program) free(value->data.program);
    break;
  case eLambda:
    if(value->data.lambda) {
      if(value->data.lambda->program) unreference(value->data.lambda->program);
      if(value->data.lambda->environment) unreference(value->data.lambda->environment);
      free(value->data.lambda);
    }
    break;
  default:
    fprintf(stderr,"warning: Freeing unknown value of kind %d\n",value->kind);
  }
  value->kind=eError;
}


void fprintfValue(FILE *fp,Value *value) {
  char s[256];
  PeisTupleResultSet *rs;
  PeisTuple *tuple;

  int keepPrinting, isContinued=0;
  static int printDepth=0;

  if(++printDepth > maxPrintDepth) { fprintf(fp,"..."); printDepth--; return; }
  if(!value) { fprintf(fp,"nil"); printDepth--; return; }
  do {
    keepPrinting=0;
    switch(value->kind) {
    case eError: fprintf(fp,"<illegal value>"); break;
    case eAtom: fprintf(fp,"%s",token2string(value->data.token)); break;
    case eConstString: case eDynamicString: fprintf(fp,"%s",value->data.string); break;
    case eInt: fprintf(fp,"%d",value->data.ivalue); break;
    case eFloat: fprintf(fp,"%f",value->data.dvalue); break;
    case eTuple: 
      tuple=value->data.tuple;
      peiskmt_getTupleName(tuple,s,sizeof(s));
      fprintf(fp,"<o:%d k:%s d:%s>",tuple->owner,s,tuple->data); 
      break;
    case eResultSet:
      rs=value->data.rs;
      for(peiskmt_resultSetFirst(rs);peiskmt_resultSetNext(rs);) {
	tuple = peiskmt_resultSetValue(rs);
	peiskmt_getTupleName(tuple,s,sizeof(s));
	fprintf(fp,"<o:%d k:%s d:%s> ",tuple->owner,s,tuple->data);
      }
      break;
    case eCons:
      if(!isContinued) fprintf(fp,"(");
      fprintfValue(fp,value->data.cons.first);
      if(value->data.cons.rest == NULL) { fprintf(fp,")"); break; }
      fprintf(fp," ");
      if(value->data.cons.rest->kind == eCons) {
	value=value->data.cons.rest;
	keepPrinting=1; isContinued=1;
      } else {
	fprintf(fp,". "); 
	fprintfValue(fp,value->data.cons.rest);
	fprintf(fp,")"); 
      }
      break;
    case eDictionary:
      {
	PeisHashTableIterator iterator;
	fprintf(fp,"{");
	
	for(peisk_hashTableIterator_first(value->data.bindings,&iterator);
	    peisk_hashTableIterator_next(&iterator);) {
	  long long key;
	  Value *val2;
	  if(peisk_hashTableIterator_value_generic(&iterator,&key,&val2)) {
	    if(isContinued) fprintf(fp," ");
	    if(val2) { 
	      fprintf(fp,"%s:",token2string(key)); 
	      fprintfValue(fp,val2);
	      isContinued=1;
	    } else {
	      fprintf(fp,"%s:NULL",token2string(key));
	    }
	  }
	}
	fprintf(fp,"}");
	break;
      }
    case eProgram:
      fprintf(fp,"<program:%s>",value->data.program?value->data.program->name:"unknown");
      break;
    case eLambda:
      {
	LambdaObject *lambda = value->data.lambda;
	if(lambda && lambda->program->data.program && lambda->pcstart>=0 && lambda->pcstart < lambda->program->data.program->maxInstructions) 
	  fprintf(fp,"<lambda:%s@%d>",lambda->program->data.program->name,lambda->program->data.program->instructions[lambda->pcstart].lineno);
	else fprintf(fp,"<lambda:NULL>");
      }
      break;
    default:
      fprintf(fp,"<unknown datatype %d>",value->kind);
    }
  } while(keepPrinting);
	printDepth--; 
}

char *value2string(Value *value,char *buf,int len) {
  char *res=buf;
  char s[256];
  int i;
  PeisTupleResultSet *rs;
  PeisTuple *tuple;
  
  if(!value) { snprintf(buf,len,"nil"); return buf; }
  switch(value->kind) {
  case eError: return "<illegal value>"; break;
  case eAtom: return token2string(value->data.token); break;
  case eConstString: case eDynamicString: return value->data.string; break;
  case eInt: snprintf(buf,len,"%d",value->data.ivalue); return buf; 
  case eFloat: snprintf(buf,len,"%f",value->data.dvalue); return buf; 
  case eTuple: 
    tuple=value->data.tuple;
    peiskmt_getTupleName(tuple,s,sizeof(s));
    //snprintf(buf,len,"<tuple %d %p> ",peiskmt_peisid(),(void*)tuple);    
    snprintf(buf,len,"<o:%d k:%s d:%s>",tuple->owner,s,tuple->data); 
    return buf;
    break;
  case eResultSet:
    rs=value->data.rs;
    for(peiskmt_resultSetFirst(rs);peiskmt_resultSetNext(rs)&&len;) {
      tuple = peiskmt_resultSetValue(rs);
      peiskmt_getTupleName(tuple,s,sizeof(s));
      i=snprintf(res,len,"<o:%d k:%s d:%s> ",tuple->owner,s,tuple->data);
      res += i; len -= i;
    }
    return buf;
    break;
  default:
    snprintf(buf,len,"<unknown datatype %d>",value->kind);
    return buf;
  }
}
int value2boolean(Value *value) {
  if(!value) return 0;
  switch(value->kind) {
  case eError: return 0; break;
  case eAtom: return value->data.token != FALSE; break;
  case eConstString: case eDynamicString: return value->data.string[0] != 0;
  case eTuple: return value->data.tuple->data[0] != 0;
  case eResultSet: return !peiskmt_resultSetIsEmpty(value->data.rs);
  default: return 0;
  }
}
int value2int(Value *value) {
  if(!value) return 0;
  switch(value->kind) {
  case eAtom: return atoi(token2string(value->data.token));
  case eConstString: case eDynamicString: return atoi(value->data.string);
  case eInt: return value->data.ivalue; 
  case eFloat: return (int) value->data.dvalue;
  default: return 0;
  }
}
double value2double(Value *value) {
  if(!value) return 0;
  switch(value->kind) {
  case eAtom: return atof(token2string(value->data.token)); break;
  case eConstString: case eDynamicString: return atof(value->data.string); break;
  case eInt: return (double) value->data.ivalue;
  case eFloat: return value->data.dvalue;
  default: return 0;
  }
}


Value *value2pop(Value *value) {
  int i;
  switch(value->kind) {
  case eError: return value; 
  case eAtom: return error2value();
  case eConstString:
  case eDynamicString:
    /* We use the tokenizer to split up the content of the value and to return the different parts of it. 
       Change the pointer of the string (without touching the memory to be free'd pointer) to update what will be tokenized on the next call. 
     */
    return atom2value(sgetToken(&value->data.string,&i));
  case eResultSet:
    if(peiskmt_resultSetIsEmpty(value->data.rs)) return atom2value(EOF);
    else {
      peiskmt_resultSetNext(value->data.rs);
      return tuple2value(peiskmt_resultSetValue(value->data.rs));
    }
  }
}

