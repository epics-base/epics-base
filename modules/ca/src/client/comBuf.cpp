/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*  
 *
 *                              
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *                                  
 *  Copyright, 1986, The Regents of the University of California.
 *                                  
 *           
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 */

#include <stdexcept>

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "comBuf.h"
#include "errlog.h"

bool comBuf::flushToWire ( wireSendAdapter & wire, const epicsTime & currentTime )
{
    unsigned index = this->nextReadIndex;
    unsigned finalIndex = this->commitIndex;
    while ( index < finalIndex ) {
        unsigned nBytes = wire.sendBytes ( 
            &this->buf[index], finalIndex - index, currentTime );
        if ( nBytes == 0u ) {
            this->nextReadIndex = index;
            return false;
        }
        index += nBytes;
    }
    this->nextReadIndex = index;
    return true;
}

// throwing the exception from a function that isnt inline 
// shrinks the GNU compiled object code
void comBuf::throwInsufficentBytesException () 
{
    throw comBuf::insufficentBytesAvailable ();
}

void comBuf::operator delete ( void * )
{
    // Visual C++ .net appears to require operator delete if
    // placement operator delete is defined? I smell a ms rat
    // because if I declare placement new and delete, but
    // comment out the placement delete definition there are
    // no undefined symbols.
    errlogPrintf ( "%s:%d this compiler is confused about placement delete - memory was probably leaked",
        __FILE__, __LINE__ );
}

comBufMemoryManager::~comBufMemoryManager () {}
