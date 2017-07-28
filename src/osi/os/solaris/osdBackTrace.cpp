/* 
 * Copyright: Stanford University / SLAC National Laboratory.
 *
 * EPICS BASE is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution. 
 *
 * Author: Till Straumann <strauman@slac.stanford.edu>, 2014
 */ 

#include <ucontext.h>

#define epicsExportSharedSymbols
#include "epicsStackTracePvt.h"

struct wlk {
	void **buf;
	int   max;
	int   cur;
};


extern "C" {

static int
walker(uintptr_t addr, int sig, void *arg)
{
struct wlk *w_p = (struct wlk *)arg;
	if ( w_p->cur < w_p->max )
		w_p->buf[w_p->cur++] = (void*)addr;
	return 0;
}

}

int epicsBackTrace(void **buf, int buf_sz)
{
ucontext_t u;
struct     wlk d;
	d.buf = buf;
	d.max = buf_sz;
	d.cur = 0;
	if ( getcontext(&u) )
		return -1;
	walkcontext( &u, walker, &d );
	return d.cur;
}
