
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

#ifndef cac_ILh
#define cac_ILh

#include "recvProcessThread_IL.h"
#include "udpiiu_IL.h"

inline int cac::vPrintf ( const char *pformat, va_list args )
{
    if ( this ) {
        return ( *this->pVPrintfFunc ) ( pformat, args );
    }
    else {
        return vfprintf ( stderr, pformat, args );
    }
}

inline const char * cac::userNamePointer () const
{
    return this->pUserName;
}

inline void cac::ipAddrToAsciiAsynchronousRequestInstall ( ipAddrToAsciiAsynchronous & request )
{
    request.ioInitiate ( this->ipToAEngine );
}

inline unsigned cac::getInitializingThreadsPriority () const
{
    return this->initializingThreadsPriority;
}

inline void cac::incrementOutstandingIO ()
{
    this->ioCounter.increment ();
}

inline void cac::decrementOutstandingIO ()
{
    this->ioCounter.decrement ();
}

inline void cac::decrementOutstandingIO ( unsigned sequenceNo )
{
    this->ioCounter.decrement ( sequenceNo );
}

inline unsigned cac::sequenceNumberOfOutstandingIO () const
{
    return this->ioCounter.sequenceNumber ();
}

inline epicsMutex & cac::mutexRef ()
{
    return this->mutex;
}

#endif // cac_ILh

