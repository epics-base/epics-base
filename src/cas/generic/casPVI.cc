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
 * Revision 1.7  1996/12/12 18:55:38  jhill
 * fixed client initiated pv delete calls interestDelete() VF bug
 *
 * Revision 1.6  1996/12/06 22:35:06  jhill
 * added destroyInProgress flag
 *
 * Revision 1.5  1996/11/02 00:54:22  jhill
 * many improvements
 *
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
casPVI::casPVI(caServer &casIn, casPV &pvAdapterIn) : 
	cas(*casIn.pCAS), 
	pv(pvAdapterIn),
	nMonAttached(0u),
	nIOAttached(0u),
	destroyInProgress(FALSE)
{
	//
	// we would like to throw an exception for this one
	// (but this is not portable)
	//
	assert(&this->cas);

	//
	// this will always be true
	//
	assert(&this->pv);

	this->cas.installItem(*this);
}


//
// casPVI::~casPVI()
//
casPVI::~casPVI()
{
	this->lock();

	this->cas.removeItem(*this);

	assert(!this->destroyInProgress);
	this->destroyInProgress = TRUE;

	//
	// delete any attached channels
	//
	tsDLIterBD<casPVListChan> iter(this->chanList.first());
	const tsDLIterBD<casPVListChan> eol;
	tsDLIterBD<casPVListChan> tmp;
	while (iter!=eol) {
		//
		// deleting the channel removes it from the list
		//

		tmp = iter;
		++tmp;
		(*iter)->destroy();
		iter = tmp;
	}

	this->unlock();

	//
	// all outstanding IO should have been deleted
	// when we destroyed the channels
	//
	casVerify (this->nIOAttached==0u);

	//
	// all monitors should have been deleted
	// when we destroyed the channels
	//
	casVerify (this->nMonAttached==0u);
}


//
// casPVI::show()
//
void casPVI::show(unsigned level) const
{
	this->lock();

	if (level>1u) {
		printf ("CA Server PV: nChanAttached=%u nMonAttached=%u nIOAttached=%u\n",
			this->chanList.count(), this->nMonAttached, this->nIOAttached);
	}
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
	this->lock();
	this->nMonAttached--;
	//
	// Dont call casPV::interestDelete() when we are in 
	// casPVI::~casPVI() (and casPV no longr exists)
	//
	if (this->nMonAttached==0u && !this->destroyInProgress) {
		(*this)->interestDelete();
	}
	this->unlock();
}

//
// casPVI::getExtServer()
// (not inline because details of caServerI must not
// leak into server tool)
//
caServer *casPVI::getExtServer() const
{
	return this->cas.getAdapter();
}

//
// casPVI::destroy()
//
// call the destroy in the server tool
//
void casPVI::destroy()
{
	this->pv.destroy();
}

// casPVI::resourceType()
//
casResType casPVI::resourceType() const
{
	return casPVT;
}

