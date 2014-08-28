/* 
 * Copyright: Stanford University / SLAC National Laboratory.
 *
 * EPICS BASE is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution. 
 *
 * Author: Till Straumann <strauman@slac.stanford.edu>, 2011
 */ 

#include "epicsStackTrace.h"
#include "errlog.h"

epicsShareFunc void epicsStackTrace(void)
{
	errlogFlush();
	errlogPrintf("StackTrace not supported on this platform, sorry\n");
	errlogFlush();
}

epicsShareFunc int epicsStackTraceGetFeatures(void)
{
	return 0;
}

