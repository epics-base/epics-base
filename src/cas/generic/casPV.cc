/*
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 *
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

epicsShareFunc casPV::~casPV()
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
epicsShareFunc caStatus casPV::interestRegister()
{
	return S_casApp_success;
}

//
// casPV::interestDelete()
//
epicsShareFunc void casPV::interestDelete()
{
}

//
// casPV::beginTransaction()
//
epicsShareFunc caStatus casPV::beginTransaction() 
{
	return S_casApp_success;
}

//
// casPV::endTransaction()
//
epicsShareFunc void casPV::endTransaction()
{
}

//
// casPV::read()
//
epicsShareFunc caStatus casPV::read(const casCtx &, gdd &)
{
	return S_casApp_noSupport;
}

//
// casPV::write()
//
epicsShareFunc caStatus casPV::write(const casCtx &, gdd &)
{
	return S_casApp_noSupport;
}

//
// casPV::bestExternalType()
//
epicsShareFunc aitEnum casPV::bestExternalType() const
{
	return aitEnumString;
}

//
// casPV::maxDimension()
// (base returns zero - scalar)
//
epicsShareFunc unsigned casPV::maxDimension() const
{
	return 0u;
}

//
// casPV::maxBound()
// (base returns scalar bound independent of the dimension arg)
//
epicsShareFunc aitIndex casPV::maxBound(unsigned /* dimension */) const
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
epicsShareFunc void casPV::postEvent (const casEventMask &select, gdd &event)
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
epicsShareFunc caServer *casPV::getCAS() const
{
	return this->casPVI::getExtServer();
}

//
// casPV::apiPointer()
//
casPV *casPV::apiPointer ()
{
    return this;
}

