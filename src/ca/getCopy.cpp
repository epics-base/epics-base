
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

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "oldAccess.h"
#include "cac.h"

epicsSingleton < tsFreeList < class getCopy, 1024 > > getCopy::pFreeList;

getCopy::getCopy ( oldCAC &cacCtxIn, oldChannelNotify &chanIn, 
                  unsigned typeIn, arrayElementCount countIn, void *pValueIn ) :
    count ( countIn ), cacCtx ( cacCtxIn ), chan ( chanIn ), pValue ( pValueIn ), 
        readSeq ( cacCtxIn.sequenceNumberOfOutstandingIO () ), type ( typeIn )
{
    cacCtxIn.incrementOutstandingIO ();
}

getCopy::~getCopy () 
{
}

void getCopy::cancel ()
{
    this->cacCtx.decrementOutstandingIO ( this->readSeq );
}

void getCopy::destroy ()
{
    delete this;
}

void getCopy::completion ( unsigned typeIn, 
    arrayElementCount countIn, const void *pDataIn )
{
    if ( this->type == typeIn ) {
        memcpy ( this->pValue, pDataIn, dbr_size_n ( typeIn, countIn ) );
        this->cacCtx.decrementOutstandingIO ( this->readSeq );
    }
    else {
        this->exception ( ECA_INTERNAL, 
            "bad data type match in get copy back response",
            typeIn, countIn);
    }
    delete this;
}

void getCopy::exception (
    int status, const char *pContext, unsigned /* typeIn */, arrayElementCount /* countIn */ )
{
    if ( status != ECA_CHANDESTROY ) {
        this->cacCtx.exception ( status, pContext, 
            __FILE__, __LINE__, this->chan, this->type, 
            this->count, CA_OP_GET );
    }
    delete this;
}

void getCopy::show ( unsigned level ) const
{
    int tmpType = static_cast <int> ( this->type );
    ::printf ( "read copy IO at %p, type %s, element count %lu\n", 
        static_cast <const void *> ( this ), dbf_type_to_text ( tmpType ), this->count );
    if ( level > 0u ) {
        ::printf ( "\tsequence number %u, user's storage %p\n",
            this->readSeq, static_cast <const void *> ( this->pValue ) );
    }
}
