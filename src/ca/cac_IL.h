
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

inline void cac::lockOutstandingIO () const
{
    this->defaultMutex.lock ();
}

inline void cac::unlockOutstandingIO () const
{
    this->defaultMutex.unlock ();
}

