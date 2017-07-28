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
#include "epicsStackTrace.h"

int epicsFindAddr(void *addr, epicsSymbol *sym_p)
{
    sym_p->f_nam = 0;
    sym_p->s_nam = 0;
    sym_p->s_val = 0;
    return -1;
}

int epicsFindAddrGetFeatures(void)
{
    return 0;
}
