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

comQueRecv::comQueRecv (): nBytesPending ( 0u )
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
        pBuf->destroy ();
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
            pComBuf->destroy ();
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
            pComBuf->destroy ();
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
        bufIn.destroy ();
    }
}

epicsUInt8 comQueRecv::popUInt8 ()
{
    comBuf *pComBuf = this->bufs.first ();
    if ( pComBuf ) {
        epicsUInt8 tmp = pComBuf->popUInt8 ();
        if ( pComBuf->occupiedBytes() == 0u ) {
            this->bufs.remove ( *pComBuf );
            pComBuf->destroy ();
        }
        this->nBytesPending--;
        return tmp;
    }
    throw insufficentBytesAvailable ();
    return 0;                   // make compiler happy
}

// optimization here complicates this function somewhat
epicsUInt16 comQueRecv::popUInt16 ()
{
    comBuf *pComBuf = this->bufs.first ();
    if ( pComBuf ) {
        epicsUInt16 tmp;
        try {
            // try first for all in one buffer efficent version
            tmp = pComBuf->popUInt16 ();
            if ( pComBuf->occupiedBytes() == 0u ) {
                this->bufs.remove ( *pComBuf );
                pComBuf->destroy ();
            }
            this->nBytesPending -= sizeof( tmp );
        }
        catch ( insufficentBytesAvailable & ) {
            // 1) split between buffers expected to run slower
            // 2) using canonical unsigned tmp avoids ANSI C conversions to int
            // 3) cast required because sizeof(unsigned) >= sizeof(epicsUInt32)
            unsigned byte1 = this->popUInt8();
            unsigned byte2 = this->popUInt8();
            tmp = static_cast <epicsUInt16> ( byte1 << 8u | byte2 );
        }
        return tmp;
    }
    throw insufficentBytesAvailable ();
    return 0; // make compiler happy
}

// optimization here complicates this function somewhat
epicsUInt32 comQueRecv::popUInt32 ()
{
    comBuf *pComBuf = this->bufs.first ();
    if ( pComBuf ) {
        epicsUInt32 tmp;
        try {
            // try first for all in one buffer efficent version 
            tmp = pComBuf->popUInt32 ();
            if ( pComBuf->occupiedBytes() == 0u ) {
                this->bufs.remove ( *pComBuf );
                pComBuf->destroy ();
            }
            this->nBytesPending -= sizeof ( tmp );
        }
        catch ( insufficentBytesAvailable & ) {
            // 1) split between buffers expected to run slower
            // 2) using canonical unsigned temporary avoids ANSI C conversions to int
            // 3) cast required because sizeof(unsigned) >= sizeof(epicsUInt32)
            unsigned byte1 = this->popUInt8();
            unsigned byte2 = this->popUInt8();
            unsigned byte3 = this->popUInt8();
            unsigned byte4 = this->popUInt8();
            tmp = static_cast <epicsUInt32>
                ( byte1 << 24u | byte2 << 16u | byte3 << 8u | byte4 ); //X aCC 392
        }
        return tmp;
    }
    throw insufficentBytesAvailable ();
    return 0; // make compiler happy
}
