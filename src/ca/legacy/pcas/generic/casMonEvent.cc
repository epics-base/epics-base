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

#include <string>
#include <stdexcept>

#include "errlog.h"

#define epicsExportSharedSymbols
#include "casMonEvent.h"
#include "casMonitor.h"
#include "casCoreClient.h"

caStatus casMonEvent::cbFunc ( 
    casCoreClient & client, 
    epicsGuard < casClientMutex > & clientGuard,
    epicsGuard < evSysMutex > & evGuard )
{
    return this->monitor.executeEvent ( 
        client, * this, *this->pValue, 
        clientGuard, evGuard );
}

void casMonEvent::assign ( const gdd & valueIn )
{
	this->pValue = & valueIn;
}

void casMonEvent::swapValues ( casMonEvent & in )
{
    assert ( & in.monitor == & this->monitor );
    this->pValue.swap ( in.pValue );
}

casMonEvent::~casMonEvent ()
{
}

#ifdef CXX_PLACEMENT_DELETE
void casMonEvent::operator delete ( void * pCadaver, 
    tsFreeList < class casMonEvent, 1024, epicsMutexNOOP > & freeList ) 
{
    freeList.release ( pCadaver, sizeof ( casMonEvent ) );
}
#endif

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



