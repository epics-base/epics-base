//
// $Id$
// Author: Jeff HIll (LANL)
//
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

