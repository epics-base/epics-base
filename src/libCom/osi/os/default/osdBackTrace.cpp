/* 
 * Copyright: Stanford University / SLAC National Laboratory.
 *
 * EPICS BASE is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution. 
 *
 * Author: Till Straumann <strauman@slac.stanford.edu>, 2011, 2014
 */ 

#define epicsExportSharedSymbols
#include "epicsStackTracePvt.h"

int epicsBackTrace(void **buf, int buf_sz)
{
	return -1;
}
