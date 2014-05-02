/** \file values.h
    \brief Declares the functionalities for manipulating values in tuplescript
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

#ifndef VALUES_H
#define VALUES_H

typedef enum { eError=0, eAtom, eConstString, eDynamicString, eFloat, eInt, eTuple, eResultSet, eDictionary, eCons, eLambda, eProgram } EValueKind;
typedef struct Value {
  /** \brief Gives the type of this value */
  EValueKind kind;
  /** \brief Reference counting (used during GC) and to keep track of unused memory objects.  */
  int refCnt;
  /** \brief The data part of the value, or a pointer to a memory area containing the data */
  union {
    int token;
    int ivalue;
    double dvalue;
    char *string;
    PeisTuple *tuple;
    PeisTupleResultSet *rs;
    /** \brief Associates each variable token (int) to a Value pointer. When used to store environments, the parent environment (if any) is given by the special variable _top */
    PeisHashTable *bindings;
    struct {
      struct Value *first;
      struct Value *rest;
    } cons;
    struct Program *program;
    struct LambdaObject *lambda;
  } data;
  /** \brief The real memory area to free when this object is removed which may differ from the data pointer. */
  void *freeArea;
} Value;

/** \brief Content for lambda objects. */
typedef struct LambdaObject {
  /** \brief Reference to the program object containing the instructions for this lambda */
  Value *program;
  /** \brief Reference to parent environment of this lambda */
  Value *environment;
  /** \brief Instruction number in program object containing the argument list and body of lambda object */
  int pcstart;
  /** \brief Count of how many arguments this function expects */
  int nargs;
} LambdaObject;

void fprintfValue(FILE *fp,Value *value);
char *value2string(Value *value,char *buf,int len);
int value2int(Value *value);
double value2double(Value *value);

void freeValue(Value *value);
int value2boolean(Value *value);
/** Returns the next value contained within the given container value. Updates the container to point to the next value. If the container is empty, the EOF atom value is returned. */
Value *value2pop(Value *container);

/** \brief Creates the initial heap of values */
void initHeap();

/** \brief Gives the next free value from the heap, and/or performs any GC'ing etc. as needed. Returns a Value with a refCnt of one */
Value *allocateValue();

/** \brief Unreferences a value */
void unreference(Value *value);
/** \brief References a value */
void reference(Value *value);

/** \brief Constructs a new Atom value with an initial reference count of one */
Value *atom2value(int atom);
/** \brief Constructs a new string value pointing to an externaly managed char buffer (ie. constant from our point of view) with an initial reference count of one */
Value *cstring2value(char *string);
/** \brief Constructs a new string value pointing to a char buffer managed by us, with an initial reference count of one */
Value *dstring2value(char *string);
/** \brief Constructs a new integer value */
Value *int2value(int n);
/** \brief Returns a true/false value object */
Value *boolean2value(int b);
/** \brief Constructs a new double floatingpoint value */
Value *double2value(double d);
/** \brief Constructs a new tuple value pointing directly into the local (cache of) tuples */
Value *tuple2value(PeisTuple *tuple);
/** \brief Constructs a new result-set value pointing directly into the local (cache of) tuples */
Value *rs2value(PeisTupleResultSet *rs);
/** \brief Constructs a new cons-cell value with empty contents. */
Value *cons2value(); 
/** \brief Constructs a new environment value object */
Value *environment2value(Value *parent,PeisHashTable *this);
/** \brief Constructs a new program value object */
Value *program2value(struct Program *program);
/** \brief Construct a new lambda value from a lambda object description */
Value *lambda2value(struct LambdaObject *lobj);

/** \brief Prints a dump of the current heap to stdout */
void dumpHeap();

#endif
