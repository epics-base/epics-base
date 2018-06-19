/* 
 * Copyright: Stanford University / SLAC National Laboratory.
 *
 * EPICS BASE is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution. 
 *
 * Author: Till Straumann <strauman@slac.stanford.edu>, 2014
 */ 

/*
 * Check stack trace functionality
 */

#include "epicsStackTrace.h"
#include "errlog.h"
#include "epicsUnitTest.h"
#include "testMain.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define TST_BUFSZ 10000

#define MAXP      10

/* estimated size of (compiled) epicsStackTraceRecurseGbl */
#define WINDOW_SZ  400

typedef struct TestDataRec_ {
    char buf[TST_BUFSZ];
    int  pos;
} TestDataRec, *TestData;

typedef void (*RecFn)(int);

/* We want a stack trace and need a few nested routines.
 * The whole magic here is intended to prevent a compiler
 * from optimizing the call stack away:
 *   - call via a volatile pointer
 *   - add a call to a no-op function at the end so that
 *     tail-call optimization doesn't eliminate the call
 *     stack.
 *
 * We use a local (static) and a global routine to test
 * if the stacktrace supports either flavor.
 */

void epicsStackTraceRecurseGbl(int lvl);
static void epicsStackTraceRecurseLcl(int lvl);

void nopFn(int lvl)
{
}

RecFn volatile lfp = epicsStackTraceRecurseLcl;
RecFn volatile gfp = epicsStackTraceRecurseGbl;
RecFn volatile nfp = nopFn;

static void
epicsStackTraceRecurseLcl(int lvl)
{
    if ( lvl )
        gfp(lvl-1);
    else
        epicsStackTrace();

    /* call something so that the call through gfp() doesn't
     * get optimized into a jump (tail-call optimization)
     */
    nfp(0);
}

void epicsStackTraceRecurseGbl(int lvl)
{
    if ( lvl )
        lfp(lvl-1);
    else
        epicsStackTrace();

    /* call something so that the call through gfp() doesn't
     * get optimized into a jump (tail-call optimization)
     */
    nfp(0);
}

static void logClient(void *ptr, const char *msg)
{
    TestData td = ptr;
    size_t sz = strlen(msg);
    size_t mx = sizeof(td->buf) - td->pos - 1;

    if ( sz > mx )
        sz = mx;
    strncpy( td->buf+td->pos, msg, sz );
    td->pos += sz;
}

static int
findStringOcc(const char *buf, const char *what)
{
    int rval = 0;
    size_t l = strlen(what);
    int ch;

    while ( (buf = strstr(buf, what)) ) {
        /* Is it just a prefix? */
        ch = buf[l];
        if ( ! isalnum(ch) && '_' != ch ) {
            rval++;
        }
        buf += l;
    }

    testDiag("found %i x %s\n", rval, what);

    return rval;
}

static int
findNumOcc(const char *buf)
{
    void *ptrs[MAXP];
    int n_ptrs = 0;
    int i,j;
    int rval = 0;

    while ( n_ptrs < sizeof(ptrs)/sizeof(ptrs[0]) && (buf=strchr(buf,'[')) ) {
        if ( 1 == sscanf(buf+1,"%p", &ptrs[n_ptrs]) )
            n_ptrs++;
        buf++;
    }
    /* We should find an address close to epicsStackTraceRecurseGbl twice */
    for (i=0; i<n_ptrs-1; i++) {
        /* I got a (unjustified) index-out-of-bound warning 
         * when setting j=i+1 here. Thus the weird j!= i check...
         */
        j = i;
        while ( j < n_ptrs ) {
            if ( j != i && ptrs[j] == ptrs[i] ) {
                if ( (char*)ptrs[i] >= (char*)epicsStackTraceRecurseGbl && (char*)ptrs[i] < (char*)epicsStackTraceRecurseGbl + WINDOW_SZ ) {
                    rval ++;    
                    testDiag("found address %p again\n", ptrs[i]);
                }
            }
            j++;
        }
    }
    return rval;
}

MAIN(epicsStackTraceTest)
{
    int features, all_features;
    TestDataRec testData;
    int gblFound, lclFound, numFound, dynFound;
    char *nl, *p;

    testData.pos = 0;

    testPlan(5);

    features = epicsStackTraceGetFeatures(); 

    all_features =   EPICS_STACKTRACE_LCL_SYMBOLS
                   | EPICS_STACKTRACE_GBL_SYMBOLS
                   | EPICS_STACKTRACE_DYN_SYMBOLS
                   | EPICS_STACKTRACE_ADDRESSES;

    if ( ! testOk( (features & ~all_features) == 0,
              "epicsStackTraceGetFeatures() obtains features") )
        testAbort("epicsStackTraceGetFeatures() not working as expected");

    testData.pos = 0;

    testDiag("calling a few nested routines and eventually dump a stack trace");

    eltc(0);
    errlogAddListener( logClient, &testData );
    gfp(3);
    errlogRemoveListeners( logClient, &testData );
    eltc(1);

    /* ensure there's a terminating NUL -- we have reserved space for it */
    testData.buf[testData.pos] = 0;

    testDiag("now scan the result for what we expect");

    dynFound = findStringOcc( testData.buf, "epicsStackTrace" );
    gblFound = findStringOcc( testData.buf, "epicsStackTraceRecurseGbl" );
    lclFound = findStringOcc( testData.buf, "epicsStackTraceRecurseLcl" );
    numFound = findNumOcc   ( testData.buf );

    if ( (features & EPICS_STACKTRACE_DYN_SYMBOLS) ) {
        testOk( dynFound == 1, "dumping symbol from library" );
    } else {
        testSkip(1, "no support for dumping library symbols on this platform");
    }

    if ( (features & EPICS_STACKTRACE_GBL_SYMBOLS) ) {
        testOk( gblFound == 2, "dumping global symbols" );
    } else {
        testSkip(1, "no support for dumping global symbols on this platform");
    }

    if ( (features & EPICS_STACKTRACE_LCL_SYMBOLS) ) {
        testOk( lclFound == 2, "dumping local symbols" );
    } else {
        testSkip(1, "no support for dumping local symbols on this platform");
    }

    if ( (features & EPICS_STACKTRACE_ADDRESSES) ) {
        testOk(  numFound > 0, "dumping addresses" );
    } else {
        testSkip(1 , "no support for dumping addresses on this platform");
    }

    p = testData.buf;
    while ( (nl = strchr(p,'\n')) ) {
        *nl = 0;
        testDiag("%s",p);
        *nl = '\n';
        p   = nl+1;
    }
    testDiag("%s", p);

    testDone();

    return 0;
}
