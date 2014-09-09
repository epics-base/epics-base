/* 
 * Copyright: Stanford University / SLAC National Laboratory.
 *
 * EPICS BASE is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution. 
 *
 * Author: Till Straumann <strauman@slac.stanford.edu>, 2011
 */ 

#include "epicsStackTracePvt.h"
#include "epicsStackTrace.h"

epicsShareFunc int epicsFindAddr(void *addr, epicsSymbol *sym_p)
{
	return -1;
}

epicsShareFunc int epicsStackTraceGetFeatures(void)
{
	return 0;
}
