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
epicsShareFunc casPV *casChannel::getPV() // X aCC 361
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

