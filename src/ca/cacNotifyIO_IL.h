
/*  $Id$
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

inline void cacNotifyIO::completionNotify ()
{
    this->notify.completionNotify ();
}

inline void cacNotifyIO::completionNotify ( unsigned type, unsigned long count, const void *pData )
{
    this->notify.completionNotify ( type, count, pData );
}

inline void cacNotifyIO::exceptionNotify ( int status, const char *pContext )
{
    this->notify.exceptionNotify ( status, pContext );
}

inline void cacNotifyIO::exceptionNotify ( int status, const char *pContext, unsigned type, unsigned long count )
{
    this->notify.exceptionNotify ( status, pContext, type, count );
}
