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
 */


#include "server.h"
#include "casPVIIL.h" // casPVI inline func
#include "caServerIIL.h" // caServerI inline func

//
// casPVI::casPVI()
//
casPVI::casPVI () : 
	pCAS (NULL), // initially there is no server attachment
	nMonAttached (0u),
	nIOAttached (0u)
{
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
			iter->destroyClientNotify ();
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
// casPVI::destroy()
//
// This version of destroy() is provided only because it can
// be safely calle din the casPVI destructor as a side effect
// of deleting the last channel.
//
void casPVI::destroy ()
{
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
// casPVI::registerEvent()
//
caStatus casPVI::registerEvent ()
{
	caStatus status;

	this->lock();
	this->nMonAttached++;
	if (this->nMonAttached==1u) {
		status = this->interestRegister ();
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
	if (this->nMonAttached==0u) {
        this->interestDelete();
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
		return this->pCAS->getAdapter();
	}
	else {
		return NULL;
	}
}

//
// casPVI::show()
//
epicsShareFunc void casPVI::show (unsigned level)  const
{
	if (level>1u) {
		printf ("CA Server PV: nChanAttached=%u nMonAttached=%u nIOAttached=%u\n",
			this->chanList.count(), this->nMonAttached, this->nIOAttached);
	}
	if (level>2u) {
		printf ("\tBest external type = %d\n", this->bestExternalType());
	}
}

//
// casPVI::resourceType()
//
epicsShareFunc casResType casPVI::resourceType() const
{
	return casPVT;
}

//
// casPVI::apiPointer()
// retuns NULL if casPVI isnt a base of casPV
//
epicsShareFunc casPV *casPVI::apiPointer ()
{
    return NULL;
}


