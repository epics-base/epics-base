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
 * Revision 1.1.1.1  1996/06/20 00:28:16  jhill
 * ca server installation
 *
 *
 */


#include <server.h>
#include <casEventSysIL.h>

//
// casMonEvent::casMonEvent()
//
casMonEvent::casMonEvent (casMonitor &monitor, gdd &newValue) :
        pValue(&newValue),
        id(monitor.casRes::getId())
{
        int gddStatus;
        gddStatus = this->pValue->reference();
        assert (!gddStatus);
}
 
//
// casMonEvent::casMonEvent()
//
casMonEvent::casMonEvent (casMonEvent &initValue) :
        pValue(initValue.pValue),
        id(initValue.id)
{
        int gddStatus;
        if (this->pValue) {
                gddStatus = this->pValue->reference();
                assert (!gddStatus);
        }
}

//
// ~casMonEvent ()
//
casMonEvent::~casMonEvent ()
{
	this->clear();
}

//
// casMonEvent::cbFunc()
//
caStatus casMonEvent::cbFunc(casEventSys &eSys)
{
        casMonitor      *pMon;
        caStatus        status;
 
        //
        // ignore this event if it is stale (and there is
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
void casMonEvent::assign (casMonitor &monitor, gdd *pValueIn)
{
        int gddStatus;
        if (this->pValue) {
                gddStatus = this->pValue->unreference();
                assert (!gddStatus);
        }
        if (pValueIn) {
                gddStatus = pValueIn->reference();
                assert (!gddStatus);
        }
        this->pValue = pValueIn;
        this->id = monitor.casRes::getId();
}


