/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
//
// Author: Jeff HIll (LANL)
//

#include <vxWorks.h>
#include <taskLib.h>

#include "exServer.h"

//
// so we can call this from the vxWorks shell
//
extern "C" {

exServer	*pExampleCAS;

//
// excas ()
// (vxWorks example server entry point)  
//
int excas (unsigned debugLevel, unsigned delaySec)
{
	epicsTime		begin(epicsTime::getCurrent());
	exServer	*pCAS;

	pCAS = new exServer(32u,5u,500u);
	if (!pCAS) {
		return (-1);
	}

	pCAS->setDebugLevel(debugLevel);
	pExampleCAS = pCAS;

	if (delaySec==0u) {
		//
		// loop here forever
		//
		while (1) {
			taskDelay(10);
		}
	}
	else {
		epicsTime total( ((float)delaySec) );
		epicsTime delay(epicsTime::getCurrent() - begin);
		//
		// loop here untill the specified execution time
		// expires
		//
		while (delay < total) {
			taskDelay(10);
			delay = epicsTime::getCurrent() - begin;
		}
	}
	pCAS->show(debugLevel);
	pExampleCAS = NULL;
	delete pCAS;
	return 0;
}

int excasShow(unsigned level)
{
	if (pExampleCAS!=NULL) {
		pExampleCAS->show(level);
	}
	return 0;
}

} // extern "C"

