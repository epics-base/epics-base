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
 * Revision 1.7  1997/08/05 00:47:10  jhill
 * fixed warnings
 *
 * Revision 1.6  1997/04/10 19:34:14  jhill
 * API changes
 *
 * Revision 1.5  1996/12/06 22:34:22  jhill
 * add maxDimension() and maxBound()
 *
 * Revision 1.4  1996/11/02 00:54:21  jhill
 * many improvements
 *
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

#include "server.h"
#include "casPVIIL.h" 	// casPVI inline func
#include "casCtxIL.h" 	// casCtx inline func

epicsShareFunc casPV::casPV () :
	casPVI (*this)
{
}

epicsShareFunc casPV::~casPV()
{
}

//
// casPV::show()
//
epicsShareFunc void casPV::show(unsigned level)  const
{
	if (level>2u) {
		printf ("casPV: Best external type = %d\n",
			this->bestExternalType());
	}
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
// (base returns zero - scaler)
//
epicsShareFunc unsigned casPV::maxDimension() const
{
	return 0u;
}

//
// casPV::maxBound()
// (base returns scaler bound independent of the dimension arg)
//
epicsShareFunc aitIndex casPV::maxBound(unsigned /* dimension */) const
{
	return 1u;
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
// casPV::destroy()
//
epicsShareFunc void casPV::destroy()
{
	delete this;
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
// into a server (this situation will exist only if
// the pv isnt deleted in a derived classes replacement
// for virtual casPV::destroy()
// ***************
//
epicsShareFunc caServer *casPV::getCAS() const
{
	return this->casPVI::getExtServer();
}

