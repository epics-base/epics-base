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
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#include <string>
#include <stdexcept>

#define epicsExportSharedSymbols
#include "casdef.h"
#include "casAsyncPVAttachIOI.h"

casAsyncPVAttachIO::casAsyncPVAttachIO ( const casCtx &ctx ) :
    pAsyncPVAttachIOI ( new casAsyncPVAttachIOI ( *this, ctx ) ) 
{
}

void casAsyncPVAttachIO::serverInitiatedDestroy ()
{
    this->pAsyncPVAttachIOI = 0;
    this->destroy ();
}

casAsyncPVAttachIO::~casAsyncPVAttachIO ()
{
    if ( this->pAsyncPVAttachIOI ) {
        throw std::logic_error ( 
            "the server library *must* initiate asynchronous IO destroy" );
    }
}

caStatus casAsyncPVAttachIO::postIOCompletion ( const pvAttachReturn & retValIn )
{
    if ( this->pAsyncPVAttachIOI ) {
	    return this->pAsyncPVAttachIOI->postIOCompletion ( retValIn );
    }
    else {
        return S_cas_redundantPost;
    }
}

void casAsyncPVAttachIO::destroy ()
{
    delete this;
}

casAsyncPVCreateIO::casAsyncPVCreateIO ( const casCtx & ctx ) : 
		casAsyncPVAttachIO ( ctx ) 
{
}

casAsyncPVCreateIO::~casAsyncPVCreateIO ()
{
}
