/*
** runlisp.c
** 
** Made by (Mathias Broxvall)
** Login   <mbl@brox>
** 
** Started on  Mon Nov 27 15:02:40 2006 Mathias Broxvall
*/

#include <stdio.h>
#include <unistd.h>

int main(short argc,char **args) {
  char *arglist[512];
  int nargs=0;
  int i;
  char lispPath[512];
  char libPath[512];
  char prefix[512];

  /*#exec /usr/bin/lisp -quiet -batch -noinit -load library:start "$@"*/
  snprintf(lispPath,511,"%s/bin/lisp",LISP_PREFIX);
  snprintf(libPath,511,"%s/lib/cmucl/start",LISP_PREFIX);

  /*
  printf("lispPath=%s\n",lispPath);
  printf("libath=%s\n",libPath);
  */

  arglist[nargs++] = lispPath;
  /*arglist[nargs++] = "-quiet";*/
  arglist[nargs++] = "-batch";
  arglist[nargs++] = "-noinit";
  arglist[nargs++] = "-load";
  arglist[nargs++] = libPath; 
  for(i=1;i<argc && nargs<511;i++)
    arglist[nargs++] = args[i];
  arglist[nargs++] = NULL;
  
  return execv(arglist[0],arglist);

}
