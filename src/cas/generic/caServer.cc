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
 *
 */

#include <server.h>

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
	caStatus 	status;
	static		init;

	if (!init) {
		gddMakeMapDBR(gddApplicationTypeTable::app_table);
		init = TRUE;
	}

	//
	// OS and IO dependent
	//
	if (!this->pCAS) {
		errMessage(S_cas_noMemory, NULL);
		return;
	}

	status = this->pCAS->init();
        if (status) {
                errMessage(status, NULL);
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
		errMessage(S_cas_noMemory, NULL);
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
                errMessage(S_cas_noMemory, NULL);
		return 0u;
        }
}
 
