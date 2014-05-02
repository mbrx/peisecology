/* general.c
   Some quite general utility algorithms.

   Copyright (C) 2004  Mathias Broxvall

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

#include <sys/time.h>
#include "general.h"

double frandom() { return (rand() % (1<<30)) / ((double) (1<<30)); }

int mymod(int v,int m) {
  int tmp=v % m;
  while(tmp < 0) tmp += m;
  return tmp;
}

int fileExists(char *name) {
  FILE *fp = fopen(name,"rb");
  if(fp) { fclose(fp); return 1; }
  return 0;
}
double timeNow() {
  struct timeval tv;
  gettimeofday(&tv,NULL);
  return tv.tv_sec + 1e-6 * tv.tv_usec;
}
