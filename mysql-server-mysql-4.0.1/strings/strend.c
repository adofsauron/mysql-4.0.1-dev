/* Copyright (C) 2000 MySQL AB

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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*  File   : strend.c
    Author : Richard A. O'Keefe.
    Updated: 23 April 1984
    Defines: strend()

    strend(s) returns a character pointer to the NUL which ends s.  That
    is,  strend(s)-s  ==  strlen(s). This is useful for adding things at
    the end of strings.  It is redundant, because  strchr(s,'\0')  could
    be used instead, but this is clearer and faster.
    Beware: the asm version works only if strlen(s) < 65535.
*/

#include <my_global.h>
#include "m_string.h"

#if	VaxAsm

char *strend(s)
const char *s;
{
  asm("locc $0,$65535,*4(ap)");
  asm("movl r1,r0");
}

#else	/* ~VaxAsm */

char *strend(register const char *s)
{
  while (*s++);
  return (char*) (s-1);
}

#endif	/* VaxAsm */
