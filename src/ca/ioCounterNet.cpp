
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

ioCounterNet::ioCounterNet () : pndrecvcnt ( 0u ), readSeq ( 0u )
{
}

void ioCounterNet::increment ()
{
    epicsAutoMutex locker ( this->mutex );
    if ( this->pndrecvcnt < UINT_MAX ) {
        this->pndrecvcnt++;
    }
    else {
        throwWithLocation ( caErrorCode (ECA_INTERNAL) );
    }
}

unsigned ioCounterNet::sequenceNumber () const
{
    return this->readSeq;
}

unsigned ioCounterNet::currentCount () const
{
    return this->pndrecvcnt;
}

void ioCounterNet::waitForCompletion ( double delaySec )
{
    this->ioDone.wait ( delaySec );
}

void ioCounterNet::cleanUp ()
{
    epicsAutoMutex locker ( this->mutex );
    this->readSeq++;
    this->pndrecvcnt = 0u;
}

void ioCounterNet::decrement ()
{
    bool signalNeeded;

    {
        epicsAutoMutex locker ( this->mutex ); 
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

    if ( signalNeeded ) {
        this->ioDone.signal ();
    }
}

void ioCounterNet::decrement ( unsigned seqNumber )
{
    bool signalNeeded;

    {
        epicsAutoMutex locker ( this->mutex );
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
    }

    if ( signalNeeded ) {
        this->ioDone.signal ();
    }
}

void ioCounterNet::show ( unsigned level ) const
{
    printf ( "ioCounterNet at %p\n", 
        static_cast <const void *> ( this ) );
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
