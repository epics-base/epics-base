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
 *
 * caServerOS.c
 * $Id$
 *
 *
 * $Log$
 * Revision 1.2  1996/09/16 18:27:09  jhill
 * vxWorks port changes
 *
 * Revision 1.1  1996/09/04 22:06:43  jhill
 * installed
 *
 * Revision 1.1.1.1  1996/06/20 00:28:06  jhill
 * ca server installation
 *
 *
 */

#include <taskLib.h>
#include <task_params.h>

//
// CA server
// 
#include <server.h>

//
// aServerOS::operator -> ()
//
inline caServerI * caServerOS::operator -> ()
{
	return &this->cas;
}

//
// casBeaconTimer::expire()
//
void casBeaconTimer::expire()	
{
	os->sendBeacon ();
}

//
// casBeaconTimer::again()
//
osiBool casBeaconTimer::again()	
{
	return osiTrue; 
}

//
// casBeaconTimer::delay()
//
const osiTime casBeaconTimer::delay()	
{
	return os->getBeaconPeriod();
}


//
// caServerOS::init()
//
caStatus caServerOS::init()
{
	this->pBTmr = new casBeaconTimer((*this)->getBeaconPeriod(), *this);
	if (!this->pBTmr) {
                ca_printf("CAS: Unable to start server beacon\n");
		return S_cas_noMemory;
        }

	//
	// WRS still passes pointers in ints
	//
	assert (sizeof(int)==sizeof(&this->cas));

	this->tid = taskSpawn (
			REQ_SRVR_NAME, 
			REQ_SRVR_PRI, 
			REQ_SRVR_OPT,
			REQ_SRVR_STACK, 
			(FUNCPTR) caServerEntry, // get your act together WRS
			(int) &this->cas, // get your act together WRS
			NULL, 
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
	if (this->tid==ERROR) {
		return S_cas_noMemory;
	}

	return S_cas_success;
}


//
// caServerOS::~caServerOS()
//
caServerOS::~caServerOS()
{
	if (this->pBTmr) {
		delete this->pBTmr;
	}

	if (taskIdVerify(this->tid)==OK) 
	{
		taskDelete(this->tid);
	}
}


//
// caServerEntry()
//
void caServerEntry(caServerI *pCAS)
{
	//
	// forever
	//
	while (TRUE) {
		pCAS->connectCB();
		printf("process timer q here?\n");
	}
}

