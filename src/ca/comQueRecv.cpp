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
 *  $Id$
 *
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *
 *  Author Jeffrey O. Hill
 *  johill@lanl.gov
 */

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "iocinf.h"
#include "virtualCircuit.h"

comQueRecv::comQueRecv ( comBufMemoryManager & comBufMemoryManagerIn ) epicsThrows (()) : 
    comBufMemMgr ( comBufMemoryManagerIn ), nBytesPending ( 0u ) 
{
}

comQueRecv::~comQueRecv () epicsThrows (())
{
    this->clear ();
}

void comQueRecv::clear () epicsThrows (())
{
    comBuf *pBuf;
    while ( ( pBuf = this->bufs.get () ) ) {
        pBuf->~comBuf ();
        this->comBufMemMgr.release ( pBuf );
    }
    this->nBytesPending = 0u;
}

unsigned comQueRecv::copyOutBytes ( epicsInt8 *pBuf, unsigned nBytes ) epicsThrows (())
{
    unsigned totalBytes = 0u;
    do {
        comBuf * pComBuf = this->bufs.first ();
        if ( ! pComBuf ) {
            this->nBytesPending -= totalBytes;
            return totalBytes;
        }
        totalBytes += pComBuf->copyOutBytes ( &pBuf[totalBytes], nBytes - totalBytes );
        if ( pComBuf->occupiedBytes () == 0u ) {
            this->bufs.remove ( *pComBuf );
            pComBuf->~comBuf ();
            this->comBufMemMgr.release ( pComBuf );
        }
    }
    while ( totalBytes < nBytes );
    this->nBytesPending -= totalBytes;
    return totalBytes;
}

unsigned comQueRecv::removeBytes ( unsigned nBytes ) epicsThrows (())
{
    unsigned totalBytes = 0u;
    unsigned bytesLeft = nBytes;
    while ( bytesLeft ) {
        comBuf * pComBuf = this->bufs.first ();
        if ( ! pComBuf ) {
            this->nBytesPending -= totalBytes;
            return totalBytes;
        }
        unsigned nBytesThisTime = pComBuf->removeBytes ( bytesLeft );
        if ( pComBuf->occupiedBytes () == 0u ) {
            this->bufs.remove ( *pComBuf );
            pComBuf->~comBuf ();
            this->comBufMemMgr.release ( pComBuf );
        }
        if ( nBytesThisTime == 0u) {
            break;
        }
        totalBytes += nBytesThisTime;
        bytesLeft = nBytes - totalBytes;
    }
    this->nBytesPending -= totalBytes;
    return totalBytes;
}

void comQueRecv::popString ( epicsOldString *pStr )
    epicsThrows (( comBuf::insufficentBytesAvailable ))
{
    for ( unsigned i = 0u; i < sizeof ( *pStr ); i++ ) {
        pStr[0][i] = this->popInt8 ();
    }
}

void comQueRecv::pushLastComBufReceived ( comBuf & bufIn )
    epicsThrows (())
{
    bufIn.commitIncomming ();
    comBuf * pComBuf = this->bufs.last ();
    if ( pComBuf ) {
        if ( pComBuf->unoccupiedBytes() ) {
            this->nBytesPending += pComBuf->push ( bufIn );
            pComBuf->commitIncomming ();
        }
    }
    unsigned bufBytes = bufIn.occupiedBytes();
    if ( bufBytes ) {
        this->nBytesPending += bufBytes;
        this->bufs.add ( bufIn );
    }
    else {
        bufIn.~comBuf ();
        this->comBufMemMgr.release ( & bufIn );
    }
}

// 1) split between buffers expected to run slower
// 2) using canonical unsigned tmp avoids ANSI C conversions to int
// 3) cast required because sizeof(unsigned) >= sizeof(epicsUInt32)
epicsUInt16 comQueRecv::multiBufferPopUInt16 ()
    epicsThrows (( comBuf::insufficentBytesAvailable ))
{
    epicsUInt16 tmp;
    if ( this->occupiedBytes() >= sizeof (tmp) ) {
        unsigned byte1 = this->popUInt8();
        unsigned byte2 = this->popUInt8();
        tmp = static_cast <epicsUInt16> ( ( byte1 << 8u ) | byte2 );
    }
    else {
        comBuf::throwInsufficentBytesException ();
        tmp = 0u;
    }
    return tmp;
}

// 1) split between buffers expected to run slower
// 2) using canonical unsigned temporary avoids ANSI C conversions to int
// 3) cast required because sizeof(unsigned) >= sizeof(epicsUInt32)
epicsUInt32 comQueRecv::multiBufferPopUInt32 ()
    epicsThrows (( comBuf::insufficentBytesAvailable ))
{
    epicsUInt32 tmp;
    if ( this->occupiedBytes() >= sizeof (tmp) ) {
        // 1) split between buffers expected to run slower
        // 2) using canonical unsigned temporary avoids ANSI C conversions to int
        // 3) cast required because sizeof(unsigned) >= sizeof(epicsUInt32)
        unsigned byte1 = this->popUInt8();
        unsigned byte2 = this->popUInt8();
        unsigned byte3 = this->popUInt8();
        unsigned byte4 = this->popUInt8();
        tmp = static_cast <epicsUInt32>
            ( ( byte1 << 24u ) | ( byte2 << 16u ) | //X aCC 392
              ( byte3 << 8u ) | byte4 );
    }
    else {
        comBuf::throwInsufficentBytesException ();
        tmp = 0u; // avoid compiler warnings
    }
    return tmp;
}

void comQueRecv::removeAndDestroyBuf ( comBuf & buf ) epicsThrows (())
{
    this->bufs.remove ( buf );
    buf.~comBuf ();
    this->comBufMemMgr.release ( & buf );
}
