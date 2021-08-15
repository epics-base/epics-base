/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 *      Author  W. Eric Norum
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "epicsMessageQueue.h"
#include "epicsThread.h"
#include "epicsExit.h"
#include "epicsEvent.h"
#include "epicsAssert.h"
#include "epicsUnitTest.h"
#include "testMain.h"

static const char *msg1 = "1234567890This is a very long message.";
static volatile int sendExit = 0;
static volatile int recvExit = 0;

#define SLEEPY_TESTS 500
static int numSent, numReceived;

/*
 * In Numerical Recipes in C: The Art of Scientific Computing (William  H.
 * Press, Brian P. Flannery, Saul A. Teukolsky, William T. Vetterling; New
 * York: Cambridge University Press, 1992 (2nd ed., p. 277)), the  follow-
 * ing comments are made:
 * "If  you want to generate a random integer between 1 and 10, you
 * should always do it by using high-order bits, as in
 *      j=1+(int) (10.0*rand()/(RAND_MAX+1.0));
 * and never by anything resembling
 *      j=1+(rand() % 10);
 */
static int
randBelow(int n)
{
    return (int)((double)n*rand()/(RAND_MAX+1.0));
}

extern "C" void
badReceiver(void *arg)
{
    epicsMessageQueue *q = (epicsMessageQueue *)arg;
    char cbuf[80];
    int len;

    cbuf[0] = '\0';
    len = q->receive(cbuf, 1);
    if (len < 0 && cbuf[0] == '\0')
        testPass("receive into undersized buffer returned error (%d)", len);
    else if (len == 1 && cbuf[0] == *msg1)
        testPass("receive into undersized buffer truncated message");
    else
        testFail("receive into undersized buffer returned %d", len);

    epicsThreadSleep(3.0);

    cbuf[0] = '\0';
    len = q->receive(cbuf, 1);
    if (len < 0 && cbuf[0] == '\0')
        testPass("receive into undersized buffer returned error (%d)", len);
    else if (len == 1 && cbuf[0] == *msg1)
        testPass("receive into undersized buffer truncated message");
    else
        testFail("receive into undersized buffer returned %d", len);
}

extern "C" void
receiver(void *arg)
{
    epicsMessageQueue *q = (epicsMessageQueue *)arg;
    const char *myName = epicsThreadGetNameSelf();
    char cbuf[80];
    int expectmsg[4];
    int len;
    int sender, msgNum;
    int errors = 0;

    for (sender = 1 ; sender <= 4 ; sender++)
        expectmsg[sender-1] = 1;
    while (!recvExit) {
        cbuf[0] = '\0';
        len = q->receive(cbuf, sizeof cbuf, 5.0);
        if (len < 0 && !recvExit) {
            testDiag("receiver() received unexpected timeout");
            ++errors;
        }
        else if (sscanf(cbuf, "Sender %d -- %d", &sender, &msgNum) == 2 &&
                 sender >= 1 && sender <= 4) {
            if (expectmsg[sender-1] != msgNum) {
                ++errors;
                testDiag("%s received %d '%.*s' -- expected %d",
                    myName, len, len, cbuf, expectmsg[sender-1]);
            }
            expectmsg[sender-1] = msgNum + 1;
            epicsThreadSleep(0.001 * (randBelow(20)));
        }
    }
    for (sender = 1 ; sender <= 4 ; sender++) {
        if (expectmsg[sender-1] > 1)
            testDiag("Received %d messages from Sender %d",
                expectmsg[sender-1]-1, sender);
    }
    if (!testOk1(errors == 0))
        testDiag("Error count was %d", errors);
    testDiag("%s exiting", myName);
}

extern "C" void
fastReceiver(void *arg)
{
    epicsMessageQueue *q = (epicsMessageQueue *)arg;
    char cbuf[80];
    int len;
    numReceived = 0;
    while (!recvExit) {
        len = q->receive(cbuf, sizeof cbuf, 0.010);
        if (len > 0) {
            numReceived++;
        }
    }
    recvExit = 0;
}

void sleepySender(double delay)
{
    epicsThreadOpts opts = {epicsThreadPriorityMedium, epicsThreadStackMedium, 1};
    epicsThreadId rxThread;

    testDiag("sleepySender: sending every %.3f seconds", delay);
    epicsMessageQueue q(4, 20);
    rxThread = epicsThreadCreateOpt("Fast Receiver", fastReceiver, &q, &opts);
    if (!rxThread)
        testAbort("Task create failed");

    numSent = 0;
    for (int i = 0 ; i < SLEEPY_TESTS ; i++) {
        if (q.send((void *)msg1, 4) == 0) {
            numSent++;
        }
        epicsThreadSleep(delay);
    }
    epicsThreadSleep(1.0);
    testOk(numSent == SLEEPY_TESTS, "Sent %d (should be %d)",
        numSent, SLEEPY_TESTS);
    testOk(numReceived == SLEEPY_TESTS, "Received %d (should be %d)",
        numReceived, SLEEPY_TESTS);

    recvExit = 1;
    while (q.send((void *)msg1, 4) != 0)
        epicsThreadSleep(0.01);
    epicsThreadMustJoin(rxThread);
}

extern "C" void
fastSender(void *arg)
{
    epicsMessageQueue *q = (epicsMessageQueue *)arg;
    numSent = 0;

    // Send first without timeout
    q->send((void *)msg1, 4);
    numSent++;

    // The rest have a timeout
    while (!sendExit) {
        if (q->send((void *)msg1, 4, 0.010) == 0) {
            numSent++;
        }
    }
    sendExit = 0;
}

void sleepyReceiver(double delay)
{
    epicsThreadOpts opts = {epicsThreadPriorityMedium,
        epicsThreadStackMedium, 1};
    epicsThreadId txThread;

    testDiag("sleepyReceiver: acquiring every %.3f seconds", delay);
    epicsMessageQueue q(4, 20);

    // Fill the queue
    for (int i = q.pending(); i < 4 ;i++) {
        q.send((void *)msg1, 4);
    }

    txThread = epicsThreadCreateOpt("Fast Sender", fastSender, &q, &opts);
    if (!txThread)
        testAbort("Task create failed");
    epicsThreadSleep(0.5);

    char cbuf[80];
    int len;
    numReceived = 0;

    for (int i = 0 ; i < SLEEPY_TESTS ; i++) {
        len = q.receive(cbuf, sizeof cbuf);
        if (len > 0) {
            numReceived++;
        }
        epicsThreadSleep(delay);
    }

    testOk(numSent == SLEEPY_TESTS, "Sent %d (should be %d)",
        numSent, SLEEPY_TESTS);
    testOk(numReceived == SLEEPY_TESTS, "Received %d (should be %d)",
        numReceived, SLEEPY_TESTS);

    sendExit = 1;
    while (q.receive(cbuf, sizeof cbuf) <= 0)
        epicsThreadSleep(0.01);
    epicsThreadMustJoin(txThread);
}

extern "C" void
sender(void *arg)
{
    epicsMessageQueue *q = (epicsMessageQueue *)arg;
    char cbuf[80];
    int len;
    int i = 0;

    while (!sendExit) {
        len = sprintf(cbuf, "%s -- %d.", epicsThreadGetNameSelf(), ++i);
        while (q->trySend((void *)cbuf, len) < 0)
            epicsThreadSleep(0.005 * (randBelow(5)));
        epicsThreadSleep(0.005 * (randBelow(20)));
    }
    testDiag("%s exiting, sent %d messages", epicsThreadGetNameSelf(), i);
}

#define NUM_SENDERS 4
extern "C" void messageQueueTest(void *parm)
{
    epicsThreadId myThreadId = epicsThreadGetIdSelf();
    epicsThreadId rxThread;
    epicsThreadId senderId[NUM_SENDERS];
    epicsThreadOpts opts = {epicsThreadPriorityMedium,
        epicsThreadStackMedium, 1};

    unsigned int i;
    char cbuf[80];
    int len;
    int pass;
    int want;

    epicsMessageQueue q1(4, 20);

    testDiag("Simple single-thread tests:");
    testOk1(q1.pending() == 0);
    for (i = 0; i < 4;) {
        int ret = q1.trySend((void *)msg1, i++);
        testOk(ret == 0, "trySend succeeded (%d == 0)", ret);
        testOk(q1.pending() == i, "loop: q1.pending() == %d", i);
    }
    testOk1(q1.pending() == 4);

    want = 0;
    len = q1.receive(cbuf, sizeof cbuf);
    testOk1(q1.pending() == 3);
    if (!testOk1((len == want) && (strncmp(msg1, cbuf, len) == 0)))
        testDiag("wanted:%d '%.*s'   got:%d '%.*s'",
            want, want, msg1, len, len, cbuf);

    want++;
    len = q1.receive(cbuf, sizeof cbuf);
    testOk1(q1.pending() == 2);
    if (!testOk1((len == want) && (strncmp(msg1, cbuf, len) == 0)))
        testDiag("wanted:%d '%.*s'   got:%d '%.*s'",
            want, want, msg1, len, len, cbuf);
    q1.trySend((void *)msg1, i++);

    want++;
    len = q1.receive(cbuf, sizeof cbuf);
    testOk1(q1.pending() == 2);
    if (!testOk1((len == want) && (strncmp(msg1, cbuf, len) == 0)))
        testDiag("wanted:%d '%.*s'   got:%d '%.*s'",
            want, want, msg1, len, len, cbuf);
    q1.trySend((void *)msg1, i++);
    testOk1(q1.pending() == 3);

    i = 3;
    while ((len = q1.receive(cbuf, sizeof cbuf, 1.0)) >= 0) {
        --i;
        testOk(q1.pending() == i, "loop: q1.pending() == %d", i);
        want++;
        if (!testOk1((len == want) & (strncmp(msg1, cbuf, len) == 0)))
            testDiag("wanted:%d '%.*s'   got:%d '%.*s'",
                want, want, msg1, len, len, cbuf);
    }
    testOk1(q1.pending() == 0);

    testDiag("Test sender timeout:");
    i = 0;
    testOk1(q1.pending() == 0);
    while (q1.send((void *)msg1, i, 1.0 ) == 0) {
        i++;
        testOk(q1.pending() == i, "loop: q1.pending() == %d", i);
    }
    testOk1(q1.pending() == 4);

    want = 0;
    len = q1.receive(cbuf, sizeof cbuf);
    testOk1(q1.pending() == 3);
    if (!testOk1((len == want) && (strncmp(msg1, cbuf, len) == 0)))
        testDiag("wanted:%d '%.*s'   got:%d '%.*s'",
            want, want, msg1, len, len, cbuf);

    want++;
    len = q1.receive(cbuf, sizeof cbuf);
    testOk1(q1.pending() == 2);
    if (!testOk1((len == want) && (strncmp(msg1, cbuf, len) == 0)))
        testDiag("wanted:%d '%.*s'   got:%d '%.*s'",
            want, want, msg1, len, len, cbuf);
    q1.send((void *)msg1, i++, 1.0);

    want++;
    len = q1.receive(cbuf, sizeof cbuf);
    testOk1(q1.pending() == 2);
    if (!testOk1((len == want) && (strncmp(msg1, cbuf, len) == 0)))
        testDiag("wanted:%d '%.*s'   got:%d '%.*s'",
            want, want, msg1, len, len, cbuf);
    q1.send((void *)msg1, i++, 1.0);
    testOk1(q1.pending() == 3);

    i = 3;
    while ((len = q1.receive(cbuf, sizeof cbuf, 1.0)) >= 0) {
        --i;
        testOk(q1.pending() == i, "loop: q1.pending() == %d", i);
        want++;
        if (!testOk1((len == want) && (strncmp(msg1, cbuf, len) == 0)))
            testDiag("wanted:%d '%.*s'   got:%d '%.*s'",
                want, want, msg1, len, len, cbuf);
    }
    testOk1(q1.pending() == 0);

    testDiag("Test receiver with timeout:");
    for (i = 0 ; i < 4 ; i++)
        testOk1 (q1.send((void *)msg1, i, 1.0) == 0);
    testOk1(q1.pending() == 4);
    for (i = 0 ; i < 4 ; i++)
        testOk(q1.receive((void *)cbuf, sizeof cbuf, 1.0) == (int)i,
            "q1.receive(...) == %d", i);
    testOk1(q1.pending() == 0);
    testOk1(q1.receive((void *)cbuf, sizeof cbuf, 1.0) < 0);
    testOk1(q1.pending() == 0);

    testDiag("Single receiver with invalid size, single sender tests:");
    rxThread = epicsThreadCreateOpt("Bad Receiver", badReceiver, &q1, &opts);
    if (!rxThread)
        testAbort("epicsThreadCreate failed");
    epicsThreadSleep(1.0);
    testOk(q1.send((void *)msg1, 10) == 0, "Send with waiting receiver");
    epicsThreadSleep(2.0);
    testOk(q1.send((void *)msg1, 10) == 0, "Send with no receiver");
    epicsThreadMustJoin(rxThread);

    testDiag("6 Single receiver single sender 'Sleepy timeout' tests,");
    testDiag("    these should take about %.2f seconds each:",
        SLEEPY_TESTS * 0.010);

    sleepySender(0.009);
    sleepySender(0.010);
    sleepySender(0.011);
    sleepyReceiver(0.009);
    sleepyReceiver(0.010);
    sleepyReceiver(0.011);

    testDiag("Single receiver, single sender tests:");
    epicsThreadSetPriority(myThreadId, epicsThreadPriorityHigh);
    rxThread = epicsThreadCreateOpt("Receiver one", receiver, &q1, &opts);
    if (!rxThread)
        testAbort("epicsThreadCreate failed");
    for (pass = 1 ; pass <= 3 ; pass++) {
        for (i = 0 ; i < 10 ; i++) {
            if (q1.trySend((void *)msg1, i) < 0)
                break;
            if (pass >= 3)
                epicsThreadSleep(0.5);
        }
        switch (pass) {
        case 1:
            if (i<6)
                testDiag("  priority-based scheduler, sent %d messages", i);
            epicsThreadSetPriority(myThreadId, epicsThreadPriorityLow);
            break;
        case 2:
            if (i<10)
                testDiag("  scheduler not strict priority, sent %d messages", i);
            else
                testDiag("  strict priority scheduler, sent 10 messages");
            break;
        case 3:
            testOk(i == 10, "%d of 10 messages sent with sender pauses", i);
            break;
        }
        epicsThreadSleep(1.0);
    }

    /*
     * Single receiver, multiple sender tests
     */
    testDiag("Single receiver, multiple sender tests:");
    testDiag("This test lasts 30 seconds...");
    for (i=0; i<NUM_SENDERS; i++) {
        char name[16];
        const int pri[NUM_SENDERS] = {
            epicsThreadPriorityLow,
            epicsThreadPriorityMedium,
            epicsThreadPriorityHigh,
            epicsThreadPriorityHigh
        };
        sprintf(name, "Sender %d", i+1);
        opts.priority = pri[i];
        senderId[i] = epicsThreadCreateOpt(name, sender, &q1, &opts);
        if (!senderId[i])
            testAbort("epicsThreadCreate failed");
    }

    for (i = 0; i < 6; i++) {
        testDiag("... %2d", 6 - i);
        epicsThreadSleep(5.0);
    }

    sendExit = 1;
    epicsThreadSleep(1.0);
    for (i=0; i<NUM_SENDERS; i++) {
        epicsThreadMustJoin(senderId[i]);
    }
    recvExit = 1;
    epicsThreadMustJoin(rxThread);
}

MAIN(epicsMessageQueueTest)
{
    epicsThreadOpts opts = {
        epicsThreadPriorityMedium,
        epicsThreadStackMedium,
        1
    };
    epicsThreadId testThread;

    testPlan(70 + NUM_SENDERS);

    testThread = epicsThreadCreateOpt("messageQueueTest",
        messageQueueTest, NULL, &opts);
    if (!testThread)
        testAbort("epicsThreadCreate failed");

    epicsThreadMustJoin(testThread);

    return testDone();
}
