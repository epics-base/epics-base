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
#include "casPVI.h"

casPV::casPV () : 
    pPVI ( 0 )
{
}

//
// This constructor is preserved for backwards compatibility only.
// Please do _not_ use this constructor.
//
casPV::casPV ( caServer & ) :
    pPVI ( 0 )
{
}

casPV::~casPV ()
{
    if ( this->pPVI ) {
        this->pPVI->casPVDestroyNotify ();
    }
}

void casPV::destroyRequest ()
{
    this->pPVI = 0;
    this->destroy ();
}

//
// casPV::destroy()
//
// (this default action will _never_ be called while in the
// casPVI destructor)
//
void casPV::destroy ()
{
	delete this;
}

//
// casPV::createChannel()
//
casChannel *casPV::createChannel (
    const casCtx &ctx, const char * const, const char * const )
{
	return new casChannel ( ctx );
}

//
// casPV::interestRegister()
//
caStatus casPV::interestRegister ()
{
	return S_casApp_success;
}

//
// casPV::interestDelete()
//
void casPV::interestDelete ()
{
}

//
// casPV::beginTransaction()
//
caStatus casPV::beginTransaction ()
{
	return S_casApp_success;
}

//
// casPV::endTransaction()
//
void casPV::endTransaction ()
{
}

//
// casPV::read()
//
caStatus casPV::read (const casCtx &, gdd &)
{
	return S_casApp_noSupport;
}

//
// casPV::write()
//
caStatus casPV::write (const casCtx &, const gdd &)
{
	return S_casApp_noSupport;
}

//
// casPV::writeNotify()
//
caStatus casPV :: writeNotify (
    const casCtx & ctx, const gdd & val )
{
    // plumbed this way to preserve backwards 
    // compatibility with the old interface which
    // did not include a writeNotify interface
	return this->write ( ctx, val );
}

//
// casPV::bestExternalType()
//
aitEnum casPV::bestExternalType () const
{
	return aitEnumString;
}

//
// casPV::maxDimension()
// (base returns zero - scalar)
//
unsigned casPV::maxDimension () const
{
	return 0u;
}

//
// casPV::maxBound()
// (base returns scalar bound independent of the dimension arg)
//
aitIndex casPV::maxBound (unsigned /* dimension */) const
{
	return 1u;
}

//
// casPV::show (unsigned level) 
//
void casPV::show ( unsigned /* level */ ) const
{
}

//
// Server tool calls this function to post a PV event.
//
void casPV::postEvent ( const casEventMask &select, const gdd &event )
{
    if ( this->pPVI ) {
	    this->pPVI->postEvent ( select, event );
    }
}

//
// Find the server associated with this PV
// ****WARNING****
// this returns NULL if the PV isnt currently installed
// into a server.
// ***************
//
caServer * casPV::getCAS () const
{
    if ( this->pPVI ) {
	    return this->pPVI->getExtServer ();
    }
    else {
        return 0;
    }
}


