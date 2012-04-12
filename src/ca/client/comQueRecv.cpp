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
 *  Author Jeffrey O. Hill
 *  johill@lanl.gov
 */

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "iocinf.h"
#include "virtualCircuit.h"

comQueRecv::comQueRecv ( comBufMemoryManager & comBufMemoryManagerIn ): 
    comBufMemMgr ( comBufMemoryManagerIn ), nBytesPending ( 0u ) 
{
}

comQueRecv::~comQueRecv ()
{
    this->clear ();
}

void comQueRecv::clear ()
{
    comBuf *pBuf;
    while ( ( pBuf = this->bufs.get () ) ) {
        pBuf->~comBuf ();
        this->comBufMemMgr.release ( pBuf );
    }
    this->nBytesPending = 0u;
}

unsigned comQueRecv::copyOutBytes ( epicsInt8 *pBuf, unsigned nBytes )
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

unsigned comQueRecv::removeBytes ( unsigned nBytes )
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
{
    for ( unsigned i = 0u; i < sizeof ( *pStr ); i++ ) {
        pStr[0][i] = this->popInt8 ();
    }
}

void comQueRecv::pushLastComBufReceived ( comBuf & bufIn )
   
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
{
    epicsUInt16 tmp;
    if ( this->occupiedBytes() >= sizeof (tmp) ) {
        unsigned byte1 = this->popUInt8 ();
        unsigned byte2 = this->popUInt8 ();
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
            ( ( byte1 << 24u ) | ( byte2 << 16u ) |
              ( byte3 << 8u ) | byte4 );
    }
    else {
        comBuf::throwInsufficentBytesException ();
        tmp = 0u; // avoid compiler warnings
    }
    return tmp;
}

void comQueRecv::removeAndDestroyBuf ( comBuf & buf )
{
    this->bufs.remove ( buf );
    buf.~comBuf ();
    this->comBufMemMgr.release ( & buf );
}

epicsUInt8 comQueRecv::popUInt8 () 
{
    comBuf * pComBuf = this->bufs.first ();
    if ( ! pComBuf ) {
        comBuf::throwInsufficentBytesException ();
    }
    epicsUInt8 tmp = '\0';
    comBuf::popStatus status = pComBuf->pop ( tmp );
    if ( ! status.success ) {
        comBuf::throwInsufficentBytesException ();
    }
    if ( status.nowEmpty ) {
        this->removeAndDestroyBuf ( *pComBuf );
    }
    this->nBytesPending--;
    return tmp;
}

epicsUInt16 comQueRecv::popUInt16 ()
{
    comBuf * pComBuf = this->bufs.first ();
    if ( ! pComBuf ) {
        comBuf::throwInsufficentBytesException ();
    }
    // try first for all in one buffer efficent version
    epicsUInt16 tmp = 0;
    comBuf::popStatus status = pComBuf->pop ( tmp );
    if ( status.success ) {
        this->nBytesPending -= sizeof ( epicsUInt16 );
        if ( status.nowEmpty ) {
            this->removeAndDestroyBuf ( *pComBuf );
        }
        return tmp;
    }
    return this->multiBufferPopUInt16 ();
}

epicsUInt32 comQueRecv::popUInt32 ()
{
    comBuf *pComBuf = this->bufs.first ();
    if ( ! pComBuf ) {
        comBuf::throwInsufficentBytesException ();
    }
    // try first for all in one buffer efficent version
    epicsUInt32 tmp = 0;
    comBuf::popStatus status = pComBuf->pop ( tmp );
    if ( status.success ) {
        this->nBytesPending -= sizeof ( epicsUInt32 );
        if ( status.nowEmpty ) {
            this->removeAndDestroyBuf ( *pComBuf );
        }
        return tmp;
    }
    return this->multiBufferPopUInt32 ();
}

bool comQueRecv::popOldMsgHeader ( caHdrLargeArray & msg )
{
    // try first for all in one buffer efficent version
    comBuf * pComBuf = this->bufs.first ();
    if ( ! pComBuf ) {
        return false;
    }
    unsigned avail = pComBuf->occupiedBytes ();
    if ( avail >= sizeof ( caHdr ) ) {
        pComBuf->pop ( msg.m_cmmd );
        ca_uint16_t smallPostsize = 0;
        pComBuf->pop ( smallPostsize );
        msg.m_postsize = smallPostsize;
        pComBuf->pop ( msg.m_dataType );
        ca_uint16_t smallCount = 0;
        pComBuf->pop ( smallCount );
        msg.m_count = smallCount;
        pComBuf->pop ( msg.m_cid );
        pComBuf->pop ( msg.m_available );
        this->nBytesPending -= sizeof ( caHdr );
        if ( avail == sizeof ( caHdr ) ) {
            this->removeAndDestroyBuf ( *pComBuf );
        }
        return true;
    }
    else if ( this->occupiedBytes () >= sizeof ( caHdr ) ) {
        msg.m_cmmd = this->popUInt16 ();
        msg.m_postsize = this->popUInt16 ();
        msg.m_dataType = this->popUInt16 ();
        msg.m_count = this->popUInt16 ();
        msg.m_cid = this->popUInt32 ();
        msg.m_available = this->popUInt32 ();
        return true;
    }
    else {
        return false;
    }
}

