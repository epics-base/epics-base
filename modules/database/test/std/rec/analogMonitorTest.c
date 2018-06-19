/*************************************************************************\
* Copyright (c) 2014 ITER Organization.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <Ralph.Lange@gmx.de>
 */

#include <string.h>

#include "registryFunction.h"
#include "osiFileName.h"
#include "epicsThread.h"
#include "epicsMath.h"
#include "epicsUnitTest.h"
#include "dbAccessDefs.h"
#include "dbStaticLib.h"
#include "dbEvent.h"
#include "caeventmask.h"
#include "db_field_log.h"
#include "chfPlugin.h"
#include "iocInit.h"
#include "testMain.h"
#include "epicsExport.h"

/* Test parameters */

#define NO_OF_RECORD_TYPES 7
#define NO_OF_DEADBANDS 3
#define NO_OF_PATTERNS 16
#define NO_OF_VALUES_PER_SEQUENCE 2

void analogMonitorTest_registerRecordDeviceDriver(struct dbBase *);

/* Indices for record type, deadband type, deadband value, test number, val in sequence */
static int irec, ityp, idbnd, itest, iseq;

/* Records to test with */
static const char t_Record[NO_OF_RECORD_TYPES][10] = {
    {"ai"}, {"ao"}, {"calc"}, {"calcout"}, {"dfanout"}, {"sel"}, {"sub"},
};
/* Deadband types to test */
static const char t_DbndType[2][6] = { {".MDEL"}, {".ADEL"} };
/* Different deadbands to test with */
static double t_Deadband[NO_OF_DEADBANDS] = { -1, 0, 1.5 };
/* Value sequences for each of the 16 tests */
static double t_SetValues[NO_OF_PATTERNS][NO_OF_VALUES_PER_SEQUENCE];
/* Expected updates (1=yes) for each sequence of each test of each deadband */
static int t_ExpectedUpdates[NO_OF_DEADBANDS][NO_OF_PATTERNS][NO_OF_VALUES_PER_SEQUENCE] = {
    {   /* deadband = -1 */
        {1, 1}, {1, 1}, {1, 1}, {1, 1},
        {1, 1}, {1, 1}, {1, 1}, {1, 1},
        {1, 1}, {1, 1}, {1, 1}, {1, 1},
        {1, 1}, {1, 1}, {1, 1}, {1, 1},
    },
    {   /* deadband = 0 */
        {1, 1}, {0, 1}, {0, 0}, {0, 0},
        {1, 1}, {1, 0}, {1, 1}, {1, 1},
        {1, 1}, {1, 1}, {1, 0}, {1, 1},
        {1, 1}, {1, 1}, {1, 1}, {1, 0},
    },
    {   /* deadband = 1.5 */
        {0, 1}, {0, 1}, {0, 0}, {0, 0},
        {1, 1}, {1, 0}, {1, 1}, {1, 1},
        {1, 1}, {1, 1}, {1, 0}, {1, 1},
        {1, 1}, {1, 1}, {1, 1}, {1, 0},
    },
};
static int t_ReceivedUpdates[NO_OF_PATTERNS][NO_OF_VALUES_PER_SEQUENCE];

/* Dummy subroutine needed for sub record */

static long myTestSub(void *p) {
    return 0;
}


/* Minimal pre-chain plugin to divert all monitors back into the test (before they hit the queue) */

static db_field_log* filter(void* pvt, dbChannel *chan, db_field_log *pfl) {
    double val = *((double*)chan->addr.pfield);

    /* iseq == -1 is the value reset before the test pattern -> do not count */
    if (iseq >= 0) {
        t_ReceivedUpdates[itest][iseq] = 1;
        testOk((val == t_SetValues[itest][iseq]) || (isnan(val) && isnan(t_SetValues[itest][iseq])),
               "update %d pattern %2d with %s = %2.1f (expected %f, got %f)",
               iseq, itest, (ityp==0?"MDEL":"ADEL"), t_Deadband[idbnd], t_SetValues[itest][iseq], val);
    }
    db_delete_field_log(pfl);
    return NULL;
}

static void channelRegisterPre(dbChannel *chan, void *pvt,
                               chPostEventFunc **cb_out, void **arg_out, db_field_log *probe)
{
    *cb_out = filter;
}

static chfPluginIf pif = {
    NULL, /* allocPvt, */
    NULL, /* freePvt, */
    NULL, /* parse_error, */
    NULL, /* parse_ok, */
    NULL, /* channel_open, */
    channelRegisterPre,
    NULL, /* channelRegisterPost, */
    NULL, /* channel_report, */
    NULL  /* channel_close */
};


MAIN(analogMonitorTest)
{
    dbChannel *pch;
    const chFilterPlugin *plug;
    const char test[] = "test";
    dbEventCtx evtctx;
    dbEventSubscription subscr;
    unsigned mask;
    struct dbAddr vaddr, daddr;
    double val;
    char chan[50]; /* Channel name */
    char cval[50]; /* Name for test values */
    char cdel[50]; /* Name for deadband values */

    /* Test patterns:
     * 0: step less than deadband (of 1.5)
     * 1: step larger than deadband (of 1.5)
     * 2: no change
     * 3: -0.0 -> +0.0
     * ... all possible combinations of steps
     * between: finite / NaN / -inf / +inf
     */
    t_SetValues[ 0][0] =  1.0;      t_SetValues[ 0][1] = 2.0;
    t_SetValues[ 1][0] =  0.0;      t_SetValues[ 1][1] = 2.0;
    t_SetValues[ 2][0] =  0.0;      t_SetValues[ 2][1] = 0.0;
    t_SetValues[ 3][0] = -0.0;      t_SetValues[ 3][1] = 0.0;
    t_SetValues[ 4][0] =  epicsNAN; t_SetValues[ 4][1] = 1.0;
    t_SetValues[ 5][0] =  epicsNAN; t_SetValues[ 5][1] = epicsNAN;
    t_SetValues[ 6][0] =  epicsNAN; t_SetValues[ 6][1] = epicsINF;
    t_SetValues[ 7][0] =  epicsNAN; t_SetValues[ 7][1] = -epicsINF;
    t_SetValues[ 8][0] =  epicsINF; t_SetValues[ 8][1] = 1.0;
    t_SetValues[ 9][0] =  epicsINF; t_SetValues[ 9][1] = epicsNAN;
    t_SetValues[10][0] =  epicsINF; t_SetValues[10][1] = epicsINF;
    t_SetValues[11][0] =  epicsINF; t_SetValues[11][1] = -epicsINF;
    t_SetValues[12][0] = -epicsINF; t_SetValues[12][1] = 1.0;
    t_SetValues[13][0] = -epicsINF; t_SetValues[13][1] = epicsNAN;
    t_SetValues[14][0] = -epicsINF; t_SetValues[14][1] = epicsINF;
    t_SetValues[15][0] = -epicsINF; t_SetValues[15][1] = -epicsINF;

    registryFunctionAdd("myTestSub", (REGISTRYFUNCTION) myTestSub);

    testPlan(1793);

    if (dbReadDatabase(&pdbbase, "analogMonitorTest.dbd",
            "." OSI_PATH_LIST_SEPARATOR ".." OSI_PATH_LIST_SEPARATOR
            "../O.Common" OSI_PATH_LIST_SEPARATOR "O.Common", NULL))
        testAbort("Error reading database description 'analogMonitorTest.dbd'");

    analogMonitorTest_registerRecordDeviceDriver(pdbbase);

    if (dbReadDatabase(&pdbbase, "analogMonitorTest.db",
            "." OSI_PATH_LIST_SEPARATOR "..", NULL))
        testAbort("Error reading test database 'analogMonitorTest.db'");

    /* Start the core IOC (no CA) */
    iocBuildIsolated();

    evtctx = db_init_events();
    chfPluginRegister(test, &pif, NULL);

    plug = dbFindFilter(test, strlen(test));
    testOk(!!plug, "interceptor plugin registered");

    /* Loop over all analog record types (one instance each) */
    for (irec = 0; irec < NO_OF_RECORD_TYPES; irec++) {
        strcpy(cval, t_Record[irec]);
        strcpy(chan, cval);
        strcat(chan, ".VAL{\"test\":{}}");
        if ((strcmp(t_Record[irec], "sel") == 0)
                || (strcmp(t_Record[irec], "calc") == 0)
                || (strcmp(t_Record[irec], "calcout") == 0)) {
            strcat(cval, ".A");
        } else {
            strcat(cval, ".VAL");
        }

        testDiag("--------------------------------------------------------");
        testDiag("Testing the %s record", t_Record[irec]);
        testDiag("--------------------------------------------------------");

        pch = dbChannelCreate(chan);
        testOk(!!pch, "dbChannel with test plugin created");
        testOk(!dbChannelOpen(pch), "dbChannel opened");

        dbNameToAddr(cval, &vaddr);

        /* Loop over both tested deadband types */
        for (ityp = 0; ityp < 2; ityp++) {
            strcpy(cdel, t_Record[irec]);
            strcat(cdel, t_DbndType[ityp]);
            dbNameToAddr(cdel, &daddr);
            mask = (ityp==0?DBE_VALUE:DBE_ARCHIVE);
            subscr = db_add_event(evtctx, pch, NULL, NULL, mask);
            db_event_enable(subscr);

            /* Loop over all tested deadband values */
            for (idbnd = 0; idbnd < NO_OF_DEADBANDS; idbnd++) {
                testDiag("Test %s%s = %g", t_Record[irec], t_DbndType[ityp], t_Deadband[idbnd]);
                dbPutField(&daddr, DBR_DOUBLE, (void*) &t_Deadband[idbnd], 1);
                memset(t_ReceivedUpdates, 0, sizeof(t_ReceivedUpdates));

                /* Loop over all test patterns */
                for (itest = 0; itest < NO_OF_PATTERNS; itest++) {
                    iseq = -1;
                    val = 0.0;
                    dbPutField(&vaddr, DBR_DOUBLE, (void*) &val, 1);

                    /* Loop over the test sequence */
                    for (iseq = 0; iseq < NO_OF_VALUES_PER_SEQUENCE; iseq++) {
                        dbPutField(&vaddr, DBR_DOUBLE, (void*) &t_SetValues[itest][iseq], 1);
                    }
                    /* Check expected vs. actual monitors */
                    testOk( (t_ExpectedUpdates[idbnd][itest][0] == t_ReceivedUpdates[itest][0]) &&
                            (t_ExpectedUpdates[idbnd][itest][1] == t_ReceivedUpdates[itest][1]),
                            "pattern %2d with %s = %2.1f (expected %d-%d, got %d-%d)",
                            itest, (ityp==0?"MDEL":"ADEL"), t_Deadband[idbnd],
                            t_ExpectedUpdates[idbnd][itest][0], t_ExpectedUpdates[idbnd][itest][1],
                            t_ReceivedUpdates[itest][0], t_ReceivedUpdates[itest][1]);
                }
            }
            db_cancel_event(subscr);
        }
    }
    iocShutdown();
    return testDone();
}
