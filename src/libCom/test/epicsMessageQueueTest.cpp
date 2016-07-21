/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
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
static epicsEventId finished;
static unsigned int mediumStack;

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
    epicsEventSignal(finished);
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

extern "C" void messageQueueTest(void *parm)
{
    epicsThreadId myThreadId = epicsThreadGetIdSelf();
    unsigned int i;
    char cbuf[80];
    int len;
    int pass;
    int want;

    epicsMessageQueue *q1 = new epicsMessageQueue(4, 20);

    testDiag("Simple single-thread tests:");
    i = 0;
    testOk1(q1->pending() == 0);
    while (q1->trySend((void *)msg1, i ) == 0) {
        i++;
        testOk(q1->pending() == i, "q1->pending() == %d", i);
    }
    testOk1(q1->pending() == 4);

    want = 0;
    len = q1->receive(cbuf, sizeof cbuf);
    testOk1(q1->pending() == 3);
    if (!testOk1((len == want) && (strncmp(msg1, cbuf, len) == 0)))
        testDiag("wanted:%d '%.*s'   got:%d '%.*s'",
            want, want, msg1, len, len, cbuf);

    want++;
    len = q1->receive(cbuf, sizeof cbuf);
    testOk1(q1->pending() == 2);
    if (!testOk1((len == want) && (strncmp(msg1, cbuf, len) == 0)))
        testDiag("wanted:%d '%.*s'   got:%d '%.*s'",
            want, want, msg1, len, len, cbuf);
    q1->trySend((void *)msg1, i++);

    want++;
    len = q1->receive(cbuf, sizeof cbuf);
    testOk1(q1->pending() == 2);
    if (!testOk1((len == want) && (strncmp(msg1, cbuf, len) == 0)))
        testDiag("wanted:%d '%.*s'   got:%d '%.*s'",
            want, want, msg1, len, len, cbuf);
    q1->trySend((void *)msg1, i++);
    testOk1(q1->pending() == 3);

    i = 3;
    while ((len = q1->receive(cbuf, sizeof cbuf, 1.0)) >= 0) {
        --i;
        testOk(q1->pending() == i, "q1->pending() == %d", i);
        want++;
        if (!testOk1((len == want) & (strncmp(msg1, cbuf, len) == 0)))
            testDiag("wanted:%d '%.*s'   got:%d '%.*s'",
                want, want, msg1, len, len, cbuf);
    }
    testOk1(q1->pending() == 0);

    testDiag("Test sender timeout:");
    i = 0;
    testOk1(q1->pending() == 0);
    while (q1->send((void *)msg1, i, 1.0 ) == 0) {
        i++;
        testOk(q1->pending() == i, "q1->pending() == %d", i);
    }
    testOk1(q1->pending() == 4);

    want = 0;
    len = q1->receive(cbuf, sizeof cbuf);
    testOk1(q1->pending() == 3);
    if (!testOk1((len == want) && (strncmp(msg1, cbuf, len) == 0)))
        testDiag("wanted:%d '%.*s'   got:%d '%.*s'",
            want, want, msg1, len, len, cbuf);

    want++;
    len = q1->receive(cbuf, sizeof cbuf);
    testOk1(q1->pending() == 2);
    if (!testOk1((len == want) && (strncmp(msg1, cbuf, len) == 0)))
        testDiag("wanted:%d '%.*s'   got:%d '%.*s'",
            want, want, msg1, len, len, cbuf);
    q1->send((void *)msg1, i++, 1.0);

    want++;
    len = q1->receive(cbuf, sizeof cbuf);
    testOk1(q1->pending() == 2);
    if (!testOk1((len == want) && (strncmp(msg1, cbuf, len) == 0)))
        testDiag("wanted:%d '%.*s'   got:%d '%.*s'",
            want, want, msg1, len, len, cbuf);
    q1->send((void *)msg1, i++, 1.0);
    testOk1(q1->pending() == 3);

    i = 3;
    while ((len = q1->receive(cbuf, sizeof cbuf, 1.0)) >= 0) {
        --i;
        testOk(q1->pending() == i, "q1->pending() == %d", i);
        want++;
        if (!testOk1((len == want) && (strncmp(msg1, cbuf, len) == 0)))
            testDiag("wanted:%d '%.*s'   got:%d '%.*s'",
                want, want, msg1, len, len, cbuf);
    }
    testOk1(q1->pending() == 0);

    testDiag("Test receiver with timeout:");
    for (i = 0 ; i < 4 ; i++)
        testOk1 (q1->send((void *)msg1, i, 1.0) == 0);
    testOk1(q1->pending() == 4);
    for (i = 0 ; i < 4 ; i++)
        testOk(q1->receive((void *)cbuf, sizeof cbuf, 1.0) == (int)i,
            "q1->receive(...) == %d", i);
    testOk1(q1->pending() == 0);
    testOk1(q1->receive((void *)cbuf, sizeof cbuf, 1.0) < 0);
    testOk1(q1->pending() == 0);

    testDiag("Single receiver with invalid size, single sender tests:");
    epicsThreadCreate("Bad Receiver", epicsThreadPriorityMedium,
        mediumStack, badReceiver, q1);
    epicsThreadSleep(1.0);
    testOk(q1->send((void *)msg1, 10) == 0, "Send with waiting receiver");
    epicsThreadSleep(2.0);
    testOk(q1->send((void *)msg1, 10) == 0, "Send with no receiver");
    epicsThreadSleep(2.0);

    testDiag("Single receiver, single sender tests:");
    epicsThreadSetPriority(myThreadId, epicsThreadPriorityHigh);
    epicsThreadCreate("Receiver one", epicsThreadPriorityMedium,
        mediumStack, receiver, q1);
    for (pass = 1 ; pass <= 3 ; pass++) {
        for (i = 0 ; i < 10 ; i++) {
            if (q1->trySend((void *)msg1, i) < 0)
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
    testDiag("This test lasts 60 seconds...");
    testOk(!!epicsThreadCreate("Sender 1", epicsThreadPriorityLow,
        mediumStack, sender, q1),
        "Created Sender 1");
    testOk(!!epicsThreadCreate("Sender 2", epicsThreadPriorityMedium,
        mediumStack, sender, q1),
        "Created Sender 2");
    testOk(!!epicsThreadCreate("Sender 3", epicsThreadPriorityHigh,
        mediumStack, sender, q1),
        "Created Sender 3");
    testOk(!!epicsThreadCreate("Sender 4", epicsThreadPriorityHigh,
        mediumStack, sender, q1),
        "Created Sender 4");

    for (i = 0; i < 10; i++) {
        testDiag("... %2d", 10 - i);
        epicsThreadSleep(6.0);
    }

    sendExit = 1;
    epicsThreadSleep(1.0);
    recvExit = 1;
    testDiag("Scheduler exiting");
}

MAIN(epicsMessageQueueTest)
{
    testPlan(62);

    finished = epicsEventMustCreate(epicsEventEmpty);
    mediumStack = epicsThreadGetStackSize(epicsThreadStackMedium);

    epicsThreadCreate("messageQueueTest", epicsThreadPriorityMedium,
        mediumStack, messageQueueTest, NULL);

    epicsEventMustWait(finished);
    testDiag("Main thread signalled");
    epicsThreadSleep(1.0);

    return testDone();
}
