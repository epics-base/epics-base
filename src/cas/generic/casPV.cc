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
 * History
 * $Log$
 * Revision 1.3  1996/07/01 19:56:12  jhill
 * one last update prior to first release
 *
 * Revision 1.2  1996/06/21 02:30:53  jhill
 * solaris port
 *
 * Revision 1.1.1.1  1996/06/20 00:28:15  jhill
 * ca server installation
 *
 *
 */


//
// Notes:
//
// 1) Always verify that pPVI isnt nill prior to using it
//

#include <server.h>
#include <casPVIIL.h> 	// casPVI inline func
#include <casCtxIL.h> 	// casCtx inline func

casPV::casPV (const casCtx &ctx, const char * const pPVName) :
	casPVI (*ctx.getServer(), pPVName, *this)
{
	
}

casPV::~casPV()
{
}

//
// casPV::show()
//
void casPV::show(unsigned level) 
{
	if (level>2u) {
		printf ("casPV: Max simultaneous async io = %d\n",
			this->maxSimultAsyncOps());
		printf ("casPV: Best external type = %d\n",
			this->bestExternalType());
	}
}

//
// casPV::maxSimultAsyncOps()
//
unsigned casPV::maxSimultAsyncOps() const
{
	return 1u;
}

//
// casPV::interestRegister()
//
caStatus casPV::interestRegister()
{
	return S_casApp_success;
}

//
// casPV::interestDelete()
//
void casPV::interestDelete()
{
}

//
// casPV::beginTransaction()
//
caStatus casPV::beginTransaction() 
{
	return S_casApp_success;
}

//
// casPV::endTransaction()
//
void casPV::endTransaction()
{
}

//
// casPV::read()
//
caStatus casPV::read(const casCtx &, gdd &)
{
	return S_casApp_noSupport;
}

//
// casPV::write()
//
caStatus casPV::write(const casCtx &, gdd &)
{
	return S_casApp_noSupport;
}

//
// casPV::bestExternalType()
//
aitEnum casPV::bestExternalType() 
{
	return aitEnumString;
}

//
// casPV::createChannel()
//
casChannel *casPV::createChannel (const casCtx &ctx, const char * const,
		const char * const)
{
        return new casChannel (ctx);
}

//
// casPV::destroy()
//
void casPV::destroy()
{
	delete this;
}

//
// Server tool calls this function to post a PV event.
//
void casPV::postEvent (const casEventMask &select, gdd &event)
{
	this->casPVI::postEvent (select, event);
}

//
// Find the server associated with this PV
// ****WARNING****
// this returns NULL if the PV isnt currently installed
// into a server (this situation will exist only if
// the pv isnt deleted in a derived classes replacement
// for virtual casPV::destroy()
// ***************
//
caServer *casPV::getCAS()
{
	return this->casPVI::getExtServer();
}

