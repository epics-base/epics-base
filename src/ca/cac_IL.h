
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
    return ( *this->pVPrintfFunc ) ( pformat, args );
}

inline int cac::printf ( const char *pformat, ... )
{
    va_list theArgs;
    int status;

    va_start ( theArgs, pformat );
    
    status = this->vPrintf ( pformat, theArgs );
    
    va_end ( theArgs );
    
    return status;
}

inline double cac::connectionTimeout () const
{
    return this->connTMO;
}

inline const char * cac::userNamePointer ()
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

// the recv thread is not permitted to flush as this
// can result in a push / pull deadlock on the TCP pipe.
// Instead, the recv thread scheduals the flush with the 
// send thread which runs at a higher priority than the 
// send thread. The same applies to the UDP thread for
// locking hierarchy reasons.
inline bool cac::flushPermit () const
{
    if ( this->pRecvProcThread ) {
        if ( this->pRecvProcThread->isCurrentThread () ) {
            return false;
        }
    }
    if ( this->pudpiiu ) {
        if ( this->pudpiiu->isCurrentThread () ) {
            return false;
        }
    }
    return true;
}

#endif // cac_ILh