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

#include <stdexcept>

#include "server.h"
#include "casEventSysIL.h" // casEventSys in line func
#include "casMonEventIL.h" // casMonEvent in line func
#include "casCtxIL.h" // casCtx in line func
#include "casCoreClientIL.h" // casCoreClient in line func

//
// casMonEvent::cbFunc()
//
caStatus casMonEvent::cbFunc ( casCoreClient & client )
{
    caStatus status;

    //
    // ignore this event if it is stale and there is
    // no call back object associated with it
    //
	// safe to cast because we have checked the type code 
	//
	casMonitor * pMon = reinterpret_cast < casMonitor * > 
        ( client.lookupRes ( this->id, casMonitorT ) );
    if ( ! pMon ) {
        // we know this isnt an overflow event because those are
        // removed from the queue when the casMonitor object is
        // destroyed
        client.casMonEventDestroy ( *this );
        status = S_casApp_success;
    }
    else {
        // this object may have been destroyed 
        // here by the executeEvent() call below
        status = pMon->executeEvent ( *this );
    }

    return status;
}

void casMonEvent::eventSysDestroyNotify ( casCoreClient & client )
{
    client.casMonEventDestroy ( *this );
}

//
// casMonEvent::assign ()
//
void casMonEvent::assign (casMonitor &monitor, const smartConstGDDPointer &pValueIn)
{
	this->pValue = pValueIn;
	this->id = monitor.casRes::getId();
}

//
// ~casMonEvent ()
// (this is not in line because it is virtual in the base)
//
casMonEvent::~casMonEvent ()
{
	this->clear();
}

void * casMonEvent::operator new ( size_t size, 
    tsFreeList < class casMonEvent, 1024 > & freeList )
{
    return freeList.allocate ( size );
}

#ifdef CXX_PLACEMENT_DELETE
void casMonEvent::operator delete ( void *pCadaver, 
    tsFreeList < class casMonEvent, 1024 > & freeList ) epicsThrows(())
{
    freeList.release ( pCadaver, sizeof ( casMonEvent ) );
}
#endif

void * casMonEvent::operator new ( size_t ) // X aCC 361
{
    // The HPUX compiler seems to require this even though no code
    // calls it directly
    throw std::logic_error ( "why is the compiler calling private operator new" );
}

void casMonEvent::operator delete ( void * )
{
    // Visual C++ .net appears to require operator delete if
    // placement operator delete is defined? I smell a ms rat
    // because if I declare placement new and delete, but
    // comment out the placement delete definition there are
    // no undefined symbols.
    errlogPrintf ( "%s:%d this compiler is confused about placement delete - memory was probably leaked",
        __FILE__, __LINE__ );
}



