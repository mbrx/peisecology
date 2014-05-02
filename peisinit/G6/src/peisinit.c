/** \file peisinit.c
    Component responsible for launching and monitoring other components on the same cpu. Similar
    to a peisized "inittab". In future should also handle ontologies etc.
*/
/*
   Copyright (C) 2005 - 2012     Mathias Broxvall

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

/** \mainpage peisinit Peisinit

    Component responsible for launching and monitoring other components on the same computer. Similar
    to a peisized "inittab". Also serves as a container for all peis specific tuples.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <semaphore.h>
#include <sys/stat.h>
#ifdef __CYGWIN__
#include <fcntl.h>
#endif
#ifndef __CYGWIN__
#ifndef __MACH__
#include <asm/fcntl.h>
#endif
#endif
#include <unistd.h>


#include <peiskernel/peiskernel.h>
#include <peiskernel/peiskernel_private.h>
#include <peiskernel/tuples.h>



typedef void *(*PosixThreadFn)(void*);

/*                  */
/* Global variables */
/*                  */

/** Structure describing the possible different components aka. "commands" */
typedef struct InitCommand {
  char *name;
  char *execString;
  char *semantics;
  char *stdinFile, *stdoutFile, *stderrFile;
  char *extraArgs;
  char *proxyDesc;
  int currState, reqState, initReqState;
  pid_t pid;
  int peisid;
  float spawnDelay;
  float lastFailure;
  pthread_t execThread;
  pthread_t spawnThread;
  sem_t spawnSem, pipeSem;
  int stdinPipe[2], stdoutPipe[2], stderrPipe[2];
} InitCommand;

int isRunning;
/** This is the directory containing all init commands */
char *initdir="/usr/local/share/peisinit";

/** Found/usable initcommands */
InitCommand commands[100];
/** Number of found/usable initcommands */
int nInitCommands;

/* The possible states a PEIS can be in */
#define PEIS_STATE_OFF       0
#define PEIS_STATE_ON        1
#define PEIS_STATE_ABORTED   2
#define PEIS_STATE_RESTART   3
#define PEIS_STATE_LAST      4

/* String names corresponding to states */
char *peisStateNames[PEIS_STATE_LAST]={"off","on","abort","start"};

/** The current line number when parsing command files */
int lineno;
/** Name of current file when parsing command files */
char *filename;
/** Pointer to a global logfile onto which all interesting events are logged */
FILE *globalLogfile;

/*                  */
/*    Prototypes    */
/*                  */

/** Looks for all *.cmp files in the initdir and parses them */
void parseInitDir();

/** Attempts to read another token from file, stores result in
    given string buffer. Returns zero if succcessfull, nonzero otherwise. */
int getToken(FILE*,char*,int buflen);

/** Equivalent to a constructor for the InitCommand structure. Resets all fields to good default values */
void clearInitCommand(InitCommand*);

/** Publishes the state tuples for the given initcommand (component) */
void updateCommandTuple(InitCommand*);

/** Trigger for changes to reqState tuples of each command */
void tupleCallback_reqState(PeisTuple *tuple,InitCommand *command);

/** Launches the given component and performs monitoring of it. Should be run in a separate thread */
void execFn(InitCommand *command);

/** Spawns the process for the given command. Should be run in yet another thread, which forks and executes the given command. Terminates when/if the given command terminates. */
void spawnFn(InitCommand *command);

/*                 */
/*      Code       */
/*                 */

void printUsage(FILE *stream,short argc,char **args) {
  fprintf(stream,"%s options:\n",args[0]);
  fprintf(stream," --help                 Print usage information\n");
  fprintf(stream," --initdir              The directory containing all available commands\n");  
  fprintf(stream," --daemon               Run in background\n");  
  peisk_printUsage(stream,argc,args);
  exit(0);
}

int main(int argc,char **args) {
  int i, asDaemon=0, lfp;
  char name[512];
  char value[512];

  if(argc > 1 && strcmp(args[1],"--help") == 0) printUsage(stderr,argc,args);
  for(i=1;i<argc;i++) {
    if(strcmp(args[i],"--initdir") == 0 && i+1 < argc) {
      initdir=args[i+1];
    }
    if(strcmp(args[i],"--daemon") == 0) {      
      asDaemon=1;
    }
  }

  if(asDaemon) {
    printf("Running as background DAEMON\n");
    /* We have been requested to run in background, fork ourselves to
       become a daemon */ 
    i=fork();
    if (i<0) exit(1); /* fork error */
    if (i>0) exit(0); /* parent exits */
    /* Place ourselves in a new session group to become independent of
       parent signals and TTY's */
    setsid();
  }

  /* Set our umask to 000, so all newly created logfiles appears as
     world read/writable so all users can start PEIS init as needed. Good
     for develoment - lousy for security! */
  umask(000);   

  /* Create a lock file to make sure no other instance of PEIS init
     is running. */
  sprintf(name,"%s/peisinit.lock",initdir);
  lfp=open(name,O_RDWR|O_CREAT,0777);
  if (lfp<0) {
    printf("Cannot open lock file %s\n",name);
    exit(1);
  }
  if (lockf(lfp,F_TLOCK,0)<0) {
    printf("Another peisinit process is running according to %s\n",name);
    exit(0); /* can not lock */
  }
  /* only first instance continues */
  sprintf(name,"%d\n",getpid());
  write(lfp,name,strlen(name)); /* record pid to lockfile */

  if(asDaemon) {
    /* Close all old file descriptors - except for the lock file */
    for (i=getdtablesize();i>=0;--i) 
      if(i != lfp) close(i); 

    /* Use /dev/null for all I/O operations */
    i=open("/dev/null",O_RDWR); /* open stdin */
    dup(i); /* stdout */
    dup(i); /* stderr */
    /* Always run relative to same directory */
    chdir(initdir);

  }


  /* Initialize peiskernel */
  peisk_initialize(&argc,args);
  peisk_setStringTuple("name","peisinit");
  snprintf(value,sizeof(value),"v%s",VERSION);
  peisk_setStringTuple("version",value);
  peisk_setStringTuple("do-quit","no");
  printf("Running PEISINIT with initdir='%s'\n",initdir);

  peisk_setStringTuple("status","init");
  
  /* Open global log file */
  snprintf(name,sizeof(name),"%s/global.log",initdir);
  globalLogfile=fopen(name,"a");
  if(!globalLogfile) { fprintf(stderr,"Failed to open %s for appending\n",name); exit(0); }
  fprintf(globalLogfile,"%8.3f S: Start INIT\n",peisk_gettimef()); fflush(globalLogfile);

  parseInitDir();
  printf("Found %d initcommands\n",nInitCommands);  

  /* Register callbacks for all tuples */
  for(i=0;i<nInitCommands;i++) {
    sprintf(name,"components.%s.reqState",commands[i].name);
    peisk_registerTupleCallback(peisk_peisid(),name,&commands[i],(PeisTupleCallback*) tupleCallback_reqState);
  }

  /* Create initial tuple information */
  int len=4096;
  char buff[4096];
  char *str;
  sprintf(buff,"( ");
  str = buff+strlen(buff);
  len -= strlen(buff);

  for(i=0;i<nInitCommands;i++) {    
    snprintf(str,len,"%s ",commands[i].name);
    len -= strlen(str);
    str += strlen(str);
    if(len <= 3) { fprintf(stderr,"Error - too many components registered, doesn't fit into buffer\n"); exit(0); }

    updateCommandTuple(&commands[i]);
    sprintf(name,"components.%s.reqState",commands[i].name);
    peisk_setStringTuple(name,peisStateNames[commands[i].initReqState]);

    sprintf(name,"components.%s.extra-arguments",commands[i].name);
    peisk_setStringTuple(name,commands[i].extraArgs);
  }
  snprintf(str,len,")");
  peisk_setStringTuple("components",buff);

  isRunning=1;
  peisk_setStringTuple("status","running");
  while(isRunning && peisk_isRunning()) {
    peisk_wait(100000);
    PeisTuple *tuple = peisk_getTuple(peisk_peisid(),"do-quit",PEISK_FILTER_OLD|PEISK_NON_BLOCKING);
    if(tuple && strncmp(tuple->data,"no",tuple->datalen) != 0) {
      printf("Soft exit: %s\n",tuple->data);
      break;
    }
  }
  peisk_setStringTuple("status","stopped");

  /* Stop all running commands */
  for(i=0;i<nInitCommands;i++) {
    char str[256];
    snprintf(str,sizeof(str),"components.%s.reqState",commands[i].name);
    peisk_setStringTuple(str,"off");
    //commands[i].reqState == PEIS_STATE_OFF;
  }
  sleep(5);
  isRunning=0;
  peisk_shutdown();
}

void parseInitDir() {
  int i;
  DIR *dir;
  struct dirent *dirent;
  FILE *fp;
  char path[256];
  char token[256];
  char str[256];
  InitCommand *command;

  /* Open the directory containing a listing of all components */
  /* TODO: Allow the usage of multiple init directories which are
     searched in a specific order. */     
  dir=opendir(initdir);
  if(!dir) {
    printf("Failed to open directory '%s'\n",initdir);
    exit(-1);
  }
  while(dirent=readdir(dir)) {
    if(strlen(dirent->d_name) > 4 && 
       strcmp(&dirent->d_name[strlen(dirent->d_name)-4],".cmp") == 0) {
      if(!snprintf(path,sizeof(path),"%s/%s",initdir,dirent->d_name)) continue;
      printf("Loading: '%s'\n",path);
      fp=fopen(path,"rb");
      lineno=1;
      filename=dirent->d_name;
      if(!fp) { printf("failed to open\n"); continue; }
      command = &commands[nInitCommands];
      clearInitCommand(command);
      while(!feof(fp) && !getToken(fp,token,sizeof(token))) {
	if(strcasecmp(token,"Name") == 0) {
	  if(getToken(fp,token,sizeof(token)) || strcmp(token,"=") != 0) { 
	    fprintf(stderr,"%s:%d: Expected '='\n",filename,lineno); break; 
	  }
	  if(getToken(fp,token,sizeof(token))) break;
	  command->name=strdup(token);
	}
	else if(strcasecmp(token,"Exec") == 0) {
	  if(getToken(fp,token,sizeof(token)) || strcmp(token,"=") != 0) { 
	    fprintf(stderr,"%s:%d: Expected '='\n",filename,lineno); break; 
	  }
	  if(getToken(fp,token,sizeof(token))) break;
	  command->execString=strdup(token);
	}
	else if(strcasecmp(token,"InitState") == 0) {
	  if(getToken(fp,token,sizeof(token)) || strcmp(token,"=") != 0) { 
	    fprintf(stderr,"%s:%d: Expected '='\n",filename,lineno); break; 
	  }
	  if(getToken(fp,token,sizeof(token))) break;
	  for(i=0;i<PEIS_STATE_LAST;i++) if(strcasecmp(token,peisStateNames[i]) == 0) break;
	  if(i == PEIS_STATE_LAST) { fprintf(stderr,"%s:%d: Bad name '%s' for state\n",filename,lineno,token); break; }
	  command->initReqState=i;
	}
	else if(strcasecmp(token,"id") == 0) {
	  if(getToken(fp,token,sizeof(token)) || strcmp(token,"=") != 0) { 
	    fprintf(stderr,"%s:%d: Expected '='\n",filename,lineno); break; 
	  }
	  if(getToken(fp,token,sizeof(token))) break;
	  if(strcmp(token,"+") == 0) {
	    if(getToken(fp,token,sizeof(token))) break;
	    if(sscanf(token,"%d",&command->peisid) != 1) { 
	      fprintf(stderr,"%s:%d: Expected ID number\n",filename,lineno); break; 
	    }
	    command->peisid += peisk_peisid();
	  } else {
	    if(sscanf(token,"%d",&command->peisid) != 1) { 
	      fprintf(stderr,"%s:%d: Expected ID number\n",filename,lineno); break; 
	    }
	  }
	}
	else if(strcasecmp(token,"spawndelay") == 0) {
	  if(getToken(fp,token,sizeof(token)) || strcmp(token,"=") != 0) { 
	    fprintf(stderr,"%s:%d: Expected '='\n",filename,lineno); break; 
	  }
	  if(getToken(fp,token,sizeof(token))) break;
	  if(sscanf(token,"%f",&command->spawnDelay) != 1) {
	    if(sscanf(token,"%d",&i) == 1) command->spawnDelay=i;
	    else { fprintf(stderr,"%s:%d: Expected spawn delay as floating point or integer\n",filename,lineno); break; }
	  }
	}
	else if(strcasecmp(token,"semantics") == 0) {
	  /* This is a semantic description for this
	     component. Sometimes these can be quite lengthy so this is why
	     the token buffer needs to be fairly long. */
	  if(getToken(fp,token,sizeof(token)) || strcmp(token,"=") != 0) { 
	    fprintf(stderr,"%s:%d: Expected '='\n",filename,lineno); break; 
	  }
	  if(getToken(fp,token,sizeof(token))) break;
	  command->semantics = strdup(token);
	} else if(strcasecmp(token,"proxy") == 0) {
	  /* This is a generic proxy description for this component.  */
	  if(getToken(fp,token,sizeof(token)) || strcmp(token,"=") != 0) { 
	    fprintf(stderr,"%s:%d: Expected '='\n",filename,lineno); break; 
	  }
	  if(getToken(fp,token,sizeof(token))) break;
	  command->proxyDesc = strdup(token);
	} else if(strcasecmp(token,"ExtraArguments") == 0) {
	  /* This is extra arguments that get passed into the
	     $PEISINIT variable. Should also be synced with the tuple extra-arguments */
	  if(getToken(fp,token,sizeof(token)) || strcmp(token,"=") != 0) { 
	    fprintf(stderr,"%s:%d: Expected '='\n",filename,lineno); break; 
	  }
	  if(getToken(fp,token,sizeof(token))) break;
	  command->extraArgs = strdup(token);	  
	} else { fprintf(stderr,"%s:%d: Unknown argument '%s'\n",filename,lineno,token); break; }
      }
      /* hm - remember to deallocate these strings sometime ... */
      strcpy(token,path);
      token[strlen(token)-4]=0;
      snprintf(str,sizeof(str),"%s.stdin",token);
      command->stdinFile=strdup(str);
      snprintf(str,sizeof(str),"%s.stdout",token);
      command->stdoutFile=strdup(str);
      snprintf(str,sizeof(str),"%s.stderr",token);
      command->stderrFile=strdup(str);
      nInitCommands++;
      fclose(fp);
    }
  }
  closedir(dir);
}

void clearInitCommand(InitCommand *command) {
  command->name=NULL;
  command->execString=NULL;
  command->pid=0;
  command->peisid=0;
  command->initReqState=PEIS_STATE_OFF;
  command->currState=PEIS_STATE_OFF;
  command->reqState=PEIS_STATE_OFF;
  command->spawnDelay=3.0;
  sem_init(&command->spawnSem,0,0);
  sem_init(&command->pipeSem,0,0);
  command->stdinFile = NULL;
  command->stdoutFile = NULL;
  command->stderrFile = NULL;
  command->extraArgs = "";
  command->proxyDesc = "";
  command->semantics = "";
}
void updateCommandTuple(InitCommand *command) {
  char name[256];
  char value[256];

  sprintf(name,"components.%s.id",command->name);
  sprintf(value,"%d",command->peisid);
  peisk_setStringTuple(name,value);  

  sprintf(name,"components.%s.pid",command->name);
  sprintf(value,"%d",command->pid);
  peisk_setStringTuple(name,value);

  sprintf(name,"components.%s.exec",command->name);
  sprintf(value,"%s",command->execString);
  peisk_setStringTuple(name,value);

  sprintf(name,"components.%s.currState",command->name);
  sprintf(value,"%s",peisStateNames[command->currState]);
  peisk_setStringTuple(name,value);  

  sprintf(name,"components.%s.semantics",command->name);
  peisk_setStringTuple(name,command->semantics);  

  sprintf(name,"components.%s.proxy",command->name);
  peisk_setStringTuple(name,command->proxyDesc);  

}


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
  if(c == '=' || c == ',' || c == '{' || c == '}' || c == '[' || c == ']' || c == '+') return 0;

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
    if(c == '=' || c == ',' || c == '{' || c == '}' || c == '[' || c == ']' || c == '"' || c == '\'' || c == '+') {
      ungetc(c,fp);
      if(c == '\n') lineno--;
      return 0;
    }
    token[pos++]=c; token[pos]=0;
    if(pos >= buflen) return 1; /* Error, buffer not long enough to read token */    
  }
}
void tupleCallback_reqState(PeisTuple *tuple,InitCommand *command) {
  int i;
  pthread_attr_t attr;

  printf("Got reqState callback for command %s, data=%s\n",command->name,tuple->data);

  for(i=0;i<PEIS_STATE_LAST;i++) 
    if(strncasecmp(peisStateNames[i],tuple->data,tuple->datalen) == 0) break;
  if(i == PEIS_STATE_LAST) return;
  command->reqState = i;
  if(i == command->currState) return;

  if(command->reqState == 1) {
    pthread_attr_init(&attr);
    pthread_create(&command->execThread,&attr,(PosixThreadFn) execFn, (void*) command);
  }
}

/******** TODO - use named pipes instead for stdin to this child process... */

void spawnFn(InitCommand *command) {
  int retval;
  int status;
  int fd;

  char *initStr="---\n";
  /*printf("SpawnFn for %s started\n",command->name);*/

  /* First, setup std{in,out,err} files */
  /* stdin is reset to zero size */
  fd=open(command->stdinFile,O_WRONLY |O_CREAT,S_IRWXU|S_IRWXG);
  if(!fd) { fprintf(stderr,"Failed to create '%s'\n",command->stdinFile); return; }
  close(fd);
  /* stdout/err is created in append mode and we append the init signature. */
  fd=open(command->stdoutFile,O_WRONLY | O_CREAT | O_APPEND,S_IRWXU|S_IRWXG);  
  if(!fd) { fprintf(stderr,"Failed to create '%s'\n",command->stdinFile); return; }
  write(fd,initStr,strlen(initStr));
  close(fd);
  fd=open(command->stderrFile,O_WRONLY | O_CREAT | O_APPEND,S_IRWXU|S_IRWXG);
  if(!fd) { fprintf(stderr,"Failed to create '%s'\n",command->stdinFile); return; }
  write(fd,initStr,strlen(initStr));
  close(fd);

  /* Setup the stdin/out/err pipes */
  if(pipe(command->stdinPipe)) { perror("spawnFn::Failed to create stdin pipe\n"); exit(0); }
  if(pipe(command->stdoutPipe)) { perror("spawnFn::Failed to create stdout pipe\n"); exit(0); }
  if(pipe(command->stderrPipe)) { perror("spawnFn::Failed to create stderr pipe\n"); exit(0); }

  /* Make our end of the communication pipes non blocking */
  if(fcntl(command->stdinPipe[1], F_SETFL, O_NONBLOCK)) { perror("Failed to set stdinPipe[1] non blocking\n"); exit(-1); }
  if(fcntl(command->stdoutPipe[0], F_SETFL, O_NONBLOCK))  { perror("Failed to set stdoutPipe[0] non blocking\n"); exit(-1); }
  if(fcntl(command->stderrPipe[0], F_SETFL, O_NONBLOCK)) { perror("Failed to set stderrPipe[0] non blocking\n"); exit(-1); }

  /*
  if(fcntl(command->stdoutPipe[0], F_SETFL, O_SYNC) == -1) perror("can't O_SYNC");
  if(fcntl(command->stdoutPipe[1], F_SETFL, O_SYNC) == -1) perror("can't O_SYNC");
  */

  command->pid = fork();
  if(command->pid == 0) {    
    char str[1024];
    int len;

    /* Attempt to read the latest value of the corresponding tuple for
       the arguments. Otherwise we will keep using the last set value
       (eg. from the configuration file). */
    snprintf(str,sizeof(str),"components.%s.extra-arguments",command->name);
    PeisTuple *tuple = peisk_getTuple(peisk_peisid(),str,0);    
    if(tuple) {
      command->extraArgs=tuple->data;
      len=tuple->datalen;
    } else {
      fprintf(stderr,"Error reading tuple %d.%s - this should not occur\n",peisk_peisid(),str);
      exit(0);
    }
    snprintf(str,sizeof(str),"--peis-id %d --peis-hostname %s --peis-network %s --peis-connect tcp://localhost:%d %s",command->peisid,peisk_hostname(),peisk_networkString,peisk_localTCPServerPort(),command->extraArgs);
    errno=0;
    /* Setup the environment */
    setenv("PEISINIT",str,1);

    /*                        */
    /* Redirect stdin/out/err */
    /*                        */

    int fd1, fd2, fd3;

    /* No need to keep the peisinit side of pipes open for child */
    close(command->stdinPipe[1]);
    close(command->stdoutPipe[0]);
    close(command->stderrPipe[0]);

    close(STDIN_FILENO);
    if((fd1=dup(command->stdinPipe[0])) == -1 || fd1 != STDIN_FILENO) { perror("spawnFn: Failed to dup the stdin pipe\n"); exit(0); }
    close(STDOUT_FILENO);
    if((fd2=dup(command->stdoutPipe[1])) == -1 || fd2 != STDOUT_FILENO) { perror("spawnFn: Failed to dup the stdout pipe\n"); exit(0); }
    close(STDERR_FILENO);
    if((fd3=dup(command->stderrPipe[1])) == -1 || fd3 != STDERR_FILENO) { /* we can't give error messages here */  exit(0); }

    /* if(fcntl(STDOUT_FILENO, F_SETFL, O_SYNC) == -1) perror("can't O_SYNC");
    if(fcntl(STDOUT_FILENO, F_SETFL, O_DIRECT) == -1) perror("can't O_SYNC");
    */

#ifdef FORCE_DUP_TO_SPECIFIC_FD
    close(STDIN_FILENO);
    if((fd1=dup2(command->stdinPipe[0],STDIN_FILENO)) == -1) { perror("spawnFn: Failed to dup the stdin pipe\n"); exit(0); }
    close(STDOUT_FILENO);
    if((fd2=dup2(command->stdoutPipe[1],STDOUT_FILENO)) == -1) { perror("spawnFn: Failed to dup the stdout pipe\n"); exit(0); }
    close(STDERR_FILENO);
    if((fd3=dup2(command->stderrPipe[1],STDERR_FILENO)) == -1) { /* we can't give error messages here */  exit(0); }
#endif

    retval=execl("/bin/bash","/bin/bash","-c",command->execString,NULL);

    char *msg="process terminated\n";
    write(STDOUT_FILENO,msg,sizeof(msg));

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    exit(retval);
  } else {
    //printf("Spawned PID %d\n",command->pid);
    //return;

    /* No need to keep the child side of pipes open for peisinit */
    close(command->stdinPipe[0]);
    close(command->stdoutPipe[1]);
    close(command->stderrPipe[1]);
    
    command->currState=1;
    /*printf("Started %s with pid %d\n",command->name,command->pid);*/
    sem_post(&command->spawnSem);

    waitpid(command->pid,&status,0);
    /*printf("Finished %s with pid %d\n",command->name,command->pid);*/

    usleep(100000);
    /*printf("Closing the pipes\n");*/
    command->currState=0;
    command->pid = 0;


    /* Close the remaining pipes */
    close(command->stdinPipe[1]);
    close(command->stdoutPipe[0]);
    close(command->stderrPipe[0]);
  }    

  /*printf("SpawnFn for %s stopped\n",command->name);*/
}

void execFn(InitCommand *command) {
  int i;
  int retval;
  pid_t pid;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  int stdinLogFD, stdoutLogFD, stderrLogFD;
  int len,len2,totlen,skip;
  void *inBuffer;
  char str[256];

  char buffer[4096];
  char buffer2[4096];
  char *value, *value2;

  char stdoutTupleName[256] , stderrTupleName[256], stdinTupleName[256];
  double t;

  /* Idea - add code to automatically append timestamps on each line. Write this to an "annoted" log file */
  while(command->reqState == PEIS_STATE_ON || command->reqState == PEIS_STATE_RESTART) {        

    fprintf(globalLogfile,"%8.3f S: Spawn %s\n",peisk_gettimef(),command->name); fflush(globalLogfile);

    /* Force the requested delay before spawning the process */
    usleep((int) (1000000.0*command->spawnDelay));

    /* Spawn this command */
    pthread_create(&command->spawnThread,&attr,(PosixThreadFn) spawnFn, (void*) command);
    sem_wait(&command->spawnSem);

    /* Ok, command has spawned */
    if(command->reqState == PEIS_STATE_RESTART) command->reqState=PEIS_STATE_ON;
    
    /* Open files to log stdin/out/err with */
    stdinLogFD = open(command->stdinFile,O_WRONLY | O_NONBLOCK | O_APPEND);
    if(!stdinLogFD) fprintf(stderr,"Failed to open '%s' for writing\n",command->stdinFile);
    stdoutLogFD = open(command->stdoutFile,O_WRONLY | O_NONBLOCK | O_APPEND);
    if(!stdoutLogFD) fprintf(stderr,"Failed to open '%s' for writing\n",command->stdoutFile);
    stderrLogFD = open(command->stderrFile,O_WRONLY | O_NONBLOCK | O_APPEND);
    if(!stderrLogFD) fprintf(stderr,"Failed to open '%s' for writing\n",command->stderrFile);   
    
    t=peisk_gettimef();
    snprintf(str,sizeof(str),"%8.3f: Spawn\n",t);
    write(stdinLogFD, str, strlen(str));
    write(stdoutLogFD, str, strlen(str));
    write(stderrLogFD, str, strlen(str));

    snprintf(stdinTupleName,sizeof(stdinTupleName),"components.%s.stdin",command->name);
    snprintf(stdoutTupleName,sizeof(stdoutTupleName),"components.%s.stdout",command->name);
    snprintf(stderrTupleName,sizeof(stderrTupleName),"components.%s.stderr",command->name);

    /* Start with empty stdout/err buffers unless already existing
       since previously. */        
    if(!peisk_getTuple(peisk_peisid(),stdoutTupleName,PEISK_KEEP_OLD))
      peisk_setStringTuple(stdoutTupleName,"");
    if(!peisk_getTuple(peisk_peisid(),stderrTupleName,PEISK_KEEP_OLD))
      peisk_setStringTuple(stderrTupleName,"");
     
    /* Always reset the stdin tuple so old data is not re-read */
    peisk_setStringTuple(stdinTupleName,"");

    /* Update tuplespace information for this command */
    command->currState = PEIS_STATE_ON;
    updateCommandTuple(command);

    /* Wait until it is finished */
    sem_post(&command->pipeSem);
    while(command->currState == PEIS_STATE_ON) {
      /* sleep(1) */

      usleep(100000);
      if(command->currState == PEIS_STATE_OFF) break;
      sem_wait(&command->pipeSem);

      /*
      fflush(command->stdoutPipe[1]);
      fflush(command->stderrPipe[1]);
      */


      /* See if there is anything new in stdout */
      while((len=read(command->stdoutPipe[0], (void*) buffer, sizeof(buffer)-1)) > 0) {
	/*printf("got %d bytes on stdout: %s\n",len,buffer);*/
	write(stdoutLogFD, buffer, len);
	for(i=0;i<len;i++) if(buffer[i] == 0) buffer[i] = '\n';
	PeisTuple *tuple = peisk_getTuple(peisk_peisid(),stdoutTupleName,PEISK_KEEP_OLD);
	if(tuple) {
	  len2=tuple->datalen;
	  value2=tuple->data;
	} else {
	  fprintf(stderr,"Error getting %d.%s\n",peisk_peisid(),stdoutTupleName); 
	  exit(0);
	}

	len2--; /* Remove trailing zero */
	if(len+len2+1>sizeof(buffer2)) {
	  skip = len+len2+1 - sizeof(buffer2);
	  memcpy(buffer2,value2+skip,len2-skip);
	  memcpy(buffer2+len2-skip,buffer,len);
	  totlen=len2+len-skip+1;
	  buffer2[len2+len-skip]=0;
	} else {
	  memcpy(buffer2,value2,len2);
	  memcpy(buffer2+len2,buffer,len);
	  buffer2[len2+len]=0;
	  totlen=len2+len+1;
	}
	peisk_setTuple(stdoutTupleName,totlen,buffer2,"text/plain",PEISK_ENCODING_ASCII);
      }

      /* See if there is anything new in stderr */
      while((len=read(command->stderrPipe[0], (void*) buffer, sizeof(buffer)-1)) > 0) {
	buffer[len]=0;
	write(stderrLogFD, buffer, len);
	for(i=0;i<len;i++) if(buffer[i] == 0) buffer[i] = '\n';
	PeisTuple *tuple = peisk_getTuple(peisk_peisid(),stderrTupleName,PEISK_KEEP_OLD);
	if(tuple) {
	  len2=tuple->datalen;
	  value2=tuple->data;
	} else {
	  fprintf(stderr,"Error getting tuple %d.%s\n",peisk_peisid(),stderrTupleName); exit(0);
	}
	
	if(len2 > strlen(value2)+1) {
	  printf("WARNING, received tuple is longer than it's string length\n");
	  len2=strlen(value2)+1;
	}
	len2--; /* Remove trailing zero */
	if(len+len2+1>sizeof(buffer2)) {
	  skip = len+len2+1 - sizeof(buffer2);
	  memcpy(buffer2,value2+skip,len2-skip);
	  memcpy(buffer2+len2-skip,buffer,len);
	  totlen=len2+len-skip+1;
	  buffer2[len2+len-skip]=0;
	} else {
	  if(len2 > 0)
	    memcpy(buffer2,value2,len2);
	  memcpy(buffer2+len2,buffer,len);
	  buffer2[len2+len]=0;
	  totlen=len2+len+1;
	}
	if(strlen(buffer2) > totlen-1) {
	  printf("Warning, error in totlen=%d. strlen=%d\n",totlen,strlen(buffer2));
	}
	peisk_setTuple(stderrTupleName,totlen,buffer2,"text/plain",PEISK_ENCODING_ASCII);
      }

      /* Read any new input tuples and feed to component */
      /* TODO - this should be with a callback function instead... */
      PeisTuple *tuple = peisk_getTuple(peisk_peisid(),stdinTupleName,0);
      if(tuple) {
	len=tuple->datalen;
	inBuffer=tuple->data;
	/* Note - we do only log those bytes which where actually written. This is why
	   len is updated here! */
	len=write(command->stdinPipe[1],inBuffer,len);
	write(stdinLogFD,inBuffer,len);
      }

      sem_post(&command->pipeSem);


      if(command->reqState == PEIS_STATE_OFF || command->reqState == PEIS_STATE_RESTART) {
	printf("Stopping process %s (pid %d)\n",command->name,command->pid);
	if(command->pid == 0) {
	  printf("Warning, attempting to stop already stopped process...\n"); break;
	}


	/* Command has been requested to be halted. Let's kill it as
	   gracefully as possible */
	/* First by using a soft do-quit tuple */
	peisk_setRemoteTuple(command->peisid,"do-quit",4,"yes","text/plain",PEISK_ENCODING_ASCII);
	usleep(500000);
	/* Then by using a kernel.do-quit tuple */
	if(command->currState == 1) {
	  peisk_setRemoteTuple(command->peisid,"kernel.do-quit",4,"yes","text/plain",PEISK_ENCODING_ASCII);
	  usleep(500000);
	}
	if(command->currState == 1) {
	  /* Otherwise with a CTRL-C */
	  if(kill(command->pid, SIGINT) != 0 && errno == ESRCH) command->currState=1;
	  else usleep(500000);
	}
	/* Otherwise with increasingly nastier signals */
	if(command->currState == 1) { kill(command->pid, SIGABRT); usleep(500000); }
	if(command->currState == 1) { kill(command->pid, SIGTERM); usleep(500000); }
	if(command->currState == 1) { kill(command->pid, SIGKILL); usleep(500000); }
      }
    }

    /* Update tuplespace information for this command */
    updateCommandTuple(command);

    fprintf(globalLogfile,"%8.3f S: Finish %s\n",peisk_gettimef(),command->name); fflush(globalLogfile);
    pthread_cancel(command->spawnThread);
    /* Force a short delay before spawning process again */
    if(command->reqState == PEIS_STATE_ON || command->reqState == PEIS_STATE_RESTART) usleep((int) (1000000.0*command->spawnDelay));
  }
}
