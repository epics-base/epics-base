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
 * Revision 1.3  1996/09/16 18:23:56  jhill
 * vxWorks port changes
 *
 * Revision 1.2  1996/06/21 02:30:52  jhill
 * solaris port
 *
 * Revision 1.1.1.1  1996/06/20 00:28:14  jhill
 * ca server installation
 *
 *
 */

#include <server.h>
#include <caServerIIL.h>	// caServerI in line func
#include <dbMapper.h>        	// ait to dbr types 
#include <gddAppTable.h>        // EPICS application type table

//
// NOTES
// .01 All use of member pCAS in this file must first verify
// that we successfully created a caServerI in
// the constructor
//


//
// caServer::caServer()
//
caServer::caServer(unsigned pvMaxNameLengthIn, unsigned pvCountEstimateIn, 
		unsigned maxSimultaneousIOIn) :
        	pCAS (new caServerI(*this, pvMaxNameLengthIn,
                	pvCountEstimateIn, maxSimultaneousIOIn)),
		valueEventMask(this->registerEvent("value")),
		logEventMask(this->registerEvent("log")),
		alarmEventMask(this->registerEvent("alarm"))
{
	static		init;

	if (!init) {
		gddMakeMapDBR(gddApplicationTypeTable::app_table);
		init = TRUE;
	}

	if (!this->pCAS) {
		errMessage(S_cas_noMemory, NULL);
		return;
	}

	if (!this->pCAS->ready()) {
		delete this->pCAS;
		this->pCAS = NULL;
		return;
	}
}

//
// caServer::~caServer()
//
caServer::~caServer()
{
	if (this->pCAS) {
		delete this->pCAS;
	}
}

//
// caServer::registerEvent()
//
casEventMask caServer::registerEvent (const char *pName)
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
void caServer::show(unsigned level)
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
void caServer::setDebugLevel (unsigned level)
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
unsigned caServer::getDebugLevel ()
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

