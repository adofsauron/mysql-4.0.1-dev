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


/************************************************************************************************
Name:		NdbUtil.C
Include:	
Link:		
Author:		UABRONM Mikael Ronstr?m UAB/B/SD
Date:		991029
Version:	0.4
Description:	Utility classes for NDB API
Documentation:
Adjust:		991029  UABRONM   First version.
Comment:	
************************************************************************************************/

#include "NdbUtil.hpp"

NdbLabel::NdbLabel() :
  theNext(NULL)
{
}

NdbLabel::~NdbLabel()
{
}

NdbSubroutine::NdbSubroutine() :
  theNext(NULL)
{
}

NdbSubroutine::~NdbSubroutine()
{
}

NdbBranch::NdbBranch() :
  theSignal(NULL),
  theNext(NULL)
{
}

NdbBranch::~NdbBranch()
{
}

NdbCall::NdbCall() :
  theSignal(NULL),
  theNext(NULL)
{
}

NdbCall::~NdbCall()
{
}
