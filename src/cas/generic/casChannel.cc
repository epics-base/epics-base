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

#include "server.h"
#include "casChannelIIL.h" // casChannelI inline func
#include "casPVListChanIL.h" // casPVListChan inline func

//
// casChannel::casChannel()
//
epicsShareFunc casChannel::casChannel(const casCtx &ctx) : 
	casPVListChan (ctx) 
{
}

//
// casChannel::~casChannel()
//
epicsShareFunc casChannel::~casChannel() 
{
}

//
// casChannel::getPV()
//
epicsShareFunc casPV *casChannel::getPV()
{
	casPVI *pPVI = &this->casChannelI::getPVI();

	if (pPVI!=NULL) {
		return pPVI->apiPointer();
	}
	else {
		return NULL;
	}
}

//
// casChannel::setOwner()
//
epicsShareFunc void casChannel::setOwner(const char * const /* pUserName */, 
	const char * const /* pHostName */)
{
	//
	// NOOP
	//
}

//
// casChannel::readAccess()
//
epicsShareFunc bool casChannel::readAccess () const 
{
	return true;
}

//
// casChannel::writeAccess()
//
epicsShareFunc bool casChannel::writeAccess() const 
{
	return true;
}


//
// casChannel::confirmationRequested()
//
epicsShareFunc bool casChannel::confirmationRequested() const 
{
	return false;
}

//
// casChannel::show()
//
epicsShareFunc void casChannel::show(unsigned level) const
{
	if (level>2u) {
		printf("casChannel: read access = %d\n",
			this->readAccess());
		printf("casChannel: write access = %d\n",
			this->writeAccess());
		printf("casChannel: confirmation requested = %d\n",
			this->confirmationRequested());
	}
}

//
// casChannel::destroy()
//
epicsShareFunc void casChannel::destroy()
{
	delete this;
}

//
// casChannel::postAccessRightsEvent()
//
epicsShareFunc void casChannel::postAccessRightsEvent()
{
	this->casChannelI::postAccessRightsEvent();
}

