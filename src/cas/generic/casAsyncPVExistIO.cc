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
#include "casAsyncPVExistIOI.h"

casAsyncPVExistIO::casAsyncPVExistIO ( const casCtx & ctx ) :
    pAsyncPVExistIOI ( new casAsyncPVExistIOI ( *this, ctx ) ) {}

void casAsyncPVExistIO::serverInitiatedDestroy ()
{
    this->pAsyncPVExistIOI = 0;
    this->destroy ();
}

casAsyncPVExistIO::~casAsyncPVExistIO ()
{
    if ( this->pAsyncPVExistIOI ) {
        throw std::logic_error ( 
            "the server library *must* initiate asynchronous IO destroy" );
    }
}

caStatus casAsyncPVExistIO::postIOCompletion ( const pvExistReturn & retValIn )
{
    if ( this->pAsyncPVExistIOI ) {
	    return this->pAsyncPVExistIOI->postIOCompletion ( retValIn );
    }
    else {
        return S_cas_redundantPost;
    }
}

void casAsyncPVExistIO::destroy ()
{
    delete this;
}

