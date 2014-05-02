/* general.h
   Contains header information for *all* source files in the project.

   Copyright (C) 2000  Mathias Broxvall

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
#include <stdlib.h>
#include <zlib.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

/*** More includes follows after typedeclarations ***/
#ifndef M_PI
#define M_PI 3.14159265
#endif

#ifndef M_PI2
#define M_PI2 2.0*M_PI
#endif

#define DEBUG(A) {fprintf(stderr,A); fflush(stderr);}

void generalInit();
double frandom();
int fileExists(char *);

/* A modulus operations which handles negative results safly */
int mymod(int v,int m);

double timeNow();
