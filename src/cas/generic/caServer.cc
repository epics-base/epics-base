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

#include "dbMapper.h"		// ait to dbr types 
#include "gddAppTable.h"	// EPICS application type table

#define epicsExportSharedSymbols
#include "server.h"
#include "caServerIIL.h"	// caServerI in line func

//
// caServer::caServer()
//
epicsShareFunc caServer::caServer ()
{
    static bool init = false;
    
    if (!init) {
        gddMakeMapDBR(gddApplicationTypeTable::app_table);
        init = true;
    }    

    this->pCAS = new caServerI(*this);
}

//
// caServer::~caServer()
//
epicsShareFunc caServer::~caServer()
{
	if (this->pCAS) {
		delete this->pCAS;
        this->pCAS = NULL;
	}
}

//
// caServer::pvExistTest()
//
epicsShareFunc pvExistReturn caServer::pvExistTest (const casCtx &, const char *)
{
	return pverDoesNotExistHere;
}

//
// caServer::createPV()
//
epicsShareFunc pvCreateReturn caServer::createPV (const casCtx &, const char *)
{
	return S_casApp_pvNotFound;
}

//
// caServer::pvAttach()
//
epicsShareFunc pvAttachReturn caServer::pvAttach (const casCtx &ctx, const char *pAliasName)
{
	//
	// remain backwards compatible (call deprecated routine)
	//
	return this->createPV(ctx, pAliasName);
}

//
// caServer::registerEvent()
//
epicsShareFunc casEventMask caServer::registerEvent (const char *pName) // X aCC 361
{
	if (this->pCAS) {
		return this->pCAS->registerEvent(pName);
	}
	else {
		casEventMask emptyMask;
		printf("caServer:: no server internals attached\n");
		return emptyMask;
	}
}

//
// caServer::show()
//
epicsShareFunc void caServer::show(unsigned level) const
{
	if (this->pCAS) {
		this->pCAS->show(level);
	}
	else {
		printf("caServer:: no server internals attached\n");
	}
}

//
// caServer::setDebugLevel()
//
epicsShareFunc void caServer::setDebugLevel (unsigned level)
{
	if (pCAS) {
		this->pCAS->setDebugLevel(level);
	}
	else {
		printf("caServer:: no server internals attached\n");
	}
}

//
// caServer::getDebugLevel()
//
epicsShareFunc unsigned caServer::getDebugLevel () const // X aCC 361
{
    if (pCAS) {
        return this->pCAS->getDebugLevel();
    }
    else {
        printf("caServer:: no server internals attached\n");
        return 0u;
    }
}

//
// caServer::valueEventMask ()
//
epicsShareFunc casEventMask caServer::valueEventMask () const // X aCC 361
{
    if (pCAS) {
        return this->pCAS->valueEventMask();
    }
    else {
        printf("caServer:: no server internals attached\n");
        return casEventMask();
    }
}

//
// caServer::logEventMask ()
//
epicsShareFunc casEventMask caServer::logEventMask () const // X aCC 361
{
    if (pCAS) {
        return this->pCAS->logEventMask();
    }
    else {
        printf("caServer:: no server internals attached\n");
        return casEventMask();
    }
}

//
// caServer::alarmEventMask ()
//
epicsShareFunc casEventMask caServer::alarmEventMask () const // X aCC 361
{
    if (pCAS) {
        return this->pCAS->alarmEventMask();
    }
    else {
        printf("caServer:: no server internals attached\n");
        return casEventMask();
    }
}

//
// caServer::alarmEventMask ()
//
class epicsTimer & caServer::createTimer ()
{
    return fileDescriptorManager.createTimer ();
}

//
// caServer::subscriptionEventsProcessed
//
epicsShareFunc unsigned caServer::subscriptionEventsProcessed () const // X aCC 361
{
    if ( pCAS ) {
        return this->pCAS->subscriptionEventsProcessed();
    }
    else {
        return 0u;
    }
}

//
// caServer::subscriptionEventsPosted
//
epicsShareFunc unsigned caServer::subscriptionEventsPosted () const // X aCC 361
{
    if ( pCAS ) {
        return this->pCAS->subscriptionEventsPosted ();
    }
    else {
        return 0u;
    }
}

epicsShareFunc void caServer::generateBeaconAnomaly ()
{
    if ( pCAS ) {
        this->pCAS->generateBeaconAnomaly ();
    }
}

//
// casRes::~casRes()
//
// This must be virtual so that derived destructor will
// be run indirectly. Therefore it cannot be inline.
//
casRes::~casRes()
{
}

//
// This must be virtual so that derived destructor will
// be run indirectly. Therefore it cannot be inline.
//
casEvent::~casEvent() 
{
}

