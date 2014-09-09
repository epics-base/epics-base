/* 
 * Copyright: Stanford University / SLAC National Laboratory.
 *
 * EPICS BASE is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution. 
 *
 * Author: Till Straumann <strauman@slac.stanford.edu>, 2011
 */ 

#define epicsExportSharedSymbols
#include "epicsStackTracePvt.h"
#include "epicsStackTrace.h"

int epicsFindAddr(void *addr, epicsSymbol *sym_p)
{
	return -1;
}

int epicsStackTraceGetFeatures(void)
{
void *test[10];

	/* If frame-pointer optimization is on then CaptureStackBackTrace
	 * does not work. Make sure all EPICS is built with -Oy-
	 */
	if ( 0 == epicsBackTrace(test, sizeof(test)/sizeof(test[0])) )
		return 0;

	/* address->symbol conversion not implemented (yet) */
	return EPICS_STACKTRACE_ADDRESSES;
}
