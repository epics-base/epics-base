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
#include "gddApps.h"
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
casPVI::~casPVI ()
{
    this->destroyInProgress = true;

	//
	// only relevant if we are attached to a server
	//
	if ( this->pCAS != NULL ) {

        epicsGuard < caServerI > guard ( * this->pCAS );

		this->pCAS->removeItem ( *this );

		//
		// delete any attached channels
		//
		tsDLIter < casPVListChan > iter = this->chanList.firstIter ();
		while ( iter.valid () ) {
			tsDLIter < casPVListChan > tmp = iter;
			++tmp;
			iter->destroyClientNotify ();
			iter = tmp;
		}
	}

	//
	// all outstanding IO should have been deleted
	// when we destroyed the channels
	//
	casVerify ( this->nIOAttached == 0u );

	//
	// all monitors should have been deleted
	// when we destroyed the channels
	//
	casVerify ( this->nMonAttached == 0u );
}

//
// casPVI::deleteSignal()
// check for none attached and delete self if so
//
void casPVI::deleteSignal ()
{
	//
	// if we are not attached to a server then the
	// following steps are not relevant
	//
	if ( this->pCAS ) {
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
        epicsGuard < caServerI > guard ( * this->pCAS );

		if ( this->chanList.count() == 0u ) {
            this->pCAS->removeItem ( *this );
            this->pCAS = NULL;
            // refresh the table whenever the server reattaches to the PV
            this->enumStrTbl.clear ();
			this->destroy ();
			//
			// !! dont access self after destroy !!
			//
		}
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
caStatus casPVI::attachToServer ( caServerI & cas )
{
	if ( this->pCAS ) {
		//
		// currently we enforce that the PV can be attached to only
		// one server at a time
		//
		if ( this->pCAS != & cas ) {
			return S_cas_pvAlreadyAttached;
		}
	}
	else {
		//
		// install the PV into the server
		//
		cas.installItem ( *this );
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
caStatus casPVI::updateEnumStringTable ( casCtx & ctx )
{
    //
    // keep trying to fill in the table if client disconnects 
    // prevented previous asynchronous IO from finishing, but if
    // a previous client has succeeded then dont bother.
    //
    if ( this->enumStrTbl.numberOfStrings () > 0 ) {
        return S_cas_success;
    }
    
    //
    // create a gdd with the "enum string table" application type
    //
    //	gddArray(int app, aitEnum prim, int dimen, ...);
    gdd *pTmp = new gddScalar ( gddAppType_enums );
    if ( pTmp == NULL ) {
        errMessage ( S_cas_noMemory, 
            "unable to read application type \"enums\" string"
            " conversion table for enumerated PV" );
        return S_cas_noMemory;
    }

    bool success = convertContainerMemberToAtomic ( *pTmp, 
         gddAppType_enums, MAX_ENUM_STATES );
    if ( ! success ) {
        pTmp->unreference ();
        errMessage ( S_cas_noMemory, 
            "unable to read application type \"enums\" string"
            " conversion table for enumerated PV");
        return S_cas_noMemory;
    }

    //
    // read the enum string table
    //
    caStatus status = this->read ( ctx, *pTmp );
	if ( status == S_casApp_asyncCompletion ) {
        return status;
    }
    else if ( status == S_casApp_postponeAsyncIO ) {
        pTmp->unreference ();
        return status;
	}
    else if ( status ) {
        pTmp->unreference ();
        errMessage ( status, 
            "- unable to read application type \"enums\" string"
            " conversion table for enumerated PV");
        return status;
	}

    updateEnumStringTableAsyncCompletion ( *pTmp );
    pTmp->unreference ();

    return status;
}

void casPVI::updateEnumStringTableAsyncCompletion ( const gdd & resp )
{
    //
    // keep trying to fill in the table if client disconnects 
    // prevented previous asynchronous IO from finishing, but if
    // a previous client has succeeded then dont bother.
    //
    if ( this->enumStrTbl.numberOfStrings () > 0 ) {
        return;
    }
    
    if ( resp.isContainer() ) {
        errMessage ( S_cas_badType, 
            "application type \"enums\" string conversion table for"
            " enumerated PV was a container (expected vector of strings)" );
        return;
    }
    
    if  ( resp.dimension() == 0 ) {
        if ( resp.primitiveType() == aitEnumString ) {
            aitString *pStr = (aitString *) resp.dataVoid ();
            if ( ! this->enumStrTbl.setString ( 0, pStr->string() ) ) {
                errMessage ( S_cas_noMemory, 
                    "no memory to set enumerated PV string cache" );
            }
        }
        else if ( resp.primitiveType() == aitEnumFixedString ) {
            aitFixedString *pStr = (aitFixedString *) resp.dataVoid ();
            if ( ! this->enumStrTbl.setString ( 0, pStr->fixed_string ) ) {
                errMessage ( S_cas_noMemory, 
                    "no memory to set enumerated PV string cache" );
            }
        }
        else {
            errMessage ( S_cas_badType, 
                "application type \"enums\" string conversion"
                " table for enumerated PV isnt a string type?" );
        }
    }
    else if ( resp.dimension() == 1 ) {
        gddStatus gdd_status;
        aitIndex index, first, count;
        
        gdd_status = resp.getBound ( 0, first, count );
        assert ( gdd_status == 0 );

        //
        // preallocate the correct amount
        //
        this->enumStrTbl.reserve ( count );

        if ( resp.primitiveType() == aitEnumString ) {
            aitString *pStr = (aitString *) resp.dataVoid ();
            for ( index = 0; index<count; index++ ) {
                if ( ! this->enumStrTbl.setString ( index, pStr[index].string() ) ) {
                    errMessage ( S_cas_noMemory, 
                        "no memory to set enumerated PV string cache" );
                }
            }
        }
        else if ( resp.primitiveType() == aitEnumFixedString ) {
            aitFixedString *pStr = (aitFixedString *) resp.dataVoid ();
            for ( index = 0; index<count; index++ ) {
                if ( ! this->enumStrTbl.setString ( index, pStr[index].fixed_string ) ) {
                    errMessage ( S_cas_noMemory, 
                        "no memory to set enumerated PV string cache" );
                }
            }
        }
        else {
            errMessage ( S_cas_badType, 
                "application type \"enums\" string conversion"
                " table for enumerated PV isnt a string type?" );
        }
    }
    else {
        errMessage ( S_cas_badType, 
            "application type \"enums\" string conversion table"
            " for enumerated PV was multi-dimensional"
            " (expected vector of strings)" );
    }
}

//
// casPVI::registerEvent()
//
caStatus casPVI::registerEvent ()
{
	caStatus status;

    epicsGuard < caServerI > guard ( * this->pCAS );
	this->nMonAttached++;
	if ( this->nMonAttached == 1u ) {
		status = this->interestRegister ();
	}
	else {
		status = S_cas_success;
	}

	return status;
}

//
// casPVI::unregisterEvent()
//
void casPVI::unregisterEvent()
{
    epicsGuard < caServerI > guard ( * this->pCAS );
	this->nMonAttached--;
	//
	// Dont call casPV::interestDelete() when we are in 
	// casPVI::~casPVI() (and casPV no longr exists)
	//
	if ( this->nMonAttached == 0u && !this->destroyInProgress ) {
        this->interestDelete();
	}
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


