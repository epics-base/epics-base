
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
#include "netReadCopyIO_IL.h"
#include "nciu_IL.h"

tsFreeList < class netReadCopyIO, 1024 > netReadCopyIO::freeList;

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
    this->baseNMIU::destroy ();
}

void netReadCopyIO::completionNotify ()
{
    this->exceptionNotify ( ECA_INTERNAL, "get completion callback with no data?" );
}

void netReadCopyIO::completionNotify ( unsigned typeIn, 
    unsigned long countIn, const void *pDataIn )
{
    if ( typeIn <= (unsigned) LAST_BUFFER_TYPE ) {
#       ifdef CONVERSION_REQUIRED 
            (*cac_dbr_cvrt[type]) ( pDataIn, this->pValue, 
			FALSE, countIn );
#       else
            memcpy ( this->pValue, pDataIn, 
                 dbr_size_n ( typeIn, countIn ) );
#       endif
        this->chan.decrementOutstandingIO (this->seqNumber);
    }
    else {
        this->exceptionNotify ( ECA_INTERNAL, "bad data type in message" );
    }
}

void netReadCopyIO::exceptionNotify ( int status, const char *pContext )
{
    ca_signal (status, pContext);
}

void netReadCopyIO::exceptionNotify ( int status, 
    const char *pContextIn, unsigned typeIn, unsigned long countIn )
{
    ca_signal_formated (status, __FILE__, __LINE__, 
        "%s type=%d count=%ld\n", 
        pContextIn, typeIn, countIn);
}

void netReadCopyIO::show ( unsigned level ) const
{
    printf ( "read copy IO at %p, type %s, element count %u\n", 
        this, dbf_type_to_text ( this->type ), this->count );
    if ( level > 0u ) {
        printf ( "\tsequence number %u, user's storage %p\n",
            this->seqNumber, this->pValue );
        this->baseNMIU::show ( level - 1u );
    }
}
