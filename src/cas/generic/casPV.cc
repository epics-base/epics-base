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
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

//
// Notes:
//
// 1) Always verify that pPVI isnt nill prior to using it
//

#include "server.h"
#include "casPVIIL.h" 	// casPVI inline func
#include "casCtxIL.h" 	// casCtx inline func

epicsShareFunc casPV::casPV ()
{
}

//
// This constructor is preserved for backwards compatibility only.
// Please do _not_ use this constructor.
//
epicsShareFunc casPV::casPV (caServer &)
{
}

epicsShareFunc casPV::~casPV ()
{
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
epicsShareFunc casChannel *casPV::createChannel (const casCtx &ctx, const char * const,
		const char * const)
{
	return new casChannel (ctx);
}

//
// casPV::interestRegister()
//
epicsShareFunc caStatus casPV::interestRegister ()
{
	return S_casApp_success;
}

//
// casPV::interestDelete()
//
epicsShareFunc void casPV::interestDelete ()
{
}

//
// casPV::beginTransaction()
//
epicsShareFunc caStatus casPV::beginTransaction () 
{
	return S_casApp_success;
}

//
// casPV::endTransaction()
//
epicsShareFunc void casPV::endTransaction ()
{
}

//
// casPV::read()
//
epicsShareFunc caStatus casPV::read (const casCtx &, gdd &)
{
	return S_casApp_noSupport;
}

//
// casPV::write()
//
epicsShareFunc caStatus casPV::write (const casCtx &, const gdd &)
{
	return S_casApp_noSupport;
}

//
// casPV::bestExternalType()
//
epicsShareFunc aitEnum casPV::bestExternalType () const
{
	return aitEnumString;
}

//
// casPV::maxDimension()
// (base returns zero - scalar)
//
epicsShareFunc unsigned casPV::maxDimension () const
{
	return 0u;
}

//
// casPV::maxBound()
// (base returns scalar bound independent of the dimension arg)
//
epicsShareFunc aitIndex casPV::maxBound (unsigned /* dimension */) const
{
	return 1u;
}

//
// casPV::show (unsigned level) 
//
epicsShareFunc void casPV::show (unsigned level) const
{
    casPVI::show (level);
}

//
// Server tool calls this function to post a PV event.
//
epicsShareFunc void casPV::postEvent (const casEventMask &select, const gdd &event)
{
	this->casPVI::postEvent (select, event);
}

//
// Find the server associated with this PV
// ****WARNING****
// this returns NULL if the PV isnt currently installed
// into a server.
// ***************
//
epicsShareFunc caServer *casPV::getCAS () const
{
	return this->casPVI::getExtServer ();
}

//
// casPV::apiPointer()
//
casPV *casPV::apiPointer ()
{
    return this;
}

