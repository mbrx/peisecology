/** \file tokenizer.c
    \brief A simple example file for tokenizing inputs from files and/or strings. 
    Usefull for eg. parsing ASCII tuples
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include "peiskernel/peiskernel_mt.h"
#include "peiskernel/hashtable.h"
#include "tokenizer.h"

#ifndef WITH_READLINE
/* The below macro utilizes gcc's extension 'statement expression' */
#define readline(prompt) ({char str[256]; printf("%s",prompt); fgets(str,sizeof(str),stdin)?strdup(str):NULL;})
#define add_history(something) {}
#else
#include <readline/readline.h>
#include <readline/history.h>
#endif

/** \brief Maximum number of tokens that can fit into the tokenizer pointer buffer */
intptr_t maxTokens;
/** \brief Current number of tokens in the tokenizer pointer buffer */
intptr_t nTokens;
/** \brief Constant time lookup table from token ID to the actual token strings */
char **tokenPointers;
/** \brief Hashtable (~ O(log n) time) lookup of strings to token IDs */
PeisHashTable *tokenizer_ht;

/** \brief Gets the next token from stdin using the readline library */
int readlineToken0(char *token,int buflen);
/** \brief Reads another token from the given file. \param fp The input file \return Returns zero on success */
int getToken0(FILE *fp,char *token,int buflen);
/** \brief Reads another token from and updates the given string pointer. \param str Pointer to a pointer to a string containing the inputs \return Returns zero on success */
int sgetToken0(char **str,char *token,int buflen);

/* see tokenizer.h */
int lineno;

void initTokenizer() {
  lineno=0;
  nTokens=0;
  maxTokens=1024;
  tokenPointers=malloc(sizeof(char*)*maxTokens);
  tokenizer_ht  = peisk_hashTable_create(PeisHashTableKey_String);
  insertToken("<!invalid token!>");
}

int insertToken(char *str) {
  void *val;
  if(peisk_hashTable_getValue(tokenizer_ht,(void*)str,&val) == 0) 
    return (intptr_t) val;
  if(nTokens == maxTokens) {
    printf("note: Tokenizer resizing token buffer\n");
    maxTokens *= 2;
    tokenPointers=realloc(tokenPointers,sizeof(char*)*maxTokens);
  }
  tokenPointers[nTokens]=strdup(str);
  peisk_hashTable_insert(tokenizer_ht,str,(void*)nTokens);
  //printf("Inserted token %d = '%s'\n",nTokens,str);
  return nTokens++;
}

char *token2string(int token) {
  if(token >= 0 && token < nTokens) return tokenPointers[token];
  else return "<!invalid token id!>";
}

int isSingularToken(char c) {
  return c == '=' || c == ',' || c == '{' || c == '}' || c == '[' || c == ']' || c == '+' || c == '(' || c == ')' || 
    c == '$' || c == '@' || c == ':' || c == '%' || c == '\'';
}

int getToken0(FILE *fp,char *token,int buflen) {
  char c;
  int pos=0;
  int isnum;

  /* Skip all whitespace */
  while(1) {
    if(feof(fp)) return 1;   /* Error, end of file before next token */
    c=getc(fp);
    if(c == '\n') lineno++;
    if(c == '#') {
      /* Begining of comment, skip to end of line */
      while(1) { if(feof(fp)) return 1; c=getc(fp);  if(c == '\n') lineno++; if(c == '\n') break; }
      continue;
    }
    if(!isspace(c)) break;
  }
  if(c == EOF) return 1;
  /* c is now first non whitespace character */
  token[pos++]=c; token[pos]=0;
  if(pos >= buflen) return 1; /* Error, buffer not long enough to read token */
  
  /* Some characters are whole tokens by themselves. */
  if(isSingularToken(c)) return 0;

  if(isdigit(c)) isnum=1;
  else isnum=0;

  /* Handle strings in a special way */
  /* "strings" -> "strings  (ie. we do not keep the last '"' in it)
   */
  if(c == '"') {
    while(1) {
      if(feof(fp)) return 1; /* End of file inside string, not ok */
      c=getc(fp);
      if(c == '\n') lineno++;
      else if(c == '"') { /*token[pos++]=c;*/ token[pos]=0; return 0; }
      else if(c == '\\') {
	if(feof(fp)) return 1; /* End of file inside string, not ok */
	c=getc(fp);
	if(c == '\n') { lineno++; continue; }
	else if(c == 'n') c = '\n';
	else if(c == 'r') c = '\r';
	else if(c == 't') c = '\t';
      }
      token[pos++]=c;       
    } 
  }

  while(1) {
    if(feof(fp)) return 0; /* End of file during token, it's ok */
    c=getc(fp);
    if(c == '\n') lineno++;
    if(isspace(c)) return 0; /* Token finished */
    if(isSingularToken(c)) {
      ungetc(c,fp);
      if(c == '\n') lineno--;
      return 0;
    }
    token[pos++]=c; token[pos]=0;
    if(pos >= buflen) return 1; /* Error, buffer not long enough to read token */    
  }
}


int sgetToken0(char **str,char *token,int buflen) {
  char c;
  int pos=0;
  int isnum;

  /* Skip all whitespace */
  while(1) {
    if(!**str) return 1;   /* Error, end of file before next token */
    c=*((*str)++);
    if(c == '\n') lineno++;
    if(c == '#') {
      /* Begining of comment, skip to end of line */
      while(1) { 
	if(!**str) return 1; 
	c=*((*str)++);
	if(c == '\n') lineno++; if(c == '\n') break; }
      continue;
    }
    if(!isspace(c)) break;
  }
  if(!c) return 1;
  /* c is now first non whitespace character */
  token[pos++]=c; token[pos]=0;
  if(pos >= buflen) return 1; /* Error, buffer not long enough to read token */
  
  /* Some characters are whole tokens by themselves. */
  if(isSingularToken(c)) return 0;

  if(isdigit(c)) isnum=1;
  else isnum=0;

  /* Handle strings in a special way */
  /* "strings" -> "strings  (ie. we do not keep the last '"' in it)
   */
  if(c == '"') {
    while(1) {
      if(!**str) return 1; /* End of file inside string, not ok */
      c=*((*str)++);

      if(c == '\n') lineno++;
      else if(c == '"') { /*token[pos++]=c;*/ token[pos]=0; return 0; }
      //if(c == '\'') { token[pos]=0; return 0; }
      if(c == '\\') {
	if(!**str) return 1; /* End of file inside string, not ok */
	c=*((*str)++);
	if(c == '\n') { lineno++; continue; }
	else if(c == 'n') c = '\n';
	else if(c == 'r') c = '\r';
	else if(c == 't') c = '\t';
      }
      token[pos++]=c;       
    } 
  }

  while(1) {
    if(!**str) return 0; /* End of file during token, it's ok */
    c=*((*str)++);
    if(c == '\n') lineno++;
    if(isnum && c != '.' && !isdigit(c)) return 0; /* Token finished */
    if(isspace(c)) return 0; /* Token finished */
    if(isSingularToken(c)) {
      (*str)--;
      if(c == '\n') lineno--;
      return 0;
    }
    token[pos++]=c; token[pos]=0;
    if(pos >= buflen) return 1; /* Error, buffer not long enough to read token */    
  }
}

/** Stores the remaining inputs from the last call to readline */
char *readlineLast=NULL;
char readlinePrompt[256], *readlinePromptType;
char *readlineRest;

int readlineToken0(char *token,int buflen) {
  char c;
  int pos=0;
  int isnum;

  if(!readlineLast) { 
    lineno++;						       
    sprintf(readlinePrompt,"%d%s",lineno,readlinePromptType);
    readlineLast=readline(readlinePrompt); 
    if(!readlineLast) return 1;
    if(readlineLast[0] && readlineLast[0] != '\n') add_history(readlineLast);    
    readlineRest=readlineLast;
  }

#define HANDLE_EMPTY() if(!*readlineRest) {			\
    lineno++;							\
    if(!peiskmt_isRunning()) return 1;				\
    free(readlineLast);						\
    sprintf(readlinePrompt,"%d%s",lineno,readlinePromptType);	\
    readlineLast=readline(readlinePrompt);			\
    if(!readlineLast) return 1;					\
    if(readlineLast[0] && readlineLast[0] != '\n') add_history(readlineLast); \
    readlineRest=readlineLast;						\
  }

  /* Skip all whitespace */
  while(1) {
    HANDLE_EMPTY();
    //printf("Skipping ws - rest: %s\n",readlineRest);

    c=*(readlineRest++);
    if(c == '\n') lineno++;
    if(c == '#') {
      /* Begining of comment, skip to end of line */
      while(1) { 
	HANDLE_EMPTY();
	c=*(readlineRest++);
	if(c == '\n') lineno++; if(c == '\n') break; }
      continue;
    }
    if(!isspace(c)) break;
  }
  if(!c) return 1;
  /* c is now first non whitespace character */
  token[pos++]=c; token[pos]=0;
  if(pos >= buflen) return 1; /* Error, buffer not long enough to read token */
  
  /* Some characters are whole tokens by themselves. */
  if(isSingularToken(c)) return 0;

  if(isdigit(c) || c == '-') isnum=1;
  else isnum=0;

  /* Handle strings in a special way */
  /* "strings" -> "strings  (ie. we do not keep the last '"' in it)
   */
  if(c == '"') {
    while(1) {
      HANDLE_EMPTY();
      //printf("Inside string - rest: %s\n",readlineRest);

      c=*((readlineRest)++);
      if(c == '\n') lineno++;
      else if(c == '"') { /*token[pos++]=c;*/ token[pos]=0; return 0; }
      else if(c == '\'') { token[pos]=0; return 0; }
      else if(c == '\\') {
	HANDLE_EMPTY();
	c=*((readlineRest)++);
	if(c == '\n') { lineno++; continue; }
	else if(c == 'n') c = '\n';
	else if(c == 'r') c = '\r';
	else if(c == 't') c = '\t';
      }
      token[pos++]=c;       
    } 
  }

  if(!*readlineRest) return 0; /* Token finished since we require a new line after this */
  while(1) {
    HANDLE_EMPTY();
    //printf("Inside token - rest: %s\n",readlineRest);

    c=*((readlineRest)++);
    if(c == '\n') lineno++;
    if(isspace(c)) return 0; /* Token finished */
    if(isSingularToken(c)) {
      (readlineRest)--;
      if(c == '\n') lineno--;
      return 0;
    }
    token[pos++]=c; token[pos]=0;
    if(pos >= buflen) return 1; /* Error, buffer not long enough to read token */
    if(!*readlineRest) return 0; /* Token finished since we require a new line after this */
  }
}



int getToken(FILE *fp,int *inp_lineno) {
  char buf[1024];
  lineno=*inp_lineno;
  if(getToken0(fp,buf,sizeof(buf)) == 0) {
    *inp_lineno=lineno;
    return insertToken(buf);
  }
  *inp_lineno=lineno;
  if(feof(fp)) return TOKEN_EOF;
  return TOKEN_ERROR;
}
int sgetToken(char **inp,int *inp_lineno) {
  char buf[1024];
  lineno=*inp_lineno;
  if(sgetToken0(inp,buf,sizeof(buf)) == 0) {
    *inp_lineno=lineno;
    return insertToken(buf);
  }
  *inp_lineno=lineno;
  if(**inp == 0) return TOKEN_EOF;
  return TOKEN_ERROR;
}
int readlineToken(int *inp_lineno) {
  char buf[1024];
  lineno=*inp_lineno;
  if(readlineToken0(buf,sizeof(buf)) == 0) {
    *inp_lineno=lineno;
    //printf("readline -> %s\n",buf);
    return insertToken(buf);
  }
  *inp_lineno=lineno;
  return TOKEN_EOF;
}

void flushReadline() {
  if(readlineRest) {
    free(readlineRest);
    readlineRest=NULL;
  }
}
