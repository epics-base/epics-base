

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

inline ioCounter::ioCounter () : pndrecvcnt ( 0u ), readSeq ( 0u )
{
}

inline void ioCounter::incrementOutstandingIO ()
{
    this->mutex.lock ();
    if ( this->pndrecvcnt < UINT_MAX ) {
        this->pndrecvcnt++;
    }
    this->mutex.unlock ();
}

inline void ioCounter::lockOutstandingIO () const
{
    this->mutex.lock ();
}

inline void ioCounter::unlockOutstandingIO () const
{
    this->mutex.unlock ();
}

inline unsigned ioCounter::readSequenceOfOutstandingIO () const
{
    return this->readSeq;
}

inline unsigned ioCounter::currentOutstandingIOCount () const
{
    return this->pndrecvcnt;
}

inline void ioCounter::waitForCompletionOfIO ( double delaySec )
{
    this->ioDone.wait ( delaySec );
}

inline void ioCounter::cleanUpOutstandingIO ()
{
    this->mutex.lock ();

    this->readSeq++;
    this->pndrecvcnt = 0u;

    this->mutex.unlock ();
}


