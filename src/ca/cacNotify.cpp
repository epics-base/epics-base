
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

#include "iocinf.h"

cacNotify::cacNotify () : pIO (0)
{
}

cacNotify::~cacNotify ()
{
    cacNotifyIO *pTmpIO = this->pIO;
    if ( pTmpIO ) {
        this->pIO = 0;
        // the code fits together better in iother places
        // if the delete and the uninstall are speparate steps
        pTmpIO->uninstall ();
        delete pTmpIO;
    }
}

void cacNotify::completionNotify ()
{
    ca_printf ("CAC: IO completion with no handler installed?\n");
}

void cacNotify::completionNotify ( unsigned type, unsigned long count, const void *pData )
{
    ca_printf ("CAC: IO completion with no handler installed? type=%u count=%u data pointer=%p\n",
                type, count, pData);
}

void cacNotify::exceptionNotify ( int status, const char *pContext )
{
    ca_signal (status, pContext);
}

void cacNotify::exceptionNotify ( int status, const char *pContext, unsigned type, unsigned long count )
{
    ca_signal_formated (status, __FILE__, __LINE__, "%s type=%d count=%ld\n", 
        pContext, type, count);
}
