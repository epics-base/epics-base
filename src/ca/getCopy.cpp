/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

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

#include <stdexcept>

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "oldAccess.h"
#include "cac.h"

getCopy::getCopy ( ca_client_context &cacCtxIn, oldChannelNotify &chanIn, 
                  unsigned typeIn, arrayElementCount countIn, void *pValueIn ) :
    count ( countIn ), cacCtx ( cacCtxIn ), chan ( chanIn ), pValue ( pValueIn ), 
        ioSeqNo ( cacCtxIn.sequenceNumberOfOutstandingIO () ), type ( typeIn )
{
    cacCtxIn.incrementOutstandingIO ( cacCtxIn.sequenceNumberOfOutstandingIO () );
}

getCopy::~getCopy () 
{
}

void getCopy::cancel ()
{
    this->cacCtx.decrementOutstandingIO ( this->ioSeqNo );
}

void getCopy::completion ( unsigned typeIn, 
    arrayElementCount countIn, const void *pDataIn )
{
    if ( this->type == typeIn ) {
        unsigned size = dbr_size_n ( typeIn, countIn );
        memcpy ( this->pValue, pDataIn, size );
        this->cacCtx.decrementOutstandingIO ( this->ioSeqNo );
    }
    else {
        this->exception ( ECA_INTERNAL, 
            "bad data type match in get copy back response",
            typeIn, countIn);
    }
    this->cacCtx.destroyGetCopy ( *this );
}

void getCopy::exception (
    int status, const char *pContext, unsigned /* typeIn */, arrayElementCount /* countIn */ )
{
    if ( status != ECA_CHANDESTROY ) {
        this->cacCtx.exception ( status, pContext, 
            __FILE__, __LINE__, this->chan, this->type, 
            this->count, CA_OP_GET );
    }
    this->cacCtx.destroyGetCopy ( *this );
}

void getCopy::show ( unsigned level ) const
{
    int tmpType = static_cast <int> ( this->type );
    ::printf ( "read copy IO at %p, type %s, element count %lu\n", 
        static_cast <const void *> ( this ), dbf_type_to_text ( tmpType ), this->count );
    if ( level > 0u ) {
        ::printf ( "\tIO sequence number %u, user's storage %p\n",
            this->ioSeqNo, static_cast <const void *> ( this->pValue ) );
    }
}

void * getCopy::operator new ( size_t ) // X aCC 361
{
    // The HPUX compiler seems to require this even though no code
    // calls it directly
    throw std::logic_error ( "why is the compiler calling private operator new" );
}

void getCopy::operator delete ( void * )
{
    // Visual C++ .net appears to require operator delete if
    // placement operator delete is defined? I smell a ms rat
    // because if I declare placement new and delete, but
    // comment out the placement delete definition there are
    // no undefined symbols.
    errlogPrintf ( "%s:%d this compiler is confused about placement delete - memory was probably leaked",
        __FILE__, __LINE__ );
}

