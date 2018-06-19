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
#include "iocLog.h"
#include "logClient.h"
#include "envDefs.h"
#include "osiSock.h"
#include "fdmgr.h"

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

static void testLogPrefix(void);
static void acceptNewClient( void *pParam );
static void readFromClient( void *pParam );
static void testPrefixLogandCompare( const char* logmessage);

static void *pfdctx;
static SOCKET sock;
static SOCKET insock;

static const char* prefixactualmsg[]= {
                     "A message without prefix",
                     "A message with prefix",
                     "DONE"
                     };
static const char *prefixstring = "fac=LI21 ";
static const char prefixexpectedmsg[] = "A message without prefix"
                     "fac=LI21 A message with prefix"
                     "fac=LI21 DONE"
                     ;
static char prefixmsgbuffer[1024];


static
void testEqInt_(int lhs, int rhs, const char *LHS, const char *RHS)
{
    testOk(lhs==rhs, "%s (%d) == %s (%d)", LHS, lhs, RHS, rhs);
}
#define testEqInt(L, R) testEqInt_(L, R, #L, #R);
static
void logClient(void* raw, const char* msg)
{
    clientPvt *pvt = raw;
    size_t len;
    char show[46];

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

    len = strlen(msg);
    if (len > 45) {
        /* Only show start and end of long messages */
        strncpy(show, msg, 20);
        show[20] = 0;
        strcat(show + 20, " ... ");
        strcat(show + 25, msg + len - 20);
    }
    else {
        strcpy(show, msg);
    }

    if (pvt->checkLen)
        if (!testOk(pvt->checkLen == len, "Received %d chars", (int) len)) {
            testDiag("Expected %d", (int) pvt->checkLen);
            if (!pvt->expect)
                testDiag("Message is \"%s\"", show);
        }

    if (pvt->expect) {
        if (!testOk(strcmp(pvt->expect, msg) == 0, "Message is \"%s\"", show)) {
            len = strlen(pvt->expect);
            if (len > 45) {
                testDiag("Expected \"%.20s ... %s\"",
                    pvt->expect, pvt->expect + len - 20);
            }
            else {
                testDiag("Expected \"%s\"", pvt->expect);
            }
        }
    }

    pvt->count++;
}

MAIN(epicsErrlogTest)
{
    size_t mlen, i, N;
    char msg[256];
    clientPvt pvt, pvt2;

    testPlan(32);

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

    testEqInt(pvt.count, 1);

    errlogAddListener(&logClient, &pvt2);

    pvt2.expect = pvt.expect = "Testing2";
    pvt2.checkLen = pvt.checkLen = strlen(pvt.expect);

    errlogPrintfNoConsole("%s", pvt.expect);
    errlogFlush();

    testEqInt(pvt.count, 2);
    testEqInt(pvt2.count, 1);

    /* Removes the first listener */
    testOk(1 == errlogRemoveListeners(&logClient, &pvt),
        "Removed 1 listener");

    pvt2.expect = "Testing3";
    pvt2.checkLen = strlen(pvt2.expect);

    errlogPrintfNoConsole("%s", pvt2.expect);
    errlogFlush();

    testEqInt(pvt.count, 2);
    testEqInt(pvt2.count, 2);

    /* Add the second listener again, then remove both instances */
    errlogAddListener(&logClient, &pvt2);
    testOk(2 == errlogRemoveListeners(&logClient, &pvt2),
        "Removed 2 listeners");

    errlogPrintfNoConsole("Something different");
    errlogFlush();

    testEqInt(pvt.count, 2);
    testEqInt(pvt2.count, 2);

    /* Re-add one listener */
    errlogAddListener(&logClient, &pvt);

    testDiag("Check truncation");

    pvt.expect = truncmsg;
    pvt.checkLen = 255;

    errlogPrintfNoConsole("%s", longmsg);
    errlogFlush();

    testEqInt(pvt.count, 3);

    pvt.expect = NULL;

    testDiag("Check priority");
    /* For the following tests it is important that
     * the buffer should not flush until we request it
     */
    pvt.jam = 1;

    errlogPrintfNoConsole("%s", longmsg);
    epicsThreadSleep(0.1);

    testEqInt(pvt.count, 3);

    epicsEventSignal(pvt.jammer);
    errlogFlush();

    testEqInt(pvt.count, 4);

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

    testEqInt(pvt.count, 0);

    /* Extract the first 2 messages, 2*(sizeof(msgNode) + 128) bytes */
    pvt.jam = -2;
    epicsEventSignal(pvt.jammer);
    epicsThreadSleep(0.1);

    testDiag("Drained %u messages", pvt.count);
    testEqInt(pvt.count, 2);

    /* The buffer has space for 1 more message: sizeof(msgNode) + 256 bytes */
    errlogPrintfNoConsole("%s", msg); /* Use up that space */

    testDiag("Overflow the buffer");
    errlogPrintfNoConsole("%s", msg);

    testEqInt(pvt.count, 2);

    epicsEventSignal(pvt.jammer); /* Empty */
    errlogFlush();

    testDiag("Logged %u messages", pvt.count);
    testEqInt(pvt.count, N+1);

    /* Clean up */
    testOk(1 == errlogRemoveListeners(&logClient, &pvt),
        "Removed 1 listener");

    testLogPrefix();

    return testDone();
}
/*
 * Tests the log prefix code
 * The prefix is only applied to log messages as they go out to the socket,
 * so we need to create a server listening on a port, accept connections etc.
 * This code is a reduced version of the code in iocLogServer.
 */
static void testLogPrefix(void) {
    struct sockaddr_in serverAddr;
    int status;
    struct timeval timeout;
    struct sockaddr_in actualServerAddr;
    osiSocklen_t actualServerAddrSize;
    char portstring[16];


    testDiag("Testing iocLogPrefix");

    timeout.tv_sec = 5; /* in seconds */
    timeout.tv_usec = 0;

    memset((void*)prefixmsgbuffer, 0, sizeof prefixmsgbuffer);

    /* Clear "errlog: <n> messages were discarded" status */
    errlogPrintfNoConsole(".");
    errlogFlush();

    sock = epicsSocketCreate(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        testAbort("epicsSocketCreate failed.");
    }

    /* We listen on a an available port. */
    memset((void *)&serverAddr, 0, sizeof serverAddr);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(0);

    status = bind (sock, (struct sockaddr *)&serverAddr,
                   sizeof (serverAddr) );
    if (status < 0) {
        testAbort("bind failed; all ports in use?");
    }

    status = listen(sock, 10);
    if (status < 0) {
        testAbort("listen failed!");
    }

    /* Determine the port that the OS chose */
    actualServerAddrSize = sizeof actualServerAddr;
    memset((void *)&actualServerAddr, 0, sizeof serverAddr);
    status = getsockname(sock, (struct sockaddr *) &actualServerAddr,
         &actualServerAddrSize);
    if (status < 0) {
        testAbort("Can't find port number!");
    }

    sprintf(portstring, "%d", ntohs(actualServerAddr.sin_port));
    testDiag("Listening on port %s", portstring);

    /* Set the EPICS environment variables for logging. */
    epicsEnvSet ( "EPICS_IOC_LOG_INET", "localhost" );
    epicsEnvSet ( "EPICS_IOC_LOG_PORT", portstring );

    pfdctx = (void *) fdmgr_init();
    if (status < 0) {
        testAbort("fdmgr_init failed!");
    }

    status = fdmgr_add_callback(pfdctx, sock, fdi_read,
        acceptNewClient, &serverAddr);

    if (status < 0) {
        testAbort("fdmgr_add_callback failed!");
    }

    testOk1(iocLogInit() == 0);
    fdmgr_pend_event(pfdctx, &timeout);

    testPrefixLogandCompare(prefixactualmsg[0]);

    iocLogPrefix(prefixstring);
    testPrefixLogandCompare(prefixactualmsg[1]);
    testPrefixLogandCompare(prefixactualmsg[2]);
    epicsSocketDestroy(sock);
}

static void testPrefixLogandCompare( const char* logmessage ) {
    struct timeval timeout;
    timeout.tv_sec = 5; /* in seconds */
    timeout.tv_usec = 0;

    errlogPrintfNoConsole("%s", logmessage);
    errlogFlush();
    iocLogFlush();
    fdmgr_pend_event(pfdctx, &timeout);
}

static void acceptNewClient ( void *pParam )
{
    osiSocklen_t addrSize;
    struct sockaddr_in addr;
    int status;

    addrSize = sizeof ( addr );
    insock = epicsSocketAccept ( sock, (struct sockaddr *)&addr, &addrSize );
    testOk(insock != INVALID_SOCKET && addrSize >= sizeof (addr),
        "Accepted new client");

    status = fdmgr_add_callback(pfdctx, insock, fdi_read,
        readFromClient,  NULL);

    testOk(status >= 0, "Client read configured");
}

static void readFromClient(void *pParam)
{
    char recvbuf[1024];
    int recvLength;

    memset(recvbuf, 0, 1024);
    recvLength = recv(insock, recvbuf, 1024, 0);
    if (recvLength > 0) {
        strcat(prefixmsgbuffer, recvbuf);

        /* If we have received all of the messages. */
        if (strstr(prefixmsgbuffer, "DONE") != NULL) {
            size_t msglen = strlen(prefixexpectedmsg);
            int prefixcmp = strncmp(prefixexpectedmsg, prefixmsgbuffer, msglen);

            if (!testOk(prefixcmp == 0, "prefix matches")) {
                testDiag("Expected '%s'\n", prefixexpectedmsg);
                testDiag("Obtained '%s'\n", prefixmsgbuffer);
            }
        }
    }
}
