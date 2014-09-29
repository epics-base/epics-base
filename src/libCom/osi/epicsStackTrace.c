/* 
 * Copyright: Stanford University / SLAC National Laboratory.
 *
 * EPICS BASE is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution. 
 *
 * Author: Till Straumann <strauman@slac.stanford.edu>, 2011, 2014
 */ 

#include <stdlib.h>

#define epicsExportSharedSymbols
#include "epicsThread.h"
#include "epicsMutex.h"
#include "errlog.h"
#include "epicsStackTracePvt.h"
#include "epicsStackTrace.h"

/* How many stack frames to capture               */
#define MAXDEPTH 100

static epicsThreadOnceId stackTraceInitId = EPICS_THREAD_ONCE_INIT;
static epicsMutexId      stackTraceMtx;

static void stackTraceInit(void *unused)
{
    stackTraceMtx = epicsMutexMustCreate();
}

static void stackTraceLock(void)
{
    epicsThreadOnce( &stackTraceInitId, stackTraceInit, 0 );
    epicsMutexLock( stackTraceMtx );
}

static void stackTraceUnlock(void)
{
    epicsMutexUnlock( stackTraceMtx );
}

static int
dumpInfo(void *addr, epicsSymbol *sym_p)
{
int rval = 0;

	rval += errlogPrintf("[%*p]", (int)(sizeof(addr)*2 + 2), addr);
	if ( sym_p ) {
		if ( sym_p->f_nam ) {
			rval += errlogPrintf(": %s", sym_p->f_nam);
		}
		if ( sym_p->s_nam ) {
			rval += errlogPrintf("(%s+0x%lx)", sym_p->s_nam, (unsigned long)((char*)addr - (char*)sym_p->s_val));
		} else {
			rval += errlogPrintf("(<no symbol information>)");
		}
	}
	rval += errlogPrintf("\n");
	errlogFlush();

    return rval;
}

void epicsStackTrace(void)
{
void        **buf;
int         i,n;
epicsSymbol sym;

	if ( 0 == epicsStackTraceGetFeatures() ) {
		/* unsupported on this platform */
		return;
	}

    if ( ! (buf = malloc(sizeof(*buf) * MAXDEPTH))) {
        free(buf);
        errlogPrintf("epicsStackTrace(): not enough memory for backtrace\n");
        return;
    }

    n = epicsBackTrace(buf, MAXDEPTH);

	if ( n > 0 ) {

		stackTraceLock();

		errlogPrintf("Dumping a stack trace of thread '%s':\n", epicsThreadGetNameSelf());

		errlogFlush();

		for ( i=0; i<n; i++ ) {
			if ( 0 == epicsFindAddr(buf[i], &sym) )
				dumpInfo(buf[i], &sym);
			else
				dumpInfo(buf[i], 0);
		}

		errlogFlush();

		stackTraceUnlock();

	}

    free(buf);
}

int epicsStackTraceGetFeatures()
{
void *test[2];

static int initflag = 10; /* init to a value larger than the test snapshot */

	/* don't bother about epicsOnce -- if there should be a race and
	 * the detection code is executed multiple times that is no big deal.
	 */
	if ( 10 == initflag ) {
		initflag = epicsBackTrace(test, sizeof(test)/sizeof(test[0]));
	}

	if ( initflag <= 0 )
		return 0; /* backtrace doesn't work or is not supported */

	return ( EPICS_STACKTRACE_ADDRESSES | epicsFindAddrGetFeatures() );
}
