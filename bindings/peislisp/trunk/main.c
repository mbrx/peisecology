/* main.c */
/* This file is only for testing some aspects of the peislisp code. 
   Not to be used inside TC or anything else. 

   - Mathias 

    Copyright (C) 2005 - 2006  Mathias Broxvall

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


#include <stdio.h>
#include <unistd.h>
#include <peiskernel/peiskernel_mt.h>

extern int thread_test_init();

int main(short argc,char **args) {

  /* A simple test program for the lisp interface to the multithreaded peiskernel */
  /*peiskmt_initialize(&argc,args);*/
  peislisp_initialize("peislisp-main-test");

  sleep(120);

  peiskmt_shutdown();
}
