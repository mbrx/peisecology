/** \file tuplescript.h
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

#ifndef TUPLESCRIPT_H
#define TUPLESCRIPT_H

typedef struct Instruction {
  int token;
  int lineno;
} Instruction;

/** \brief Represents a tokenized program. This is an abstract non-executed entity that requires an execution state (eg. PC) before it is an actual process. */
typedef struct Program {
  char *name;
  int maxInstructions, nInstructions;
  Instruction *instructions;
} Program;

typedef enum { eCommandLine=0, eFile, eStdin } ESourceKind;
typedef struct Source {
  ESourceKind kind;
  int lineno;
  char *name;
  FILE *fp;
} Source;

typedef enum { eBlock=0, eIfStatement, eElseStatement, eForLoop, eFunction } EControlKind;
typedef struct ControlFlow {
  EControlKind kind; 
  /** \brief Operation stack where this control flow op. started, prevents closing OP's until control block is finished */
  int opStart; 
  /** \brief Gives the instruction number where the statement started */
  int origin;
  /** \brief Frame at which control block originated */
  int originFrameDepth;
  /** \brief Gives the latest boolean evaluation (only used in IF statements) */
  char boolean;          
  /** \brief Only used in FOR loops */
  int controlVariable;   
  /** \brief Used as iterant in FOR loops or return environment in function blocks */
  Value *iterant;         
} ControlFlow;

typedef struct Environment {
  /** Reference countable objects should always have the refcount first so that it is safe to typecast them */
  int refcount; 

  /** Surrounding environment, or NULL if the top environment */
  struct Environment *parent;
  /** Hashtable associating tokens to the corresponding values */
  PeisHashTable *bindings;
} Environment;

typedef struct  {
  int operand, lineno, valueStackStart;  
} OperandState;

#define TOKENS(constName,cName) constName ,
/** \brief List of all tokens, as given by the X-Macro TOKENS and tokens.def */
typedef enum Tokens { 
  NOT_A_TOKEN=0, 
#include "tokens.def"
  LAST_KEYWORD
} Tokens;

/** \brief Maximum depth of program frames (loaded files, function calls, lambdas) */
#define MAX_FRAME_DEPTH 64
/** \brief Maximum number of nested conditional statement */
#define MAX_CONTROL_FLOW 256
/** \brief Maximum number of operators that can be stacked */
#define MAX_OP_DEPTH 1024
/** \brief Maximum number of value operators that can be stacked */
#define MAX_VALUE_DEPTH 1024

/** \brief Executes the current program and reads tokens from the current source input as needed. */
void run();
/** \brief Gets the next instruction/data token from the current source */
Instruction *getInstruction();
/** \brief Gets the next value from the currently executing program */
Value *getValue();
/** \brief Skips execution until we find the next RC_BRACKET, keeping any increasing LC_BRACKET in mind  */
void skipBlock(Instruction *from);
/** \brief True if the given atom is a reserved keyword */
int isReserved(int atom);

/** \brief Gives the parent environment of the given environment object (if available) */
Value *environmentParent(Value *env);
/** \brief Performs a lookup in the variable and function dictionaries from the current environment and up. Sets isFun to non-zero if result came from function dictionary. */
Value *lookupFunVariable(Value *environment,long token,char *isFun);
/** \brief Createss (or overwrite) the corresponding local variable */
void createLocalBinding(Value *environment,long token,Value *value);
/* \brief Assigns to a variable. \detail If the correspodning variable exists
   anywhere in the environments, it will be used. Otherwise a
   new global variable will be created. */
void assignVariable(Value *environment,long token,Value *value);

/*****************************/
/* Execution state variables */
/*****************************/

/** Pointer to currently executed program */
extern Program *programs[MAX_FRAME_DEPTH];
/** Value object holding currently executed program */
extern Value *programsVal[MAX_FRAME_DEPTH];
/** Program counter for currently executed program */
extern int pc[MAX_FRAME_DEPTH];
extern Source sources[MAX_FRAME_DEPTH];
/** Depth of file/source inputs */
extern int frameDepth;

/** Value used in the execution state that may 
    need to be reallocated and/or gc'ed */
extern Value *value, *value2, *value3, *key;

extern OperandState opStack[MAX_OP_DEPTH];
extern Value *valueStack[MAX_VALUE_DEPTH];
extern int opDepth, valueDepth;

extern ControlFlow control[MAX_CONTROL_FLOW];
extern int controlDepth;

/** \brief Stores all variables in the current environment, function definitions are stored within a special dictionary within these */
extern Value *environment;



#endif
