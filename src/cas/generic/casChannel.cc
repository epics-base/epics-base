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
 *
 */


#include <server.h>
#include <casChannelIIL.h> // casChannelI inline func
#include <casPVListChanIL.h> // casPVListChan inline func

//
// casChannel::casChannel()
//
casChannel::casChannel(const casCtx &ctx) : 
	casPVListChan (ctx, *this) 
{
}

//
// casChannel::~casChannel()
//
casChannel::~casChannel() 
{
}

casPV *casChannel::getPV()
{
	casPVI &pvi((*this)->getPVI());

	if (&pvi!=NULL) {
		return pvi.intefaceObjectPointer();
	}
	else {
		return NULL;
	}
}

//
// casChannel::postEvent()
//
void casChannel::postEvent (const casEventMask &select, gdd &event)
{
	(*this)->postEvent(select, event);
}

//
// casChannel::setOwner()
//
void casChannel::setOwner(const char * const /* pUserName */, 
	const char * const /* pHostName */)
{
	//
	// NOOP
	//
}

//
// casChannel::interestRegister()
//
caStatus casChannel::interestRegister()
{
	return S_casApp_success;
}

//
// casChannel::interestDelete()
//
void casChannel::interestDelete()
{
}

//
// casChannel::readAccess()
//
aitBool casChannel::readAccess () const 
{
	return aitTrue;
}

//
// casChannel::writeAccess()
//
aitBool casChannel::writeAccess() const 
{
	return aitTrue;
}


//
// casChannel::confirmationRequested()
//
aitBool casChannel::confirmationRequested() const 
{
	return aitFalse;
}

//
// casChannel::show()
//
void casChannel::show(unsigned level) 
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
void casChannel::destroy()
{
	delete this;
}


