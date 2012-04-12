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

#include <string>
#include <stdexcept>

#include "errlog.h"

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "oldAccess.h"
#include "cac.h"

getCopy::getCopy ( 
    epicsGuard < epicsMutex > & guard, ca_client_context & cacCtxIn, 
    oldChannelNotify & chanIn, unsigned typeIn, 
    arrayElementCount countIn, void * pValueIn ) :
    count ( countIn ), cacCtx ( cacCtxIn ), chan ( chanIn ), pValue ( pValueIn ), 
        ioSeqNo ( 0 ), type ( typeIn )
{
    this->ioSeqNo = cacCtxIn.sequenceNumberOfOutstandingIO ( guard );
    cacCtxIn.incrementOutstandingIO ( guard, this->ioSeqNo );
}

getCopy::~getCopy () 
{
}

void getCopy::cancel ()
{
    epicsGuard < epicsMutex > guard ( this->cacCtx.mutexRef () );
    this->cacCtx.decrementOutstandingIO ( guard, this->ioSeqNo );
}

void getCopy::completion ( 
    epicsGuard < epicsMutex > & guard, unsigned typeIn, 
    arrayElementCount countIn, const void *pDataIn )
{
    if ( this->type == typeIn ) {
        unsigned size = dbr_size_n ( typeIn, countIn );
        memcpy ( this->pValue, pDataIn, size );
        this->cacCtx.decrementOutstandingIO ( guard, this->ioSeqNo );
        this->cacCtx.destroyGetCopy ( guard, *this );
        // this object destroyed by preceding function call
    }
    else {
        this->exception ( guard, ECA_INTERNAL, 
            "bad data type match in get copy back response",
            typeIn, countIn);
        // this object destroyed by preceding function call
    }
}

void getCopy::exception (
    epicsGuard < epicsMutex > & guard,
    int status, const char *pContext, 
    unsigned /* typeIn */, arrayElementCount /* countIn */ )
{
    oldChannelNotify & chanTmp ( this->chan );
    unsigned typeTmp ( this->type );
    arrayElementCount countTmp ( this->count );
    ca_client_context & caClientCtx ( this->cacCtx );
    // fetch client context and destroy prior to releasing
    // the lock and calling cb in case they destroy channel there
    this->cacCtx.destroyGetCopy ( guard, *this );
    if ( status != ECA_CHANDESTROY ) {
        caClientCtx.exception ( guard, status, pContext, 
            __FILE__, __LINE__, chanTmp, typeTmp, 
            countTmp, CA_OP_GET );
    }
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

