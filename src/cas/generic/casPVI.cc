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
#include <casPVIIL.h> // casPVI inline func
#include <caServerIIL.h> // caServerI inline func


//
// casPVI::casPVI()
//
casPVI::casPVI(caServerI &casIn, const char * const pNameIn, 
		casPV &pvAdapterIn) : 
	cas(casIn), 
	stringId(pNameIn),
	nMonAttached(0u),
	nIOAttached(0u)
{
	assert(&cas);
	//
	// casPVI must always be a base of casPV
	//
	assert(&pvAdapterIn == (casPV *)this);
	this->cas.installPV(*this);
}


//
// casPVI::~casPVI()
//
casPVI::~casPVI()
{
	casPVListChan		*pChan;
	casPVListChan		*pNextChan;
	tsDLIter<casPVListChan>	iter(this->chanList);

	this->lock();

	//
	// delete any attached channels
	//
	pChan = iter();
	while (pChan) {
		//
		// deleting the channel removes it from the list
		//
		pNextChan = iter();
		(*pChan)->destroy();
		pChan = pNextChan;
	}

	this->cas.removePV(*this);

	casVerify (this->nIOAttached==0u);

	this->unlock();
}

//
// caPVI::verifyPVName()
//
caStatus casPVI::verifyPVName(gdd &name)
{
	int		gddStatus;
 
        //
        // verify up front that they have supplied
        // a valid name (so that we wont fail
        // in the PV constructor)
        //
        gddStatus = name.Reference();
        if (gddStatus) {
                serverToolDebug();
                return S_cas_badPVName;
        }
        gddStatus = name.Unreference();
        if (gddStatus) {
                serverToolDebug();
                return S_cas_badPVName;
        }
 
	if (name.PrimitiveType() != aitEnumString) {
                serverToolDebug();
                return S_cas_badPVName;
	}

        if (name.Dimension() != 1u) {
                serverToolDebug();
                return S_cas_badPVName;
        }

	return S_cas_success;
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

