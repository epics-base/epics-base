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
 * Revision 1.4  1996/09/04 20:23:17  jhill
 * init new member cas and add arg to serverToolDebug()
 *
 * Revision 1.3  1996/07/01 19:56:13  jhill
 * one last update prior to first release
 *
 * Revision 1.2  1996/06/26 21:18:57  jhill
 * now matches gdd api revisions
 *
 * Revision 1.1.1.1  1996/06/20 00:28:15  jhill
 * ca server installation
 *
 *
 */


#include "server.h"
#include "casPVIIL.h" // casPVI inline func
#include "caServerIIL.h" // caServerI inline func


//
// casPVI::casPVI()
//
casPVI::casPVI(caServerI &casIn, const char * const pNameIn, 
		casPV &pvAdapterIn) : 
	stringId(pNameIn),
	cas(casIn), 
	pv(pvAdapterIn),
	nMonAttached(0u),
	nIOAttached(0u)
{
	assert(&this->cas);
	assert(&this->pv);
	this->cas.installPV(*this);
}


//
// casPVI::~casPVI()
//
casPVI::~casPVI()
{
	casPVListChan		*pChan;
	casPVListChan		*pNextChan;

	this->lock();

	//
	// delete any attached channels
	//
	tsDLFwdIter<casPVListChan> iter(this->chanList);
	pChan = iter.next();
	while (pChan) {
		//
		// deleting the channel removes it from the list
		//
		pNextChan = iter.next();
		(*pChan)->destroy();
		pChan = pNextChan;
	}

	this->cas.removePV(*this);

	this->unlock();

	//
	// all outstanding IO should have been deleted
	// when we destroyed the channels
	//
	casVerify (this->nIOAttached==0u);
}


//
// casPVI::show()
//
void casPVI::show(unsigned level)
{
	this->lock();

	printf("\"%s\"\n", this->resourceName());

	(*this)->show(level);

	this->unlock();
}

//
// casPVI::registerEvent()
//
caStatus casPVI::registerEvent ()
{
	caStatus status;

	this->lock();
	this->nMonAttached++;
	if (this->nMonAttached==1u) {
		status = (*this)->interestRegister();
	}
	else {
		status = S_cas_success;
	}
	this->unlock();
	return status;
}

//
// casPVI::unregisterEvent()
//
void casPVI::unregisterEvent()
{
	this->nMonAttached--;
	if (this->nMonAttached==0u) {
		(*this)->interestDelete();
	}
}

//
// casPVI::getExtServer()
// (not inline because details of caServerI must not
// leak into server tool)
//
caServer *casPVI::getExtServer()
{
	return this->cas.getAdapter();
}


