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

#include "gddAppTable.h" // EPICS application type table
#include "dbMapper.h" // EPICS application type table

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
        // update only when attaching to the server so
        // this does not change while clients are using it
        //
        this->updateEnumStringTable ();

		//
		// install the PV into the server
		//
		cas.installItem (*this);
		this->pCAS = &cas;
	}
	return S_cas_success;
}

//
// casPVI::updateEnumStringTable ()
//
// fetch string conversion table so that we can perform proper conversion
// of enumerated PVs to strings during reads
//
// what a API complexity nightmare this GDD is
//
void casPVI::updateEnumStringTable ()
{
    static const aitUint32 stringTableTypeStaticInit = 0xffffffff;
    static aitUint32 stringTableType = stringTableTypeStaticInit;
    unsigned nativeType;
    caStatus status;
    gdd *pTmp;

    //
    // reload the enum string table each time that the
    // PV is attached to the server
    //
    this->enumStrTbl.clear ();

    //
    // fetch the native type
    //
	status = this->bestDBRType (nativeType);
	if (status) {
		errMessage(status, "best external dbr type fetch failed");
        return;
	}

    //
    // empty string table for non-enumerated PVs
    //
    if (nativeType!=aitEnumEnum16) {
        this->enumStrTbl.clear ();
        return;
    }
    
    //
    // lazy init
    //
    if (stringTableType==stringTableTypeStaticInit) {
        stringTableType = gddApplicationTypeTable::app_table.registerApplicationType ("enums");
    }
    
    //
    // create a gdd with the "enum string table" application type
    //
    pTmp = new gddScalar (stringTableType);
    if (pTmp==NULL) {
        errMessage (S_cas_noMemory, "unable to read application type \"enums\" string conversion table for enumerated PV");
        return;
    }

    //
    // create a false context which is guaranteed to cause
    // any asynch IO to be ignored
    //
    casCtx ctx;

    //
    // read the enum string table
    //
    status = this->read (ctx, *pTmp);
	if (status == S_casApp_asyncCompletion || status == S_casApp_postponeAsyncIO) {
        pTmp->unreference ();
		errMessage (status, " sorry, no support in server library for asynchronous completion of \"enums\" string conversion table for enumerated PV");
		errMessage (status, " please fetch \"enums\" string conversion table into cache during asychronous PV attach IO completion");
        return;
	}
    else if (status) {
        pTmp->unreference ();
        errMessage (status, "unable to read application type \"enums\" string conversion table for enumerated PV");
        return;
	}

    if (pTmp->isContainer()) {
        errMessage (S_cas_badType, "application type \"enums\" string conversion table for enumerated PV was a container (expected vector of strings)");
        pTmp->unreference ();
        return;
    }
    
    if (pTmp->dimension()==0) {
        if (pTmp->primitiveType()==aitEnumString) {
            aitString *pStr = (aitString *) pTmp->dataVoid ();
            this->enumStrTbl[0].assign (pStr->string());
        }
        else if (pTmp->primitiveType()==aitEnumFixedString) {
            aitFixedString *pStr = (aitFixedString *) pTmp->dataVoid ();
            this->enumStrTbl[0].assign (pStr->fixed_string);
        }
        else {
            errMessage (S_cas_badType, "application type \"enums\" string conversion table for enumerated PV isnt a string type?");
        }
    }
    else if (pTmp->dimension()==1) {
        gddStatus gdd_status;
        aitIndex index, first, count;
        
        gdd_status = pTmp->getBound (0, first, count);
        assert (gdd_status == 0);

        //
        // preallocate the correct amount
        //
        this->enumStrTbl.reserve (count);

        if (pTmp->primitiveType()==aitEnumString) {
            aitString *pStr = (aitString *) pTmp->dataVoid ();
            for (index = 0; index<count; index++) {
                this->enumStrTbl[index].assign (pStr[index].string());
            }
        }
        else if (pTmp->primitiveType()==aitEnumFixedString) {
            aitFixedString *pStr = (aitFixedString *) pTmp->dataVoid ();
            for (index = 0; index<count; index++) {
                this->enumStrTbl[index].assign (pStr[index].fixed_string);
            }
        }
        else {
            errMessage (S_cas_badType, "application type \"enums\" string conversion table for enumerated PV isnt a string type?");
        }
    }
    else {
        errMessage (S_cas_badType, "application type \"enums\" string conversion table for enumerated PV was multi-dimensional (expected vector of strings)");
    }

    pTmp->unreference ();

    return;
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


