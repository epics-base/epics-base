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

#include "epicsGuard.h"
#include "gddAppTable.h" // EPICS application type table
#include "gddApps.h"
#include "dbMapper.h" // EPICS application type table

#define epicsExportSharedSymbols
#include "caServerDefs.h"
#include "caServerI.h"
#include "casPVI.h"
#include "chanIntfForPV.h"
#include "casAsyncIOI.h"
#include "casMonitor.h"

casPVI::casPVI ( casPV & intf ) : 
	pCAS ( NULL ), pv ( intf ), 
    nMonAttached ( 0u ), nIOAttached ( 0u ) {}

casPVI::~casPVI ()
{
	//
	// all channels should have been destroyed 
    // (otherwise the server tool is yanking the 
    // PV out from under the server)
	//
	casVerify ( this->chanList.count() == 0u );

	//
	// all outstanding IO should have been deleted
	// when we destroyed the channels
	//
	casVerify ( this->nIOAttached == 0u );
    if (this->nIOAttached) {
        errlogPrintf ( "The number of IO objected supposedly attached is %u\n", this->nIOAttached );
    }

	//
	// all monitors should have been deleted
	// when we destroyed the channels
	//
	casVerify ( this->nMonAttached == 0u );

    this->pv.pPVI = 0;
    this->pv.destroy ();
}

//
// check for none attached and delete self if so
//
// this call must be protected by the server's lock
// ( which also protects channel creation)
//
void casPVI::deleteSignal ()
{
    bool destroyNeeded = false;
    {
        epicsGuard < epicsMutex > guard ( this->mutex );

	    //
	    // if we are not attached to a server then the
	    // following steps are not relevant
	    //
	    if ( this->pCAS ) {
		    if ( this->chanList.count() == 0u ) {
                this->pCAS = NULL;
                // refresh the table whenever the server reattaches to the PV
                this->enumStrTbl.clear ();
                destroyNeeded = true;
		    }
	    }
    }

    if ( destroyNeeded ) {
		delete this;
    }

	// !! dont access self after potential delete above !!
}

casPVI * casPVI::attachPV ( casPV & pv ) 
{
    if ( ! pv.pPVI ) {
        pv.pPVI = new ( std::nothrow ) casPVI ( pv );
    }
    return pv.pPVI;
}

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
		this->pCAS = & cas;
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
    epicsGuard < epicsMutex > guard ( this->mutex );

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

void casPVI::postEvent ( const casEventMask & select, const gdd & event )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
	if ( this->nMonAttached ) {
        // we are paying some significant locking overhead for
        // these diagnostic counters
        this->pCAS->updateEventsPostedCounter ( this->nMonAttached );
	    tsDLIter < chanIntfForPV > iter = this->chanList.firstIter ();
        while ( iter.valid () ) {
		    iter->postEvent ( select, event );
		    ++iter;
	    }
	}
}

caStatus casPVI::installMonitor ( 
    casMonitor & mon, tsDLList < casMonitor > & monitorList )
{
    bool newInterest = false;
    {
        epicsGuard < epicsMutex > guard ( this->mutex );
        assert ( this->nMonAttached < UINT_MAX );
	    this->nMonAttached++;
	    if ( this->nMonAttached == 1u ) {
            newInterest = true;
	    }
        // use pv lock to protect channel's monitor list
	    monitorList.add ( mon );
    }
    if ( newInterest ) {
		return this->pv.interestRegister ();
    }
    else {
		return S_cas_success;
    }
}

casMonitor * casPVI::removeMonitor ( 
    tsDLList < casMonitor > & list, ca_uint32_t clientIdIn )
{
    casMonitor * pMon = 0;
    bool noInterest = false;
    {
	    //
	    // (it is reasonable to do a linear search here because
	    // sane clients will require only one or two monitors 
	    // per channel)
	    //
        epicsGuard < epicsMutex > guard ( this->mutex );
	    tsDLIter < casMonitor > iter = list.firstIter ();
        while ( iter.valid () ) {
		    if ( iter->matchingClientId ( clientIdIn ) ) {
                list.remove ( *iter.pointer () );
                assert ( this->nMonAttached > 0 );
	            this->nMonAttached--;
                noInterest = 
                    ( this->nMonAttached == 0u );
                pMon = iter.pointer ();
                break;
		    }
		    iter++;
	    }
    }
    if ( noInterest ) {
        this->pv.interestDelete ();
    }
    return pMon;
}

caServer *casPVI::getExtServer () const // X aCC 361
{
	if ( this->pCAS ) {
		return this->pCAS->getAdapter ();
	}
	else {
		return NULL;
	}
}

void casPVI::show ( unsigned level )  const
{
    epicsGuard < epicsMutex > guard ( this->mutex );
	printf ( "CA Server PV: nChanAttached=%u nMonAttached=%u nIOAttached=%u\n",
		this->chanList.count(), this->nMonAttached, this->nIOAttached );
	if ( level >= 1u ) {
		printf ( "\tBest external type = %d\n", this->bestExternalType() );
	}
	if ( level >= 2u ) {
        this->pv.show ( level - 2u );
	}
}

casPV * casPVI::apiPointer ()
{
    return & this->pv;
}

void casPVI::installChannel ( chanIntfForPV & chan )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
	this->chanList.add ( chan );
}
 
void casPVI::removeChannel ( 
    chanIntfForPV & chan, tsDLList < casMonitor > & src, 
    tsDLList < casMonitor > & dest )
{
    bool noInterest = false;
    {
        epicsGuard < epicsMutex > guard ( this->mutex );
        src.removeAll ( dest );
        if ( dest.count() ) {
            assert ( this->nMonAttached >= dest.count() );
            this->nMonAttached -= dest.count ();
            noInterest = ( this->nMonAttached == 0u );
        }
	    this->chanList.remove ( chan );
    }
    if ( noInterest ) {
        this->pv.interestDelete ();
    }
}

void casPVI::clearOutstandingReads ( tsDLList < casAsyncIOI > & ioList )
{
    epicsGuard < epicsMutex > guard ( this->mutex );

    // cancel any pending asynchronous IO 
	tsDLIter < casAsyncIOI > iterIO = 
        ioList.firstIter ();
	while ( iterIO.valid () ) {
		tsDLIter < casAsyncIOI > tmp = iterIO;
		++tmp;
        if ( iterIO->oneShotReadOP () ) {
            ioList.remove ( *iterIO );
            assert ( this->nIOAttached != 0 );
            this->nIOAttached--;
        }
		delete iterIO.pointer ();
		iterIO = tmp;
	}
}

void casPVI::destroyAllIO ( tsDLList < casAsyncIOI > & ioList )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
	while ( casAsyncIOI * pIO = ioList.get() ) {
        pIO->removeFromEventQueue ();
		delete pIO;
        assert ( this->nIOAttached != 0 );
        this->nIOAttached--;
	}
}

void casPVI::installIO ( 
    tsDLList < casAsyncIOI > & ioList, casAsyncIOI & io )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    ioList.add ( io );
    assert ( this->nIOAttached != UINT_MAX );
    this->nIOAttached++;
}

void casPVI::uninstallIO ( 
    tsDLList < casAsyncIOI > & ioList, casAsyncIOI & io )
{
    {
        epicsGuard < epicsMutex > guard ( this->mutex );
        ioList.remove ( io );
        assert ( this->nIOAttached != 0 );
        this->nIOAttached--;
    }
	this->ioBlockedList::signal();
}

caStatus  casPVI::bestDBRType ( unsigned & dbrType ) // X aCC 361
{
	aitEnum bestAIT = this->bestExternalType ();
    if ( bestAIT == aitEnumInvalid || bestAIT < 0 ) {
		return S_cas_badType;
    }
    unsigned aitIndex = static_cast < unsigned > ( bestAIT );
	if ( aitIndex >= sizeof ( gddAitToDbr ) / sizeof ( gddAitToDbr[0] ) ) {
		return S_cas_badType;
    }
	dbrType = gddAitToDbr[bestAIT];
	return S_cas_success;
}


