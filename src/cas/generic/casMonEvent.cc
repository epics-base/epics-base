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


#include "server.h"
#include "casEventSysIL.h" // casEventSys in line func
#include "casMonEventIL.h" // casMonEvent in line func
#include "casCtxIL.h" // casCtx in line func

//
// casMonEvent::cbFunc()
//
caStatus casMonEvent::cbFunc(casEventSys &eSys)
{
        casMonitor      *pMon;
        caStatus        status;
 
        //
        // ignore this event if it is stale and there is
        // no call back object associated with it
        //
        pMon = eSys.resIdToMon(this->id);
        if (!pMon) {
                /*
                 * we know this isnt an overflow event because those are
                 * removed from the queue when the call back object
                 * is deleted
                 */
                status = S_casApp_success;
                delete this;
        }
        else {
                status = pMon->executeEvent(this);
        }
 
        return status;
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

