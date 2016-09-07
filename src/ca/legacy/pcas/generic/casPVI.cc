/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#include "epicsGuard.h"
#include "gddAppTable.h" // EPICS application type table
#include "gddApps.h"
#include "dbMapper.h" // EPICS application type table
#include "errlog.h"

#define epicsExportSharedSymbols
#include "caServerDefs.h"
#include "caServerI.h"
#include "casPVI.h"
#include "chanIntfForPV.h"
#include "casAsyncIOI.h"
#include "casMonitor.h"

casPVI::casPVI ( casPV & intf ) : 
	pCAS ( NULL ), pPV ( & intf ), nMonAttached ( 0u ), 
        nIOAttached ( 0u ), deletePending ( false ) {}

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
    if ( this->nIOAttached ) {
        errlogPrintf ( "The number of IO objected attached is %u\n", this->nIOAttached );
    }

	//
	// all monitors should have been deleted
	// when we destroyed the channels
	//
	casVerify ( this->nMonAttached == 0u );

    {
        epicsGuard < epicsMutex > guard ( this->mutex );
        this->deletePending = true;
        if ( this->pPV ) {
            this->pPV->destroyRequest ();
        }
    }
}

void casPVI::casPVDestroyNotify ()
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    this->pPV = 0;
    if ( ! this->deletePending ) {
        // last channel to be destroyed destroys the casPVI
        tsDLIter < chanIntfForPV > iter = this->chanList.firstIter ();
        while ( iter.valid() ) {
            iter->postDestroyEvent ();
            iter++;
        }
    }
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

caStatus casPVI::attachToServer ( caServerI & cas )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
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
caStatus casPVI::updateEnumStringTable ( casCtx & ctxIn )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    
    //
    // create a gdd with the "enum string table" application type
    //
    //	gddArray(int app, aitEnum prim, int dimen, ...);
    gdd * pTmp = new gddScalar ( gddAppType_enums );
    if ( pTmp == NULL ) {
        errMessage ( S_cas_noMemory, 
            "unable to create gdd for read of application type \"enums\" string"
            " conversion table for enumerated PV" );
        return S_cas_noMemory;
    }

    caStatus status = convertContainerMemberToAtomic ( *pTmp, 
         gddAppType_enums, MAX_ENUM_STATES );
    if ( status != S_cas_success ) {
        pTmp->unreference ();
        errMessage ( status, 
            "unable to to config gdd for read of application type \"enums\" string"
            " conversion table for enumerated PV");
        return status;
    }

    //
    // read the enum string table
    //
    status = this->read ( ctxIn, *pTmp );
    if ( status == S_cas_success ) {
        updateEnumStringTableAsyncCompletion ( *pTmp );
	}
    else if ( status != S_casApp_asyncCompletion && 
                status != S_casApp_postponeAsyncIO ) {
        errPrintf ( status, __FILE__, __LINE__,
            "- unable to read application type \"enums\" "
            " (string conversion table) from enumerated native type PV \"%s\"", 
            this->getName() );
    }

    pTmp->unreference ();

    return status;
}

void casPVI::updateEnumStringTableAsyncCompletion ( const gdd & resp )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    
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
            errPrintf ( S_cas_badType, __FILE__, __LINE__,
                "application type \"enums\" string conversion"
                " table for enumerated PV \"%s\" isnt a string type?",
                getName() );
        }
    }
    else if ( resp.dimension() == 1 ) {
        gddStatus gdd_status;
        aitIndex index, first, count;
        
        gdd_status = resp.getBound ( 0, first, count );
        assert ( gdd_status == 0 );

        //
        // clear and preallocate the correct amount
        //
        this->enumStrTbl.clear ();
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
            for ( index = 0; index < count; index++ ) {
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
    // if this is a DBE_PROPERTY event for an enum type
    // update the enum string table
    if ( (select & this->pCAS->propertyEventMask()).eventsSelected() ) {
        const gdd *menu = NULL;
        if ( event.applicationType() == gddAppType_dbr_gr_enum )
            menu = event.getDD( gddAppTypeIndex_dbr_gr_enum_enums );
        else if ( event.applicationType() == gddAppType_dbr_ctrl_enum )
            menu = event.getDD( gddAppTypeIndex_dbr_ctrl_enum_enums );
        if ( menu )
            updateEnumStringTableAsyncCompletion( *menu );
    }

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
    epicsGuard < epicsMutex > guard ( this->mutex );
    assert ( this->nMonAttached < UINT_MAX );
	this->nMonAttached++;
    // use pv lock to protect channel's monitor list
	monitorList.add ( mon );
    if ( this->nMonAttached == 1u && this->pPV ) {
		return this->pPV->interestRegister ();
    }
    else {
		return S_cas_success;
    }
}

casMonitor * casPVI::removeMonitor ( 
    tsDLList < casMonitor > & list, ca_uint32_t clientIdIn )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    casMonitor * pMon = 0;
	//
	// (it is reasonable to do a linear search here because
	// sane clients will require only one or two monitors 
	// per channel)
	//
	tsDLIter < casMonitor > iter = list.firstIter ();
    while ( iter.valid () ) {
		if ( iter->matchingClientId ( clientIdIn ) ) {
            list.remove ( *iter.pointer () );
            assert ( this->nMonAttached > 0 );
	        this->nMonAttached--;
            pMon = iter.pointer ();
            break;
		}
		iter++;
	}
    if ( this->nMonAttached == 0u && this->pPV ) {
        this->pPV->interestDelete ();
    }
    return pMon;
}

caServer *casPVI::getExtServer () const
{
    epicsGuard < epicsMutex > guard ( this->mutex );
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
        this->pPV->show ( level - 2u );
	}
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
    epicsGuard < epicsMutex > guard ( this->mutex );
    src.removeAll ( dest );
    if ( dest.count() ) {
        assert ( this->nMonAttached >= dest.count() );
        this->nMonAttached -= dest.count ();
    }
	this->chanList.remove ( chan );
    if ( this->nMonAttached == 0u && this->pPV ) {
        this->pPV->interestDelete ();
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
		    delete iterIO.pointer ();
            assert ( this->nIOAttached != 0 );
            this->nIOAttached--;
        }
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

caStatus  casPVI::bestDBRType ( unsigned & dbrType )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
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

caStatus casPVI::read ( const casCtx & ctx, gdd & prototype )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    if ( this->pPV ) {
        caStatus status = this->pPV->beginTransaction ();
        if ( status != S_casApp_success ) {
            return status;
        }
        status = this->pPV->read ( ctx, prototype );
        this->pPV->endTransaction ();
        return status;
    }
    else {
        return S_cas_disconnect;
    }
}

caStatus casPVI::write ( const casCtx & ctx, const gdd & value )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    if ( this->pPV ) {
        caStatus status = this->pPV->beginTransaction ();
        if ( status != S_casApp_success ) {
            return status;
        }
        status = this->pPV->write ( ctx, value );
        this->pPV->endTransaction ();
        return status;
    }
    else {
        return S_cas_disconnect;
    }
}

caStatus casPVI::writeNotify ( const casCtx & ctx, const gdd & value )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    if ( this->pPV ) {
        caStatus status = this->pPV->beginTransaction ();
        if ( status != S_casApp_success ) {
            return status;
        }
        status = this->pPV->writeNotify ( ctx, value );
        this->pPV->endTransaction ();
        return status;
    }
    else {
        return S_cas_disconnect;
    }
}

casChannel * casPVI::createChannel ( const casCtx & ctx,
    const char * const pUserName, const char * const pHostName )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    if ( this->pPV ) {
        return this->pPV->createChannel ( ctx, pUserName, pHostName );
    }
    else {
        return 0;
    }
}

aitEnum casPVI::bestExternalType () const
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    if ( this->pPV ) {
        return this->pPV->bestExternalType ();
    }
    else {
        return aitEnumInvalid;
    }
}

// CA only does 1D arrays for now 
aitIndex casPVI::nativeCount () 
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    if ( this->pPV ) {
	    if ( this->pPV->maxDimension() == 0u ) {
		    return 1u; // scalar
	    }
	    return this->pPV->maxBound ( 0u );
    }
    else {
        return S_cas_disconnect;
    }
}

const char * casPVI::getName () const
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    if ( this->pPV ) {
        return this->pPV->getName ();
    }
    else {
        return "<disconnected>";
    }
}

