
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

#include <iocinf.h>
#include <comBuf_IL.h>

comQueRecv::~comQueRecv ()
{
    comBuf *pBuf;

    this->mutex.lock ();
    while ( ( pBuf = this->bufs.get () ) ) {
        pBuf->destroy ();
    }
    this->mutex.unlock ();
}

unsigned comQueRecv::occupiedBytes () const
{
    this->mutex.lock ();

    unsigned count = this->bufs.count ();
    unsigned nBytes;
    if ( count >= 2u ) {
        nBytes = this->bufs.first ()->occupiedBytes ();
        nBytes += this->bufs.last ()->occupiedBytes ();
        nBytes += ( count - 2u ) * comBuf::maxBytes ();
    }
    else if ( count == 1u ) {
        nBytes = this->bufs.first ()->occupiedBytes ();
    }
    else {
        nBytes = 0u;
    }

    this->mutex.unlock ();

    return nBytes;
}

bool comQueRecv::copyOutBytes ( void *pBuf, unsigned nBytes )
{
    char *pCharBuf = static_cast < char * > ( pBuf );

    this->mutex.lock ();

    // dont return partial message
    if ( nBytes > this->occupiedBytes () ) {
        this->mutex.unlock ();
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

    this->mutex.unlock ();

    return true;
}

unsigned comQueRecv::fillFromWire ()
{
    // this approach requires that only one thread performs fill
    // but its advantage is that the lock is not held while filling

    comBuf *pComBuf = new comBuf;
    if ( ! pComBuf ) {
        // no way to be informed when memory is available
        threadSleep ( 0.5 );
        return 0u;
    }

    unsigned nNewBytes = pComBuf->fillFromWire ( *this );

    this->mutex.lock ();

    comBuf *pLastBuf = this->bufs.last ();
    if ( pLastBuf ) {
        pLastBuf->copyIn ( *pComBuf );
    }
    if ( pComBuf->occupiedBytes () ) {
        this->bufs.add ( *pComBuf );
    }
    else {
        pComBuf->destroy ();
    }

    this->mutex.unlock ();

    return nNewBytes;
}

