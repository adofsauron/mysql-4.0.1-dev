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

#ifndef FS_REF_H
#define FS_REF_H

#include "SignalData.hpp"

/**
 * FsRef - Common signal class for all REF signals sent from Ndbfs
 * GSN_FSCLOSEREF, GSN_FSOPENREF, GSN_FSWRITEREF, GSN_FSREADREF,
 * GSN_FSSYNCREF
 */


/**
 * 
 * SENDER:  Ndbfs
 * RECIVER: 
 */
class FsRef {
  /**
   * Reciver(s)
   */
  friend class Dbdict;
  friend class Backup;

  /**
   * Sender(s)
   */
  friend class Ndbfs;
  friend class VoidFs;

  /**
   * For printing
   */
  friend bool printFSREF(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo);

public:
 /**
 * Enum type for errorCode
 */
  enum NdbfsErrorCodeType {
    fsErrNone=0,
    fsErrHardwareFailed=1,
    fsErrUserError=2,
    fsErrEnvironmentError=3,
    fsErrTemporaryNotAccessible=4,
    fsErrNoSpaceLeftOnDevice=5,
    fsErrPermissionDenied=6,
    fsErrInvalidParameters=7,
    fsErrUnknown=8,
    fsErrNoMoreResources=9,
    fsErrFileDoesNotExist=10,
    fsErrReadUnderflow = 11,
    fsErrMax
  };
  /**
   * Length of signal
   */
  STATIC_CONST( SignalLength = 4 );

private:

  /**
   * DATA VARIABLES
   */
  UintR userPointer;          // DATA 0
  UintR errorCode;            // DATA 1
  UintR osErrorCode;          // DATA 2
  UintR senderData;

  static NdbfsErrorCodeType getErrorCode(const UintR & errorcode);
  static void setErrorCode(UintR & errorcode, NdbfsErrorCodeType errorcodetype);
  static void setErrorCode(UintR & errorcode, UintR errorcodetype);

};


inline
FsRef::NdbfsErrorCodeType 
FsRef::getErrorCode(const UintR & errorcode){
  return (NdbfsErrorCodeType)errorcode;
}

inline
void
FsRef::setErrorCode(UintR & errorcode, NdbfsErrorCodeType errorcodetype){
  ASSERT_MAX(errorcodetype, fsErrMax, "FsRef::setErrorCode");
  errorcode = (UintR)errorcodetype;
}

inline
void
FsRef::setErrorCode(UintR & errorcode, UintR errorcodetype){
  ASSERT_MAX(errorcodetype, fsErrMax, "FsRef::setErrorCode");
  errorcode = errorcodetype;
}




#endif
