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

#define epicsExportSharedSymbols
#include "casdef.h"
#include "casChannelI.h"

casChannel::casChannel ( const casCtx & /* ctx */ ) : 
    pChanI ( 0 )
{
}

casChannel::~casChannel () 
{
    if ( this->pChanI ) {
        this->pChanI->casChannelDestroyFromInterfaceNotify ();
    }
}

void casChannel::destroyRequest ()
{
    this->pChanI = 0;
    this->destroy ();
}

casPV * casChannel::getPV ()
{
    if ( this->pChanI ) {
	    casPVI & pvi = this->pChanI->getPVI ();
        return pvi.apiPointer ();
    }
    else {
        return 0;
    }
}

void casChannel::setOwner ( const char * const /* pUserName */, 
	const char * const /* pHostName */ )
{
	//
	// NOOP
	//
}

bool casChannel::readAccess () const 
{
	return true;
}

bool casChannel::writeAccess () const 
{
	return true;
}

bool casChannel::confirmationRequested () const 
{
	return false;
}

caStatus casChannel::beginTransaction ()
{
    return S_casApp_success;
}

void casChannel::endTransaction ()
{
}

caStatus casChannel::read ( const casCtx & ctx, gdd & prototype )
{
    return ctx.getPV()->read ( ctx, prototype );
}

caStatus casChannel::write ( const casCtx & ctx, const gdd & value )
{
    return ctx.getPV()->write ( ctx, value );
}

caStatus casChannel::writeNotify ( const casCtx & ctx, const gdd & value )
{
    return ctx.getPV()->writeNotify ( ctx, value );
}

void casChannel::show ( unsigned level ) const
{
	if ( level > 2u ) {
		printf ( "casChannel: read access = %d\n",
			this->readAccess() );
		printf ( "casChannel: write access = %d\n",
			this->writeAccess() );
		printf ( "casChannel: confirmation requested = %d\n",
			this->confirmationRequested() );
	}
}

void casChannel::destroy ()
{
	delete this;
}

void casChannel::postAccessRightsEvent ()
{
    if ( this->pChanI ) {
	    this->pChanI->postAccessRightsEvent ();
    }
}

