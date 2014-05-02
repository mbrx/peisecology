/*
** tokenizer.c
**
** Functions for parsing input
** 
** Made by (Mathias Broxvall)
** Login   <mbl@brox>
** 
** Started on  Wed Mar 12 18:31:01 2008 Mathias Broxvall
** Last update Sun May 12 01:17:25 2002 Speed Blue
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include <stdio.h>
#include "support.h"
#include "callbacks.h"
#include "peiskernel/peiskernel.h"
#include "peiskernel/tuples.h"
#include <ctype.h>
#include <string.h>

#include "tupleview.h"
#include "tokenizer.h"

int getToken(FILE *fp,char *token,int buflen) {
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
  if(c == '=' || c == ',' || c == '{' || c == '}' || c == '[' || c == ']' || c == '+' || c == '(' || c == ')') return 0;

  if(isdigit(c)) isnum=1;
  else isnum=0;

  /* Handle strings in a special way */
  /* "strings" -> "strings" while 'strings' -> strings */
  if(c == '"' || c == '\'') {
    if(c == '\'') pos--;
    while(1) {
      if(feof(fp)) return 1; /* End of file inside string, not ok */
      c=getc(fp);
      if(c == '\n') lineno++;
      if(c == '"') { token[pos++]=c; token[pos]=0; return 0; }
      if(c == '\'') { token[pos]=0; return 0; }
      if(c == '\\') {
	if(feof(fp)) return 1; /* End of file inside string, not ok */
	c=getc(fp);
	if(c == '\n') lineno++;
      }
      token[pos++]=c;       
    } 
  }

  while(1) {
    if(feof(fp)) return 0; /* End of file during token, it's ok */
    c=getc(fp);
    if(c == '\n') lineno++;
    if(isspace(c)) return 0; /* Token finished */
    if(c == '=' || c == ',' || c == '{' || c == '}' || c == '[' || c == ']' || c == '"' || c == '\'' || c == '+' || c == '(' || c == ')') {
      ungetc(c,fp);
      if(c == '\n') lineno--;
      return 0;
    }
    token[pos++]=c; token[pos]=0;
    if(pos >= buflen) return 1; /* Error, buffer not long enough to read token */    
  }
}


int sgetToken(char **str,char *token,int buflen) {
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
  if(c == '=' || c == ',' || c == '{' || c == '}' || c == '[' || c == ']' || c == '+' || c == '(' || c == ')') return 0;

  if(isdigit(c)) isnum=1;
  else isnum=0;

  /* Handle strings in a special way */
  /* "strings" -> "strings" while 'strings' -> strings */
  if(c == '"' || c == '\'') {
    if(c == '\'') pos--;
    while(1) {
      if(!**str) return 1; /* End of file inside string, not ok */
      c=*((*str)++);

      if(c == '\n') lineno++;
      if(c == '"') { token[pos++]=c; token[pos]=0; return 0; }
      if(c == '\'') { token[pos]=0; return 0; }
      if(c == '\\') {
	if(!**str) return 1; /* End of file inside string, not ok */
	c=*((*str)++);
	if(c == '\n') lineno++;
      }
      token[pos++]=c;       
    } 
  }

  while(1) {
    if(!**str) return 0; /* End of file during token, it's ok */
    c=*((*str)++);
    if(c == '\n') lineno++;
    if(isspace(c)) return 0; /* Token finished */
    if(c == '=' || c == ',' || c == '{' || c == '}' || c == '[' || c == ']' || c == '"' || c == '\'' || c == '+' || c == '(' || c == ')') {
      (*str)--;
      if(c == '\n') lineno--;
      return 0;
    }
    token[pos++]=c; token[pos]=0;
    if(pos >= buflen) return 1; /* Error, buffer not long enough to read token */    
  }
}
