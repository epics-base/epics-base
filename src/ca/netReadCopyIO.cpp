
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

tsFreeList < class netReadCopyIO > netReadCopyIO::freeList;

netReadCopyIO::netReadCopyIO ( nciu &chanIn, unsigned typeIn, unsigned long countIn, 
                              void *pValueIn, unsigned seqNumberIn ) :
    baseNMIU (chanIn), type (typeIn), count (countIn), 
    pValue (pValueIn), seqNumber (seqNumberIn)
{
    chanIn.incrementOutstandingIO ();
}

netReadCopyIO::~netReadCopyIO () 
{
}

void netReadCopyIO::disconnect ( const char *pHostName )
{
    this->exceptionNotify ( ECA_DISCONN, pHostName );
    delete this;
}

void netReadCopyIO::completionNotify ()
{
    this->exceptionNotify ( ECA_INTERNAL, "get completion callback with no data?" );
}

void netReadCopyIO::completionNotify ( unsigned type, unsigned long count, const void *pData )
{
    if ( type <= (unsigned) LAST_BUFFER_TYPE ) {
#       ifdef CONVERSION_REQUIRED 
            (*cac_dbr_cvrt[type]) ( pData, this->pValue, FALSE, count );
#       else
            memcpy ( this->pValue, pData, dbr_size_n ( type, count ) );
#       endif
        chan.decrementOutstandingIO (this->seqNumber);
    }
    else {
        this->exceptionNotify ( ECA_INTERNAL, "bad data type in message" );
    }
}

void netReadCopyIO::exceptionNotify ( int status, const char *pContext )
{
    ca_signal (status, pContext);
}

void netReadCopyIO::exceptionNotify ( int status, const char *pContext, unsigned type, unsigned long count )
{
    ca_signal_formated (status, __FILE__, __LINE__, "%s type=%d count=%ld\n", 
        pContext, type, count);
}

void * netReadCopyIO::operator new ( size_t size )
{
    return netReadCopyIO::freeList.allocate ( size );
}

void netReadCopyIO::operator delete ( void *pCadaver, size_t size )
{
    netReadCopyIO::freeList.release ( pCadaver, size );
}
