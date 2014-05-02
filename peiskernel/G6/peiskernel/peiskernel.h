/** \file peiskernel.h
    Defines the interface to the peiskernel library and main entrypoint for documenting all *functions* that implements the interface
	to the peiskernel. 
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

#ifndef PEISKERNEL_H
#define PEISKERNEL_H

#ifndef _SYS_SELECT_H
#include <sys/select.h>
#endif

#ifndef TUPLES_H
#include "peiskernel/tuples.h"
#endif

/* For CYGWIN  --AS 060816 */
#ifdef __CYGWIN__
#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0
#endif
#endif

/* We give a special doxygen page for all of the peis project here, which will be accessible as a convinient manpage */

/** \defgroup peis Ecologies of PEIS project

  The Ecologies of PEIS Project uses a number of different programs
  which are running on a number of different computers and robots. All
  these different programs have been developed to accomplish one
  specific task each, and by communicating and collaborating with each
  other these programs can together accomplish much more advanced
  tasks. In a way, the funding principle for the Ecologies of PEIS
  project is very similar to the  UNIX philosphy .  

  These different programs are all tied together using the peiskernel
  software library for communication and collaboration. As such, they
  all (should) accept the following common set of commandline options.

  peiskernel options:
  - --peis-id int                Specify an address to be used in the peis network
  - --peis-name name             Alternative name of component
  - --peis-user name             Alternative name of user
  - --peis-hostname name         Alternative name of host computer
  - --peis-port port             Select port for incomming connections
  - --peis-connect url           Specify an initial start connection to be used
  - --peis-debug-packages        Generates a lot of debug information
  - --peis-debug-routes          Print debuging information for routing tables
  - --peis-load file             Load extra commandlines from file
  - --peis-leaf                  Leaf node only allowing connections to localhost(s)
  - --peis-silent                Suppress all printouts to stdout
  - --peis-time-master           Overrides time synchronisation of ecology
  - --peis-package-loss float    Introduces an artifical package loss for debugging
  - --peis-print-status          Enable printing status info (default off)
  - --peis-print-connections     Enable printing connection info (default off)
  - --peis-print-package-errors  Enable printing package errors (default off)
  - --peis-print-tuple-errors    Enable printing tuple errors (default off)
  - --peis-print-debug-steps     Enable printing out lots of debugging steps (default off)
  - --peis-print-warnings        Enable printing of misc warnings about which only Mathias cares about
  - --peis-print-port-statistics Lots of port statistics that only Mathias cares about
  - --peis-network string        Set network connection string (default default)
  - --peis-set-tuple key value   Assign tuple value to key
  - --peis-net-metric value      Cost to communicate over network (default 2)
  - --peis-bluetooth device      Use given bluetooth adaptor

   The PEIS kernel is a software library written in pure C and with
   as few library dependencies as possible. The purpose of this
   library is to enable software programs to participate as PEIS
   components in the PEIS Ecology network. 

  Users of the peiskernel should primarily see the \ref tuples
  "Tuplespace layer" and/or the 
  \ref peiskmt "multithreaded Peis Kernel" for the basic
  operations of how to use this library.   

  Developers, or expert users, may also want to read up on the
  different modules and layers that implement the peiskernel. You may
  want to start with the \ref peisk "Peis Kernel" module.  

  Note that some of this documentation is only generated from the
  Doxygen.private file in the sourcecode. 

.SH See also:
  tuples(3), peisk(3), peiskmt(3)
*/
/*\{@*/



/**********************************************************************/
/*                                                                    */
/*      Public interface to the peis kernel core functions            */
/*                                                                    */
/**********************************************************************/

/** Callback hook for intercepting packages. Return non zero to stop further processing (routing) */
typedef int PeisHook(int port,int destination,int sender,int datalen,void *data);

/** Callback hook for intercepting packages. Return non zero to stop further processing (routing) */
typedef void PeisPeriodic(void *user);

/** Callback hook to be used when notified of hosts removed from the peis network. */
typedef void PeisDeadhostHook(int id,void *user);

/** True after initialization until we have performed a shutdown */
extern int peisk_isRunning();                                     

/** Prints all peiskernel specific options */
void peisk_printUsage(FILE *stream,short argc,char **args);     
/** Initializes peiskernel using any appropriate commandline
    options. */
void peisk_initialize(int *argc,char **args);                   
/** Stops the running peis kernel */
void peisk_shutdown();                                          
/** Performs one full event step for all connections */
void peisk_step();                                              
/** Add to list of hosts that will repeatedly be attempted to
    connected to */
void peisk_autoConnect(char *uri);

/** Adds a new hook to the given port, fails (-2) if port is
    invalid. Hooks are stacked with the most recently added one beeing
    the first  hook to call. Hooks are called sequentially in a chain
    upon receiving any packages, the last action that is taken is
    routing the package toward the destination. If any hook returns
    non-zero then the package is dropped and no other hooks/routing is
    performed on the package. */
int peisk_registerHookWithName(int port,PeisHook *hook,char *name);
/** Macro wrapper for registering a hook with a default name consisting of the function argument */
#define peisk_registerHook(port,hook) peisk_registerHookWithName(port,hook,#hook)

/** Frees the given hook at the given port number. Returns zero if hook was unused. */
int peisk_unregisterHook(int port,PeisHook *hook);                             

/** Register a hook to be called when a host is removed from the routing table (deleted from the ecology). */
void peisk_registerDeadhostHook(PeisDeadhostHook *fn,void *arg);


/** Wait given number of microseconds while maintaining all peis services */
void peisk_wait(int useconds);                                  
/** Waits until something interesting happens on the sockets, or until
    the given amount of time has passed. Does not call any other PEIS
    related functions (ie. it does not invoke the step function, and hence
    the maxUSeconds should be less than the cycle time of 10.000uS) */
void peisk_waitOneCycle(int maxUSeconds);                              
    

/** Gives the current time in seconds since the EPOCH with integer
    precision. Uses the synchronised PEIS vector clock. */
int peisk_gettime(); 
/** Gives the current time in seconds since the EPOCH with
    floatingpoint precision. Uses the synchronised PEIS vector clock. */
double peisk_gettimef();                        
/** Gives the current time in seconds and microseconds since the EPOCH
    with two long int pointer arguments where the result is
    stored. Uses the synchronised PEIS vector clock. */
void peisk_gettime2(int *t0,int *t1);
/** Gives the current time in seconds and microseconds since the EPOCH
    with two long int pointer arguments where the result is
    stored. Uses the native system clock. */
void peisk_getrawtime2(int *t0,int *t1);
/** Gives the current time in seconds since the EPOCH with
    floatingpoint precision. Uses the native system clock. */
double peisk_getrawtimef();                        

/** Sets all signals needed for peiskernel when using the system call select(2) */
void peisk_setSelectReadSignals(int *n,fd_set *read,fd_set *write,fd_set *excp);  


/** Returns the address / id of this peiskernel */
extern int peisk_peisid();

/** Gives the hostname of this peiskernel */
extern const char *peisk_hostname();

/** Convinience function to print binary data as a hexdump + ascii
    translation. */
void peisk_hexDump(void *data,int datalen);

/** Attempts to read another token from the given file and stores result in a
    freshly allocated memory buffer. Use "free" to deallocate memory
    buffer when no longer needed. Returns NULL in case of
    failure. Keeps track of linenumbers in the global variable
    peisk_lineno */
char *peisk_getToken(FILE *);

/** See peisk_getToken */
extern int peisk_lineno;


/** True if host is known and reachable according to routing tables. */
int peisk_isRoutable(int id);

/** \}@ peis */

#ifdef PEISK_PRIVATE
#include  <peiskernel/peiskernel_private.h>
#endif
#endif

