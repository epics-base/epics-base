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
 * Revision 1.10  1998/06/16 02:31:36  jhill
 * allow the PV to be created before the server
 *
 * Revision 1.9  1997/08/05 00:47:11  jhill
 * fixed warnings
 *
 * Revision 1.8  1997/04/10 19:34:15  jhill
 * API changes
 *
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
casPVI::casPVI (casPV &pvAdapterIn) : 
	pCAS(NULL), // initially there is no server attachment
	pv(pvAdapterIn),
	nMonAttached(0u),
	nIOAttached(0u),
	destroyInProgress(FALSE)
{
	//
	// this will always be true
	//
	assert (&this->pv!=NULL);
}


//
// casPVI::~casPVI()
//
casPVI::~casPVI()
{
	//
	// only relevant if we are attached to a server
	//
	if (this->pCAS!=NULL) {

		this->lock();

		this->pCAS->removeItem(*this);

		assert (!this->destroyInProgress);
		this->destroyInProgress = TRUE;

		//
		// delete any attached channels
		//
		tsDLIterBD<casPVListChan> iter(this->chanList.first());
		while (iter!=tsDLIterBD<casPVListChan>::eol()) {
			//
			// deleting the channel removes it from the list
			//

			tsDLIterBD<casPVListChan> tmp = iter;
			++tmp;
			(*iter)->destroy();
			iter = tmp;
		}

		this->unlock();
	}

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
// casPVI::attachToServer ()
// 
caStatus casPVI::attachToServer (caServerI &cas)
{

	if (this->pCAS) {
		//
		// currently we enforce that the PV can be attached to only
		// one server at a time
		//
		if (this->pCAS != &cas) {
			return S_cas_pvAlreadyAttached;
		}
	}
	else {
		//
		// install the PV into the server
		//
		cas.installItem (*this);
		this->pCAS = &cas;
	}
	return S_cas_success;
}


//
// casPVI::show()
//
void casPVI::show(unsigned level) const
{
	if (level>1u) {
		printf ("CA Server PV: nChanAttached=%u nMonAttached=%u nIOAttached=%u\n",
			this->chanList.count(), this->nMonAttached, this->nIOAttached);
	}
	(*this)->show(level);
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
	if (this->pCAS) {
		return this->pCAS->getAdapter();;
	}
	else {
		return NULL;
	}
}

//
// casPVI::destroy()
//
// call the destroy in the server tool
//
void casPVI::destroy()
{
	//
	// Flag that a server is no longer using this PV
	//
	this->pCAS = NULL;
	this->pv.destroy();
}

// casPVI::resourceType()
//
epicsShareFunc casResType casPVI::resourceType() const
{
	return casPVT;
}

