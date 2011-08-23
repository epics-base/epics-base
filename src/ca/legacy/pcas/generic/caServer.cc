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
 *      $Revision-Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#include "dbMapper.h"		// ait to dbr types 
#include "gddAppTable.h"	// EPICS application type table
#include "fdManager.h"

#define epicsExportSharedSymbols
#include "casMonitor.h"
#include "caServerI.h"

caServer::caServer ()
{
    static bool init = false;
    
    if ( ! init ) {
        gddMakeMapDBR(gddApplicationTypeTable::app_table);
        init = true;
    }    

    this->pCAS = new caServerI ( *this );
}

caServer::~caServer()
{
	if (this->pCAS) {
		delete this->pCAS;
        this->pCAS = NULL;
	}
}

pvExistReturn caServer::pvExistTest ( const casCtx & ctx, 
	const caNetAddr & /* clientAddress */, const char * pPVAliasName )
{
    return this->pvExistTest ( ctx, pPVAliasName );
}

pvExistReturn caServer::pvExistTest ( const casCtx &, const char * )
{
	return pverDoesNotExistHere;
}

pvCreateReturn caServer::createPV ( const casCtx &, const char * )
{
	return S_casApp_pvNotFound;
}

pvAttachReturn caServer::pvAttach ( const casCtx &ctx, const char *pAliasName )
{
	// remain backwards compatible (call deprecated routine)
	return this->createPV ( ctx, pAliasName );
}

casEventMask caServer::registerEvent (const char *pName) // X aCC 361
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

void caServer::show(unsigned level) const
{
	if (this->pCAS) {
		this->pCAS->show(level);
	}
	else {
		printf("caServer:: no server internals attached\n");
	}
}

void caServer::setDebugLevel (unsigned level)
{
	if (pCAS) {
		this->pCAS->setDebugLevel(level);
	}
	else {
		printf("caServer:: no server internals attached\n");
	}
}

unsigned caServer::getDebugLevel () const // X aCC 361
{
    if (pCAS) {
        return this->pCAS->getDebugLevel();
    }
    else {
        printf("caServer:: no server internals attached\n");
        return 0u;
    }
}

casEventMask caServer::valueEventMask () const // X aCC 361
{
    if (pCAS) {
        return this->pCAS->valueEventMask();
    }
    else {
        printf("caServer:: no server internals attached\n");
        return casEventMask();
    }
}

casEventMask caServer::logEventMask () const // X aCC 361
{
    if (pCAS) {
        return this->pCAS->logEventMask();
    }
    else {
        printf("caServer:: no server internals attached\n");
        return casEventMask();
    }
}

casEventMask caServer::alarmEventMask () const // X aCC 361
{
    if ( pCAS ) {
        return this->pCAS->alarmEventMask ();
    }
    else {
        printf ( "caServer:: no server internals attached\n" );
        return casEventMask ();
    }
}

class epicsTimer & caServer::createTimer ()
{
    return fileDescriptorManager.createTimer ();
}

unsigned caServer::subscriptionEventsProcessed () const // X aCC 361
{
    if ( pCAS ) {
        return this->pCAS->subscriptionEventsProcessed ();
    }
    else {
        return 0u;
    }
}

unsigned caServer::subscriptionEventsPosted () const // X aCC 361
{
    if ( pCAS ) {
        return this->pCAS->subscriptionEventsPosted ();
    }
    else {
        return 0u;
    }
}

void caServer::generateBeaconAnomaly ()
{
    if ( pCAS ) {
        this->pCAS->generateBeaconAnomaly ();
    }
}
