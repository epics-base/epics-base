/*************************************************************************\
* Copyright (c) 2010 Brookhaven Science Associates, as Operator of
*     Brookhaven National Laboratory.
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 *      Author: Michael Davidsaver
 *      Date:   2010-09-24
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "epicsAssert.h"
#include "epicsThread.h"
#include "epicsEvent.h"
#include "dbDefs.h"
#include "errlog.h"
#include "epicsUnitTest.h"
#include "testMain.h"

#define LOGBUFSIZE 2048

static
const char longmsg[]="A0123456789abcdef"
                     "B0123456789abcdef"
                     "C0123456789abcdef"
                     "D0123456789abcdef"
                     "E0123456789abcdef"
                     "F0123456789abcdef"
                     "G0123456789abcdef"
                     "H0123456789abcdef"
                     "I0123456789abcdef"
                     "J0123456789abcdef"
                     "K0123456789abcdef"
                     "L0123456789abcdef"
                     "M0123456789abcdef"
                     "N0123456789abcdef"
                     "O0123456789abcdef"
                     "P0123456789abcdef"
                     "Q0123456789abcdef"
                     "R0123456789abcdef"
                     "S0123456789abcdef"
                     ;
STATIC_ASSERT(NELEMENTS(longmsg)==324);

static
const char truncmsg[]="A0123456789abcdef"
                      "B0123456789abcdef"
                      "C0123456789abcdef"
                      "D0123456789abcdef"
                      "E0123456789abcdef"
                      "F0123456789abcdef"
                      "G0123456789abcdef"
                      "H0123456789abcdef"
                      "I0123456789abcdef"
                      "J0123456789abcdef"
                      "K0123456789abcdef"
                      "L0123456789abcdef"
                      "M0123456789abcdef"
                      "N0123456789abcdef"
                      "O01<<TRUNCATED>>\n"
                      ;
STATIC_ASSERT(NELEMENTS(truncmsg)==256);

typedef struct {
    unsigned int count;
    const char* expect;
    size_t checkLen;
    epicsEventId jammer;
    int jam;
} clientPvt;

static
void logClient(void* raw, const char* msg)
{
    clientPvt *pvt = raw;
    size_t L;

    /* Simulate thread priority on non-realtime
     * OSs like Linux.  This will cause the logging
     * thread to sleep with the buffer lock held.
     */
    if (pvt->jam > 0) {
        pvt->jam = 0;
        epicsEventMustWait(pvt->jammer);
    } else if (pvt->jam < 0) {
        pvt->jam++;
        if (pvt->jam == 0)
            epicsEventMustWait(pvt->jammer);
    }

    L = strlen(msg);

    if (pvt->checkLen)
        if (!testOk(pvt->checkLen == L, "Received %d chars", (int) L)) {
            testDiag("Expected %d", (int) pvt->checkLen);
            if (!pvt->expect)
                testDiag("Message was \"%s\"", msg);
        }
    if (pvt->expect)
        if (!testOk(strcmp(pvt->expect, msg) == 0, "msg is \"%s\"", msg))
            testDiag("Expected \"%s\"", pvt->expect);

    pvt->count++;
}

MAIN(epicsErrlogTest)
{
    size_t mlen, i, N;
    char msg[256];
    clientPvt pvt, pvt2;

    testPlan(25);

    strcpy(msg, truncmsg);

    errlogInit2(LOGBUFSIZE, 256);

    pvt.count = 0;
    pvt2.count = 0;

    pvt.expect = NULL;
    pvt2.expect = NULL;

    pvt.checkLen = 0;
    pvt2.checkLen = 0;

    pvt.jam = 0;
    pvt2.jam = 0;

    pvt.jammer = epicsEventMustCreate(epicsEventEmpty);
    pvt2.jammer = epicsEventMustCreate(epicsEventEmpty);

    testDiag("Check listener registration");

    errlogAddListener(&logClient, &pvt);

    pvt.expect = "Testing";
    pvt.checkLen = strlen(pvt.expect);

    errlogPrintfNoConsole("%s", pvt.expect);
    errlogFlush();

    testOk1(pvt.count == 1);

    errlogAddListener(&logClient, &pvt2);

    pvt2.expect = pvt.expect = "Testing2";
    pvt2.checkLen = pvt.checkLen = strlen(pvt.expect);

    errlogPrintfNoConsole("%s", pvt.expect);
    errlogFlush();

    testOk1(pvt.count == 2);
    testOk1(pvt2.count == 1);

    /* Removes the first listener, but the second remains */
    errlogRemoveListener(&logClient);

    pvt2.expect = "Testing3";
    pvt2.checkLen = strlen(pvt2.expect);

    errlogPrintfNoConsole("%s", pvt2.expect);
    errlogFlush();

    testOk1(pvt.count == 2);
    testOk1(pvt2.count == 2);

    /* Remove the second listener */
    errlogRemoveListener(&logClient);

    errlogPrintfNoConsole("Something different");
    errlogFlush();

    testOk1(pvt.count == 2);
    testOk1(pvt2.count == 2);

    /* Re-add one listener */
    errlogAddListener(&logClient, &pvt);

    testDiag("Check truncation");

    pvt.expect = truncmsg;
    pvt.checkLen = 255;

    errlogPrintfNoConsole("%s", longmsg);
    errlogFlush();

    testOk1(pvt.count == 3);

    pvt.expect = NULL;

    testDiag("Check priority");
    /* For the following tests it is important that
     * the buffer should not flush until we request it
     */
    pvt.jam = 1;

    errlogPrintfNoConsole("%s", longmsg);
    epicsThreadSleep(0.1);

    testOk1(pvt.count == 3);

    epicsEventSignal(pvt.jammer);
    errlogFlush();

    testOk1(pvt.count == 4);

    testDiag("Find buffer capacity (%u theoretical)",LOGBUFSIZE);

    pvt.checkLen = 0;

    for (mlen = 8; mlen <= 255; mlen *= 2) {
        double eff;
        char save = msg[mlen - 1];

        N = LOGBUFSIZE / mlen; /* # of of messages to send */
        msg[mlen - 1] = '\0';
        pvt.count = 0;
        /* pvt.checkLen = mlen - 1; */

        pvt.jam = 1;

        for (i = 0; i < N; i++) {
            errlogPrintfNoConsole("%s", msg);
        }

        epicsEventSignal(pvt.jammer);
        errlogFlush();

        eff = (double) (pvt.count * mlen) / LOGBUFSIZE * 100.0;
        testDiag(" For %d messages of length %d got %u (%.1f%% efficient)",
                 (int) N, (int) mlen, pvt.count, eff);

        msg[mlen - 1] = save;
        N = pvt.count;  /* Save final count for the test below */

        /* Clear "errlog: <n> messages were discarded" status */
        pvt.checkLen = 0;
        errlogPrintfNoConsole(".");
        errlogFlush();
    }

    testDiag("Checking buffer use after partial flush");

    /* Use the numbers from the largest block size above */
    mlen /= 2;
    msg[mlen - 1] = '\0';

    pvt.jam = 1;
    pvt.count = 0;

    testDiag("Filling with %d messages of size %d", (int) N, (int) mlen);

    for (i = 0; i < N; i++) {
        errlogPrintfNoConsole("%s", msg);
    }
    epicsThreadSleep(0.1); /* should really be a second Event */

    testOk1(pvt.count == 0);

    /* Extract the first 2 messages, 2*(sizeof(msgNode) + 128) bytes */
    pvt.jam = -2;
    epicsEventSignal(pvt.jammer);
    epicsThreadSleep(0.1);

    testDiag("Drained %u messages", pvt.count);
    testOk1(pvt.count == 2);

    /* The buffer has space for 1 more message: sizeof(msgNode) + 256 bytes */
    errlogPrintfNoConsole("%s", msg); /* Use up that space */

    testDiag("Overflow the buffer");
    errlogPrintfNoConsole("%s", msg);

    testOk1(pvt.count == 2);

    epicsEventSignal(pvt.jammer); /* Empty */
    errlogFlush();

    testDiag("Logged %u messages", pvt.count);
    testOk1(pvt.count == N+1);

    /* Clean up */
    errlogRemoveListener(&logClient);

    return testDone();
}
