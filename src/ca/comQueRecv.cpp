
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

#include "iocinf.h"
#include "comBuf_IL.h"

comQueRecv::comQueRecv ()
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
}

unsigned comQueRecv::occupiedBytes () const
{
    unsigned count = this->bufs.count ();
    unsigned nBytes;

    if ( count == 0u ) {
        nBytes = 0u;
    }
    else if ( count == 1u ) {
        nBytes = this->bufs.first ()->occupiedBytes ();
    }
    else {
        // this requires the compress operation in 
        // copyIn ( comBuf & bufIn )
        nBytes = this->bufs.first ()->occupiedBytes ();
        nBytes += this->bufs.last ()->occupiedBytes ();
        nBytes += ( count - 2u ) * comBuf::maxBytes ();
    }

    return nBytes;
}

bool comQueRecv::copyOutBytes ( void *pBuf, unsigned nBytes )
{
    char *pCharBuf = static_cast < char * > ( pBuf );

    // dont return partial message
    if ( nBytes > this->occupiedBytes () ) {
        return false;
    }

    unsigned bytesLeft = nBytes;
    while ( bytesLeft ) {
        comBuf * pComBuf = this->bufs.first ();
        assert ( pComBuf );
        bytesLeft -= pComBuf->copyOutBytes ( &pCharBuf[nBytes-bytesLeft], bytesLeft );
        if ( pComBuf->occupiedBytes () == 0u ) {
            this->bufs.remove ( *pComBuf );
            pComBuf->destroy ();
        }
    }

    return true;
}

void comQueRecv::pushLastComBufReceived ( comBuf & bufIn )
{
    comBuf *pLastBuf = this->bufs.last ();
    if ( pLastBuf ) {
        pLastBuf->copyIn ( bufIn );
    }
    if ( bufIn.occupiedBytes () ) {
        // move occupied bytes to the start of the buffer
        bufIn.compress ();
        this->bufs.add ( bufIn );
    }
    else {
        bufIn.destroy ();
    }
}

