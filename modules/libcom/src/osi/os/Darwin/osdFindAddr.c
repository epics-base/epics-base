/* 
 * Copyright: Stanford University / SLAC National Laboratory.
 *
 * EPICS BASE is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution. 
 *
 * Author: Till Straumann <strauman@slac.stanford.edu>, 2011, 2014
 */ 

/* Make sure dladdr() is visible */
#define _DARWIN_C_SOURCE

#include <dlfcn.h>

#define epicsExportSharedSymbols
#include "epicsStackTrace.h"
#include "epicsStackTracePvt.h"

/* Darwin's finds local symbols, too :-) */

int epicsFindAddr(void *addr, epicsSymbol *sym_p)
{
    Dl_info    inf;

    if ( ! dladdr(addr, &inf) ) {
        sym_p->f_nam = 0;
        sym_p->s_nam = 0;
        sym_p->s_val = 0;
    } else {
        sym_p->f_nam = inf.dli_fname;
        sym_p->s_nam = inf.dli_sname;
        sym_p->s_val = inf.dli_saddr;
    }

    return 0;
}

int epicsFindAddrGetFeatures(void)
{
    return  EPICS_STACKTRACE_LCL_SYMBOLS
          | EPICS_STACKTRACE_GBL_SYMBOLS
          | EPICS_STACKTRACE_DYN_SYMBOLS;
}
