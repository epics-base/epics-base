
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
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
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
            this->nBytesPending += pComBuf->copyIn ( bufIn );
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
}

// optimization here complicates this function somewhat
epicsUInt16 comQueRecv::popUInt16 ()
{
    epicsUInt16 tmp;
    comBuf *pComBuf = this->bufs.first ();
    if ( pComBuf ) {
        // try first for all in one buffer efficent version
        comBuf::statusPopUInt16 ret = pComBuf->popUInt16 ();
        if ( ret.success ) {
            if ( pComBuf->occupiedBytes() == 0u ) {
                this->bufs.remove ( *pComBuf );
                pComBuf->destroy ();
            }
            tmp = ret.val;
            this->nBytesPending -= sizeof(tmp);
        }
        else {
            // split between buffers runs slower
            tmp  = this->popUInt8() << 8u;
            tmp |= this->popUInt8() << 0u;
        }
        return tmp;
    }
    throw insufficentBytesAvailable ();
}

// optimization here complicates this function somewhat
epicsUInt32 comQueRecv::popUInt32 ()
{
    epicsUInt32 tmp;
    comBuf *pComBuf = this->bufs.first ();
    if ( pComBuf ) {
        // try first for all in one buffer efficent version 
        comBuf::statusPopUInt32 ret = pComBuf->popUInt32 ();
        if ( ret.success ) {
            if ( pComBuf->occupiedBytes() == 0u ) {
                this->bufs.remove ( *pComBuf );
                pComBuf->destroy ();
            }
            tmp = ret.val;
            this->nBytesPending -= sizeof(tmp);
        }
        else {
            // split between buffers runs slower
            tmp  = this->popUInt8() << 24u;
            tmp |= this->popUInt8() << 16u;
            tmp |= this->popUInt8() << 8u;
            tmp |= this->popUInt8() << 0u;
        }
        return tmp;
    }
    throw insufficentBytesAvailable ();
}
