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
 */

#include "dbMapper.h"		// ait to dbr types 
#include "gddAppTable.h"	// EPICS application type table

#include "server.h"
#include "caServerIIL.h"	// caServerI in line func

//
// if the compiler supports explicit instantiation of
// template member functions
//
#if defined(EXPL_TEMPL)
	//
	// From Stroustrups's "The C++ Programming Language"
	// Appendix A: r.14.9 
	//
	// This explicitly instantiates the template class's member
	// functions into "templInst.o"
	//
	template class resTable <casEventMaskEntry, stringId>;
	template class resTable <casRes, chronIntId>;
#endif

//
// caServer::caServer()
//
epicsShareFunc caServer::caServer (unsigned pvCountEstimateIn)
{
    static bool init = false;
    
    if (!init) {
        gddMakeMapDBR(gddApplicationTypeTable::app_table);
        init = true;
    }    

    this->pCAS = new caServerI(*this, pvCountEstimateIn);
    if (this->pCAS==NULL) {
        errMessage (S_cas_noMemory, " - unable to create caServer");
        throw S_cas_noMemory;
    }
}

//
// caServer::~caServer()
//
epicsShareFunc caServer::~caServer()
{
	if (this->pCAS) {
		delete this->pCAS;
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
epicsShareFunc casEventMask caServer::registerEvent (const char *pName)
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
epicsShareFunc unsigned caServer::getDebugLevel () const
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
epicsShareFunc casEventMask caServer::valueEventMask () const
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
epicsShareFunc casEventMask caServer::logEventMask () const
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
epicsShareFunc casEventMask caServer::alarmEventMask () const
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

