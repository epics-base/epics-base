
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
 *	505 665 1831
 */

#include "iocinf.h"

void ioCounter::decrementOutstandingIO ()
{
    bool signalNeeded;
    this->mutex.lock ();
    if ( this->pndrecvcnt > 0u ) {
        this->pndrecvcnt--;
        if ( this->pndrecvcnt == 0u ) {
            signalNeeded = true;
        }
        else {
            signalNeeded = false;
        }
    }
    else {
        signalNeeded = true;
    }
    this->mutex.unlock ();

    if ( signalNeeded ) {
        this->ioDone.signal ();
    }
}

void ioCounter::decrementOutstandingIO ( unsigned seqNumber )
{
    bool signalNeeded;

    this->mutex.lock ();
    if ( this->readSeq == seqNumber ) {
        if ( this->pndrecvcnt > 0u ) {
            this->pndrecvcnt--;
            if ( this->pndrecvcnt == 0u ) {
                signalNeeded = true;
            }
            else {
                signalNeeded = false;
            }
        }
        else {
            signalNeeded = true;
        }
    }
    else {
        signalNeeded = false;
    }
    this->mutex.unlock ();

    if ( signalNeeded ) {
        this->ioDone.signal ();
    }
}

void ioCounter::showOutstandingIO ( unsigned level ) const
{
    printf ( "ioCounter at %p\n", this );
    printf ( "\tthere are %u unsatisfied IO operations blocking ca_pend_io()\n",
            this->pndrecvcnt );
    if ( level > 0u ) {
        printf ( "\tthe current read sequence number is %u\n",
                this->readSeq );
    }
    if ( level > 1u ) {
        printf ( "IO done event:\n");
        this->ioDone.show ( level - 2u );
    }
}
