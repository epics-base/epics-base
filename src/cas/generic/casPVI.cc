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

#include "gddAppTable.h" // EPICS application type table
#include "dbMapper.h" // EPICS application type table

#include "server.h"
#include "casPVIIL.h" // caServerI inline func

casRes::casRes () {}

//
// casPVI::casPVI()
//
casPVI::casPVI () : 
	pCAS (NULL), // initially there is no server attachment
	nMonAttached (0u),
	nIOAttached (0u),
    destroyInProgress (false)
{
}

//
// casPVI::~casPVI()
//
casPVI::~casPVI()
{
    this->destroyInProgress = true;

	//
	// only relevant if we are attached to a server
	//
	if (this->pCAS!=NULL) {

		this->lock();

		this->pCAS->removeItem(*this);

		//
		// delete any attached channels
		//
		tsDLIter <casPVListChan> iter = this->chanList.firstIter ();
		while ( iter.valid () ) {
			tsDLIter<casPVListChan> tmp = iter;
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
// casPVI::deleteSignal()
// check for none attached and delete self if so
//
void casPVI::deleteSignal ()
{
	caServerI *pLocalCAS = this->pCAS;

	//
	// if we are not attached to a server then the
	// following steps are not relevant
	//
	if (pLocalCAS) {
		//
		// We dont take the PV lock here because
		// the PV may be destroyed and we must
		// keep the lock unlock pairs consistent
		// (because the PV's lock is really a ref
		// to the server's lock)
		//
		// This is safe to do because we take the PV
		// lock when we add a new channel (and the
		// PV lock is realy the server's lock)
		//
		pLocalCAS->lock();

		if (this->chanList.count()==0u) {
            pLocalCAS->removeItem (*this);
            this->pCAS = NULL;
			this->destroy ();
			//
			// !! dont access self after destroy !!
			//
		}

		pLocalCAS->unlock();
	}
}

//
// casPVI::destroy()
//
// This version of destroy() is provided only because it can
// be safely called in the casPVI destructor as a side effect
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
        errMessage (S_cas_noMemory, 
"unable to read application type \"enums\" string conversion table for enumerated PV");
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
		errMessage (status, 
" sorry, no support in server library for asynchronous completion of \"enums\" string conversion table for enumerated PV");
		errMessage (status, 
" please fetch \"enums\" string conversion table into cache during asychronous PV attach IO completion");
        return;
	}
    else if (status) {
        pTmp->unreference ();
        errMessage (status, 
"unable to read application type \"enums\" string conversion table for enumerated PV");
        return;
	}

    if (pTmp->isContainer()) {
        errMessage (S_cas_badType, 
"application type \"enums\" string conversion table for enumerated PV was a container (expected vector of strings)");
        pTmp->unreference ();
        return;
    }
    
    if (pTmp->dimension()==0) {
        if (pTmp->primitiveType()==aitEnumString) {
            aitString *pStr = (aitString *) pTmp->dataVoid ();
            if ( ! this->enumStrTbl.setString ( 0, pStr->string() ) ) {
                errMessage ( S_cas_noMemory, "no memory to set enumerated PV string cache" );
            }
        }
        else if (pTmp->primitiveType()==aitEnumFixedString) {
            aitFixedString *pStr = (aitFixedString *) pTmp->dataVoid ();
            if ( ! this->enumStrTbl.setString ( 0, pStr->fixed_string ) ) {
                errMessage ( S_cas_noMemory, "no memory to set enumerated PV string cache" );
            }
        }
        else {
            errMessage (S_cas_badType, 
"application type \"enums\" string conversion table for enumerated PV isnt a string type?");
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
        this->enumStrTbl.reserve ( count );

        if (pTmp->primitiveType()==aitEnumString) {
            aitString *pStr = (aitString *) pTmp->dataVoid ();
            for ( index = 0; index<count; index++ ) {
                if ( ! this->enumStrTbl.setString ( index, pStr[index].string() ) ) {
                    errMessage ( S_cas_noMemory, "no memory to set enumerated PV string cache" );
                }
            }
        }
        else if (pTmp->primitiveType()==aitEnumFixedString) {
            aitFixedString *pStr = (aitFixedString *) pTmp->dataVoid ();
            for ( index = 0; index<count; index++ ) {
                if ( ! this->enumStrTbl.setString ( index, pStr[index].fixed_string ) ) {
                    errMessage ( S_cas_noMemory, "no memory to set enumerated PV string cache" );
                }
            }
        }
        else {
            errMessage (S_cas_badType, 
"application type \"enums\" string conversion table for enumerated PV isnt a string type?");
        }
    }
    else {
        errMessage (S_cas_badType, 
"application type \"enums\" string conversion table for enumerated PV was multi-dimensional (expected vector of strings)");
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
	if ( this->nMonAttached==0u && !this->destroyInProgress ) {
        this->interestDelete();
	}
	this->unlock();
}

//
// casPVI::getExtServer()
// (not inline because details of caServerI must not
// leak into server tool)
//
caServer *casPVI::getExtServer() const // X aCC 361
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


