/** \file tuplescript.c
    \brief Main entry point of the tuplescript interpreter
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


/** \defgroup tuplescript The TupleScript Language

    Tuplescript is the name of both a small example language and the
    example interpreter of this language which is suitable for
    running small scripts that executes functionalities in the
    tuplespace --  similar to many shell scripting
    languages. The interpreter is fairly inefficient and is meant
    mostly as a convinient way of making scripted experiments and
    simple manipulations of the tuplespace - not for performing
    advanced computations.  
    
    Some of the capabilities of the scripting language include:

    - The basic notation for ALL expressions and statements are using
    prefix notation (aka. polish notation) with a fixed number of
    arguments to all operators and statements

    - The scripting language consists of multiple statements, where each
    statement has fixed number of arguments (zero or more). 

    - There is no explicit separation between statements. 

    - Conditional statements and loop constructs uses blocks of
    statements contained within curly brackers '{' and '}'. This is
    the only statement with a variable number of arguments

    - Write tuples to local and remote tuplespaces using the 'set'
      statement. To select the owner to which they are written use the
      'owner' statement (which persists until changed again).

    - Read singular tuples from the localhost tuplespace as variables
      $X reads tuple named X

    - Read singular tuples from remote tuplespaces (when subscribed)
      using !id:X

    - Read multiple tuples from any tuplespaces using @id.X

    - Read multiple abstract tuples from any tuplespaces using @<o=id
      n=X ...>

    - Statements and arguments can be given on the commandline

    - Load code from a file and/or stdin

    Also, remains as a member of the tuplespace until aborted.
    
    See also \ref simpledb.c and \ref tokenizer.c for comparisions
*/


/** \ingroup tuplescript */
/** \defgroup simpledb Simpledb
    Simpledb have been depracated in favour of tuplescript. See \ref tuplescript and \ref tuplescript.c
*/

/** \ingroup tuplescript */
/** \defgroup operations Tuplescript Operations */


#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include "peiskernel_mt.h"
#include "tokenizer.h"
#include "hashtable.h"
#include "values.h"
#include "tuplescript.h"

void printUsage(FILE *stream,short argc,char **args) {
  fprintf(stream,"%s options:\n",args[0]);
  fprintf(stream," --help                 Print usage information\n");
  //fprintf(stream," --load <file>          Load given file\n");
  fprintf(stream," --repl                 Run a REPL\n");
  peisk_printUsage(stream,argc,args);
  exit(0);
}

/*****************************/
/* Execution state variables */
/*****************************/

Program *programs[MAX_FRAME_DEPTH];
Value *programsVal[MAX_FRAME_DEPTH];
int pc[MAX_FRAME_DEPTH];
Source sources[MAX_FRAME_DEPTH];
int frameDepth=0;

/* Values used in the execution state that may 
   need to be reallocated and/or gc'ed */
Value *value, *value2, *value3, *key;

Value *environment;

/** Stack of special control flow statements */
ControlFlow control[MAX_CONTROL_FLOW];
int controlDepth=0;

/** Stack of operations/functioncalls to perform */
OperandState opStack[MAX_OP_DEPTH];
/** Stack of values to be applied to the operations */
Value *valueStack[MAX_VALUE_DEPTH];
int opDepth, valueDepth, valueStart;

int debug=0;

int owner=-1;     /**< Default owner used when setting tuples */
int hold=0;       /**< If we should hold (stay alive) after settings tuples or quit */
int blocking=0;   /**< If local and remote tuple read operations should be blocking */
int repl=1;
int indentation=0;
int doQuit=0;

int CLargc;
char **CLargs;
int CLnext;

void makePrompt() {
  int i;
  char *s;
  static char prompt[256];
  readlinePromptType=prompt;
  sprintf(prompt,"> ");
  s=prompt+strlen(prompt);
  for(i=0;i<indentation;i++) s += sprintf(s,"  ");
}

#define MAX_OP_ARGS 6
#define NO_ARG 0
#define VALUE_ARG -1
#define TUPLE_ARG -2
#define ATOM_ARG -3
int opArgs[LAST_KEYWORD][MAX_OP_ARGS];

int main(int argc,char **args) {
  int i,j;

  /* Initialize tuplespace */
  peiskmt_initialize(&argc,args);
  initTokenizer();
  initHeap();
  makePrompt();    

  /* Hardcoded tokens as given by the X-macro TOKENS in tokens.def */
#define TOKENS(constName,cName) insertToken(cName);
#include "tokens.def"

  for(i=0;i<LAST_KEYWORD;i++) opArgs[i][0]=-1;
#define OP0(NAME) { opArgs[NAME][0]=0; }
#define OP1(NAME,TYPE) { opArgs[NAME][0]=1; opArgs[NAME][1]=TYPE; }
#define OP2(NAME,T1,T2) { opArgs[NAME][0]=2; opArgs[NAME][1]=T1; opArgs[NAME][2]=T2; }
#define OP3(NAME,T1,T2,T3) { opArgs[NAME][0]=3; opArgs[NAME][1]=T1; opArgs[NAME][2]=T2; opArgs[NAME][3]=T3;}
#define OP4(NAME,T1,T2,T3,T4) { opArgs[NAME][0]=4; opArgs[NAME][1]=T1; opArgs[NAME][2]=T2; opArgs[NAME][3]=T3; opArgs[NAME][4]=T4;}

  OP1(ECHO,VALUE_ARG); OP1(LOAD,VALUE_ARG); 
  OP2(SET,TUPLE_ARG,VALUE_ARG); 
  OP2(SET_WAIT,TUPLE_ARG,VALUE_ARG);
  OP2(APPEND,TUPLE_ARG,VALUE_ARG);
  OP1(SUBSCRIBE,TUPLE_ARG);
  OP2(DECLARE_META,TUPLE_ARG,VALUE_ARG);
  OP2(SET_META,TUPLE_ARG,TUPLE_ARG);
  OP2(LET,ATOM_ARG,VALUE_ARG);
  OP1(SLEEP,VALUE_ARG); OP0(HOLD); OP0(FINISH); OP0(EXIT); OP0(BLOCKING); OP0(NON_BLOCKING); 
  OP0(DEBUG); OP0(NEWLINE);
  OP2(IF,VALUE_ARG,LC_BRACKET);
  OP0(TRUE); OP0(FALSE);
  OP1(ELSE,LC_BRACKET);
  OP0(LC_BRACKET);
  OP0(RC_BRACKET);
  OP0(COMMA);
  OP1(DOT_OWNER,VALUE_ARG);
  OP1(DOT_KEY,VALUE_ARG);
  OP1(DOT_DATA,VALUE_ARG);
  OP1(DOLLAR,ATOM_ARG);
  OP1(PERCENT,TUPLE_ARG);  
  OP1(AT,TUPLE_ARG);
  OP1(AMPERSAND,TUPLE_ARG);
  OP4(FOR,ATOM_ARG,IN,VALUE_ARG,LC_BRACKET);
  OP2(LOCAL,ATOM_ARG,VALUE_ARG);
  OP0(CONTINUE); OP0(AGAIN);
  OP3(_TCONS,VALUE_ARG,COLON,ATOM_ARG);
  OP2(CONS,VALUE_ARG,VALUE_ARG);
  OP1(QUOTE,ATOM_ARG);
  OP0(NIL);
  OP1(L_PARAN,VALUE_ARG);
  OP0(R_PARAN);

  OP2(FIRST_SET,VALUE_ARG,VALUE_ARG);
  OP2(REST_SET,VALUE_ARG,VALUE_ARG);
  OP1(FIRST,VALUE_ARG);
  OP1(REST,VALUE_ARG);
  OP1(DOT_MIMETYPE,VALUE_ARG);
  OP1(LIST,VALUE_ARG);
  OP0(DUMP_HEAP);
  OP0(LAMBDA);  
  OP0(DEFUN);  
  OP1(CALL,VALUE_ARG);

  OP1(MINUS,VALUE_ARG);
  OP1(SYM_MINUS,VALUE_ARG);
  OP1(SUM,VALUE_ARG);
  OP1(PLUS,VALUE_ARG);
  OP1(SYM_PLUS,VALUE_ARG);
  OP1(PROD,VALUE_ARG);
  OP1(ASTERIX,VALUE_ARG);
  OP2(EQ,VALUE_ARG,VALUE_ARG);
  OP2(SYM_EQ,VALUE_ARG,VALUE_ARG);
  OP2(EQUAL,VALUE_ARG,VALUE_ARG);
  OP1(AGAIN,VALUE_ARG);  
  OP1(CONTINUE,VALUE_ARG);

  /* Use stdin as default input */
  frameDepth=0;
  int nextInput=0;

  for(i=1;i<argc;i++) {
    if(strcasecmp(args[i],"--help") == 0) printUsage(stdout,argc,args);
    else if(strcasecmp(args[i],"--repl") == 0) {
      repl=1;
      sources[nextInput].kind = eStdin;
      sources[nextInput].name = strdup("stdin");
      sources[nextInput].lineno = 0;
      programs[nextInput] = malloc(sizeof(Program));
      programs[nextInput]->name = sources[nextInput].name;
      programs[nextInput]->maxInstructions = 256;
      programs[nextInput]->nInstructions = 0;
      programs[nextInput]->instructions = malloc(sizeof(Instruction)*programs[nextInput]->maxInstructions);
      pc[nextInput] = 0;
      programsVal[nextInput] = program2value(programs[nextInput]);
      nextInput++;
    }
    else {
      if(nextInput==0) repl=0;
      sources[nextInput].kind = eFile;
      sources[nextInput].name = strdup(args[i]);
      sources[nextInput].fp = fopen(args[i],"rb");
      if(!sources[nextInput].fp) {
	fprintf(stderr,"Could not open '%s'\n",args[i]);
	continue;
      }
      sources[nextInput].lineno = 0;
      programs[nextInput] = malloc(sizeof(Program));
      programs[nextInput]->name = sources[nextInput].name;
      programs[nextInput]->maxInstructions = 256;
      programs[nextInput]->nInstructions = 0;
      programs[nextInput]->instructions = malloc(sizeof(Instruction)*programs[nextInput]->maxInstructions);
      pc[nextInput] = 0;
      programsVal[nextInput] = program2value(programs[nextInput]);
      nextInput++;
    }
  }
  if(nextInput == 0) {
    sources[0].kind = eStdin;
    sources[0].name = strdup("stdin");
    sources[0].lineno = 0;
    programs[nextInput] = malloc(sizeof(Program));
    programs[0]->name = sources[0].name;
    programs[0]->maxInstructions = 256;
    programs[0]->nInstructions = 0;
    programs[0]->instructions = malloc(sizeof(Instruction)*programs[0]->maxInstructions);
    pc[nextInput] = 0;
    programsVal[nextInput] = program2value(programs[nextInput]);
  }


  environment = environment2value(NULL,peisk_hashTable_create(PeisHashTableKey_Integer));

  /*CLargc=argc;
  CLargs=args;
  CLnext=1;*/

 
  if(repl) {
    while(!doQuit && peiskmt_isRunning()) { run(); } // flushReadline(); }
  }
  else { 
    run();   
    if(hold)
      while(!doQuit && peiskmt_isRunning()) { sleep(1); }
  }
  /*printf("I am impolite for now...\n");
    exit(0);*/
  peiskmt_shutdown();
}

Value *environmentParent(Value *env) {
  void *val;
  if(env->kind != eDictionary) return NULL;
  if(peisk_hashTable_getValue(env->data.bindings,(void*)_TOP,&val) == 0)
    return (Value*) val;
  else return NULL;
}
void bindFunction(Value *environment,long name,Value *value) {
  Value *funcDict;
  void *val;
  if(peisk_hashTable_getValue(environment->data.bindings,(void*)_FUNCTION_DICT,&val) == 0) {
    funcDict=(Value*) val;
  } else {
    funcDict=environment2value(NULL,peisk_hashTable_create(PeisHashTableKey_Integer));
    peisk_hashTable_insert(environment->data.bindings,(void*)_FUNCTION_DICT,(void*)funcDict);
  }
  createLocalBinding(funcDict,name,value);
}
Value *lookupFunVariable(Value *environment,long token,char *isFun) {
  *isFun=0;
  while(environment) {
    void *val;
    if(peisk_hashTable_getValue(environment->data.bindings,(void*)token,&val) == 0) 
      return (Value*) val;
    else {
      if(peisk_hashTable_getValue(environment->data.bindings,(void*)_FUNCTION_DICT,&val) == 0) {
	Value *funcDict; funcDict=(Value*) val;
	if(peisk_hashTable_getValue(funcDict->data.bindings,(void*)token,&val) == 0) {
	  *isFun=1;
	  return (Value*) val;	
	}
      }
    }
    environment=environmentParent(environment);
  }
  return NULL;
}
void createLocalBinding(Value *environment,long token,Value *value) {
  void *oldval;
  reference(value);
  if(peisk_hashTable_getValue(environment->data.bindings,(void*)token,&oldval) == 0) 
    unreference((Value*)oldval);
  peisk_hashTable_insert(environment->data.bindings,(void*)token,(void*)value);
}
void assignVariable(Value *environment,long token,Value *value) {
  Value *env=environment;
  Value *prevEnv=NULL;
  void *oldval;
  while(env) {
    if(peisk_hashTable_getValue(env->data.bindings,(void*)token,&oldval) == 0) {
      reference(value);
      unreference((Value*)oldval);
      peisk_hashTable_insert(env->data.bindings,(void*)token,(void*)value);
      return;
    } else {
      prevEnv=env;
      env=environmentParent(env); 
    }
  }
  /* No previous such variable binding found, create a new one as a top level variable */
  if(prevEnv) {
    reference(value);
    peisk_hashTable_insert(prevEnv->data.bindings,(void*)token,(void*)value);    
  } else {
    fprintf(stderr,"Error - no global environment found while assinging value to variable %s... this is a bug in the interpreter!\n",token2string(token));
  }
}

Instruction *getInstruction() {
  Program *program = programs[frameDepth];
  Source *source = &sources[frameDepth];
  if(pc[frameDepth] < program->nInstructions) return &program->instructions[pc[frameDepth]++];
  int token;
  switch(source->kind) {
  case eCommandLine:
    if(CLargc==CLnext) token=TOKEN_EOF;
    else {
      if(CLargs[CLnext][0] == '-' && CLargs[CLnext][1] == '-') token=insertToken(CLargs[CLnext++]+2);
      else token=insertToken(CLargs[CLnext++]); 
      source->lineno++;
    }
    break;
  case eFile:
    token=getToken(source->fp,&source->lineno);
    break;
  case eStdin:
    token=readlineToken(&source->lineno);
    if(token == TOKEN_EOF) { doQuit=1; return NULL; }
    break;
  default:
    fprintf(stderr,"Error - unknown source type\n");
    exit(0);
  }
  if(program->maxInstructions == program->nInstructions) {
    program->maxInstructions *= 2;
    program->instructions = realloc(program->instructions,sizeof(struct Instruction)*program->maxInstructions);
  }
  program->instructions[program->nInstructions].lineno = source->lineno;
  program->instructions[program->nInstructions].token = token;
  program->nInstructions++;
  return &program->instructions[pc[frameDepth]++];
}


void skipBlock(Instruction *instruction) {
  Instruction *i2;
  Program *program = programs[frameDepth];

  int cnt;
  for(cnt=1;cnt;) {
    i2 = getInstruction();
    if(!i2) {
      fprintf(stderr,"%s:%d -- %d Syntax error - expected closing '}''\n",program->name,instruction->lineno,i2->lineno);	    
      return;
    }
    if(i2->token == LC_BRACKET) cnt++;
    if(i2->token == RC_BRACKET) cnt--;
  }
}

int isReserved(int atom) { return atom<LAST_KEYWORD; }

void aritBResult(int b) {
  if(valueStack[valueStart]->refCnt == 1 && 
     (valueStack[valueStart]->kind == eInt || 
      valueStack[valueStart]->kind == eFloat || 
      valueStack[valueStart]->kind == eConstString)) {
    valueStack[valueStart]->kind = eAtom;
    valueStack[valueStart]->data.token = b?TRUE:FALSE;
  } else {
    unreference(valueStack[valueStart]);
    valueStack[valueStart]=boolean2value(b);
  }
}
void aritIResult(int i) {
  if(valueStack[valueStart]->refCnt == 1 && 
     (valueStack[valueStart]->kind == eInt || 
      valueStack[valueStart]->kind == eFloat || 
      valueStack[valueStart]->kind == eConstString)) {
    valueStack[valueStart]->kind = eInt;
    valueStack[valueStart]->data.ivalue = i;
  } else {
    unreference(valueStack[valueStart]);
    valueStack[valueStart]=int2value(i);
  }
}
void aritDResult(double d) {
  if(valueStack[valueStart]->refCnt == 1 && 
     (valueStack[valueStart]->kind == eInt || 
      valueStack[valueStart]->kind == eFloat || 
      valueStack[valueStart]->kind == eConstString)) {
    valueStack[valueStart]->kind = eFloat;
    valueStack[valueStart]->data.dvalue = d;
  } else {
    unreference(valueStack[valueStart]);
    valueStack[valueStart]=double2value(d);
  }
}

void run() {
  Instruction *instruction, *i2;
  
  PeisTuple *tuple;
  PeisTupleResultSet *rs;

  float valf;
  int vald;
  int o,mo;
  char s[256];
  int cnt;
  /** Stores the program counter at the start of the current statement */
  int pcStart;
  int i;

  char val_buf[1024], key_buf[256], own_buf[256], mkey_buf[256], mown_buf[256];
  char *v,*k, *str, *mv, *mk;
  double d;
  long l;

  /** \brief Use this macro to check for a specific keyword within the main run dispatcher. Used for simple syntactical checks in statements with syntactic sugar */
#define EXPECT(keyword,errMsg) {i2=getInstruction(); if(!i2) return; if(i2->token != keyword) { fprintf(stderr,"%s:%d Syntax error - %s",program->name,instruction->lineno,errMsg); return; } }

#define IS_STRING(v,errMsg) {if(!v || (v->kind != eConstString && v->kind != eDynamicString)) { fprintf(stderr,"%s:%d - %s",programs[depth]->name,instruction->lineno,errMsg); return; }}
#define IS_ANY(v,errMsg) {if(!v || !v->kind) { fprintf(stderr,"%s:%d - %s",programs[depth]->name,instruction->lineno,errMsg); return; }}

#define OWNER_KEY(O,key,opname) {	\
    own=getValue(); \
    if(!own || (own->kind != eConstString && own->kind != eDynamicString)) { \
      fprintf(stderr,"%s:%d - Bad datatype of OWNER part in %s\n",program->name,instruction->lineno,opname); return; \
    } \
    v=value2string(own,own_buf,sizeof(own_buf)); \
    if(sscanf(v,"%d",&O) != 1) {\
      fprintf(stderr,"%s:%d - Failed to parse %s owner (was %s)\n",program->name,instruction->lineno,opname,v); \
    } \
    unreference(own); \
    EXPECT(COLON,"Expected ':' in between owner and key operands\n"); \
    i2 = getInstruction(); \
    if(i2->token == L_PARAN) { key = getValue(); EXPECT(R_PARAN,"Expected ')' after complex tuple key argument"); } \
    else key = atom2value(i2->token); \
    if(!key || !key->kind) { fprintf(stderr,"%s:%d - Bad value for key in %s\n",program->name,instruction->lineno,opname); return; } \
  }

  opDepth=0;
  valueDepth=0;
  
  int argNum,operand;

  while(1) {

    value=NULL;
    value2=NULL;
    value3=NULL;
    key=NULL;

    Program *program = programs[frameDepth];
    if(controlDepth == MAX_CONTROL_FLOW) {
      fprintf(stderr,"%s:%d Error - too many nested controlflow statements\n",program->name,instruction->lineno);
      return;
    }
    pcStart=pc[frameDepth]; 
    argNum=-1;
    operand=-1;
    if(opDepth > 0) { 
      argNum=valueDepth - opStack[opDepth-1].valueStackStart;    
      operand=opStack[opDepth-1].operand;
    }
    if(opDepth > 0 && opArgs[operand][argNum+1] == TUPLE_ARG) {
      /* Operands that require tuple name arguments (id:key) are treated by the special tuple_cons operand */
      opStack[opDepth].operand=_TCONS;
      opStack[opDepth].valueStackStart=valueDepth;
      opStack[opDepth].lineno = instruction->lineno;
      opDepth++;
    }

    instruction = getInstruction();
    if(!instruction) return;
    if(doQuit || !peiskmt_isRunning()) return;
    if(instruction->token == TOKEN_EOF) {
      //printf("Closing %s\n",program->name);
      if(sources[frameDepth].kind == eFile) { fclose(sources[frameDepth].fp); }
      free(sources[frameDepth].name);
      unreference(programsVal[frameDepth]);
      if(--frameDepth < 0) return;
      continue;
    }
    if(debug) { for(o=0;o<frameDepth;o++) printf("  "); printf("%s:%d '%s' (%d)\n",program->name,instruction->lineno,token2string(instruction->token),instruction->token); }
    if(opDepth > 0 && argNum<MAX_OP_ARGS-1 && opArgs[operand][argNum+1] == ATOM_ARG) {
      valueStack[valueDepth++]=atom2value(instruction->token);
    } else if(opDepth > 0 && argNum<MAX_OP_ARGS-1 && opArgs[operand][argNum+1] > 0) {
      if(instruction->token != opArgs[operand][argNum+1]) {
	fprintf(stderr,"%s:%d/%d - Syntax error. Expected '%s', not '%s' here for op %s\n",program->name,opStack[opDepth-1].lineno,instruction->lineno,
		token2string(opArgs[operand][argNum+1]),token2string(instruction->token),token2string(operand));
	return;
      } else valueStack[valueDepth++]=atom2value(instruction->token);
    } else if(isReserved(instruction->token)) {
      /* Push a new operand onto operand stack */      
      operand=instruction->token;
      if(opArgs[operand][0] < 0) {
	fprintf(stderr,"%s:%d - Syntax error. Illegal operand '%s'\n",program->name,instruction->lineno,token2string(instruction->token));
	return;
      }
      //printf("Pushing operand %s, stackstart: %d\n",token2string(operand),valueDepth);
      opStack[opDepth].operand = operand;
      opStack[opDepth].valueStackStart=valueDepth;
      opStack[opDepth].lineno = instruction->lineno;
      opDepth++;
    } else {
      char *tok = token2string(instruction->token);
      Value *val;
      if(isdigit(tok[0]) || tok[0]=='-' || tok[0]=='.') {
	float f; char *c;
	for(c=tok;*c&&*c!='.';c++) {}
	if(*c && sscanf(tok,"%f",&f) == 1) val=double2value(f);
	else if(!*c && sscanf(tok,"%d",&i) == 1) val=int2value(i);
	else val=cstring2value(tok);
      } else if(tok[0] == '\"')
	val=cstring2value(tok+1);
      else {
	char isFun=0;
	val=lookupFunVariable(environment,instruction->token,&isFun);
	if(val) {
	  reference(val);
	  if(isFun) {
	    opStack[opDepth].operand = CALL;
	    opStack[opDepth].valueStackStart=valueDepth;
	    opStack[opDepth].lineno=instruction->lineno;
	    opDepth++;
	  }
	} else {
	  fprintf(stderr,"%s:%d - No such variable '%s' defined\n",program->name,instruction->lineno,token2string(instruction->token));
	  return;
	}
      }
      //printf("Pushing: "); fprintfValue(stdout,val); printf("\n");
      valueStack[valueDepth++]=val;
    }
    
    /* Next, execute all operands that can be executed given these inputs */
    while(1) {
      if(opDepth <= 0) break;
      if(opDepth <= control[controlDepth-1].opStart) break; // Prevent execution across blocks
      valueStart=opStack[opDepth-1].valueStackStart;
      argNum=valueDepth - valueStart;
      operand=opStack[opDepth-1].operand;
      if(argNum < opArgs[operand][0]) break;
      //printf("Executing %s\n",token2string(operand));
      switch(operand) {
      case COMMA: break;
      case QUOTE: break;
      case NIL: valueStack[valueDepth++]=NULL; break;

	#include "ops_io.c"
	#include "ops_arithmethic.c"
	#include "ops_containers.c"
	#include "ops_tuples.c"
	#include "ops_control.c"

      case SLEEP:
	value=valueStack[valueStart];
	d=value2double(value);
	unreference(value);
	valueDepth=valueStart;
	if(d>0.0) usleep((long)(d*1000000.0));
	break;
      case DUMP_HEAP:
	dumpHeap();
	break;
      }
      opDepth--;
    }
  cont_running:
    /* Dump any remaining (return) values to stdout */
    while(opDepth == 0 && valueDepth > 0) {
      value=valueStack[--valueDepth];
      fprintfValue(stdout,value); printf("\n");
      unreference(value);      
    }
  }
}
