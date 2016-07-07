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
#include "casAsyncReadIOI.h"

casAsyncReadIO::casAsyncReadIO ( const casCtx & ctx ) :
    pAsyncReadIOI ( new casAsyncReadIOI ( *this, ctx ) ) {}

void casAsyncReadIO::serverInitiatedDestroy ()
{
    this->pAsyncReadIOI = 0;
    this->destroy ();
}

casAsyncReadIO::~casAsyncReadIO ()
{
    if ( this->pAsyncReadIOI ) {
        throw std::logic_error ( 
            "the server library *must* initiate asynchronous IO destroy" );
    }
}

caStatus casAsyncReadIO::postIOCompletion ( 
    caStatus completionStatusIn, const gdd & valueRead )
{
    if ( this->pAsyncReadIOI ) {
	    return this->pAsyncReadIOI->postIOCompletion (
            completionStatusIn, valueRead );
    }
    else {
        return S_cas_redundantPost;
    }
}

void casAsyncReadIO::destroy ()
{
    delete this;
}



