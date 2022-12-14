/* Copyright (C) 2003 MySQL AB

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

#ifndef NDB_KERNEL_TYPES_H
#define NDB_KERNEL_TYPES_H

#include <ndb_types.h>

typedef Uint16 NodeId; 
typedef Uint16 BlockNumber;
typedef Uint32 BlockReference;
typedef Uint16 GlobalSignalNumber;

enum Operation_t {
  ZREAD    = 0
  ,ZUPDATE  = 1
  ,ZINSERT  = 2
  ,ZDELETE  = 3
  ,ZWRITE   = 4
  ,ZREAD_EX = 5
#if 0
  ,ZREAD_CONSISTENT = 6
#endif
};

#endif




