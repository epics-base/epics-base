/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *      $Id$
 *
 *      Author  W. Eric Norum
 *              norume@aps.anl.gov
 *              630 252 4793
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <epicsMessageQueue.h>
#include <epicsThread.h>
#include <epicsAssert.h>

const char *msg1 = "1234567890This is a very long message.";

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

static void
receiver0(void *arg)
{
    epicsMessageQueue *q = (epicsMessageQueue *)arg;
    char cbuf[80];

    assert(q->receive(cbuf) == 5);
    assert(q->receive(cbuf) == 0);
    delete q;
}

static void
receiver(void *arg)
{
    epicsMessageQueue *q = (epicsMessageQueue *)arg;
    char cbuf[80];
    int expectmsg[4];
    int len;
    int sender, msgNum;

    for (sender = 1 ; sender <= 4 ; sender++)
        expectmsg[sender-1] = 1;
    for (;;) {
        cbuf[0] = '\0';
        len = q->receive(cbuf);
        if ((sscanf(cbuf, "Sender %d -- %d", &sender, &msgNum) == 2)
         && (sender >= 1)
         && (sender <= 4)) {
            if (expectmsg[sender-1] != msgNum)
                printf("%s received %d '%.*s' -- expected %d\n", epicsThreadGetNameSelf(), len, len, cbuf, expectmsg[sender-1]);
            expectmsg[sender-1] = msgNum + 1;
            epicsThreadSleep(0.001 * (randBelow(20)));
        }
        else {
            printf("%s received %d '%.*s'\n", epicsThreadGetNameSelf(), len, len, cbuf);
            for (sender = 1 ; sender <= 4 ; sender++) {
                if (expectmsg[sender-1] > 1)
                    printf("Sender %d -- %d messages\n", sender, expectmsg[sender-1]-1);
            }
        }
    }
}

static void
sender(void *arg)
{
    epicsMessageQueue *q = (epicsMessageQueue *)arg;
    char cbuf[80];
    int len;
    int i = 0;

    for (;;) {
        len = sprintf(cbuf, "%s -- %d.", epicsThreadGetNameSelf(), ++i);
        while (q->trySend((void *)cbuf, len) < 0)
            epicsThreadSleep(0.005 * (randBelow(5)));
        epicsThreadSleep(0.005 * (randBelow(20)));
    }
}

extern "C" void epicsMessageQueueTest()
{
    unsigned int i;
    char cbuf[80];
    int len;
    int pass;
    int used;
    int want;

    epicsMessageQueue *q1 = new epicsMessageQueue(4, 20);

    printf ("Simple single-thread tests.\n");
    i = 0;
    used = 0;
    assert(q1->pending() == 0);
    while (q1->trySend((void *)msg1, i ) == 0) {
        i++;
        assert(q1->pending() == i);
        printf("Should have %d used -- ", ++used);
        q1->show();
    }
    assert(q1->pending() == 4);

    want = 0;
    len = q1->receive(cbuf);
    assert(q1->pending() == 3);
    if ((len != want) || (strncmp(msg1, cbuf, len) != 0))
        printf("wanted:%d '%.*s'   got:%d '%.*s'\n", want, want, msg1, len, len, cbuf);

    want++;
    len = q1->receive(cbuf);
    assert(q1->pending() == 2);
    if ((len != want) || (strncmp(msg1, cbuf, len) != 0))
        printf("wanted:%d '%.*s'   got:%d '%.*s'\n", want, want, msg1, len, len, cbuf);
    q1->trySend((void *)msg1, i++);

    want++;
    len = q1->receive(cbuf);
    assert(q1->pending() == 2);
    if ((len != want) || (strncmp(msg1, cbuf, len) != 0))
        printf("wanted:%d '%.*s'   got:%d '%.*s'\n", want, want, msg1, len, len, cbuf);
    q1->trySend((void *)msg1, i++);
    assert(q1->pending() == 3);

    i = 3;
    while ((len = q1->receive(cbuf, 1.0)) >= 0) {
        assert(q1->pending() == --i);
        want++;
        if ((len != want) || (strncmp(msg1, cbuf, len) != 0))
            printf("wanted:%d '%.*s'   got:%d '%.*s'\n", want, want, msg1, len, len, cbuf);
    }
    assert(q1->pending() == 0);

    printf ("Test sender timeout.\n");
    i = 0;
    used = 0;
    assert(q1->pending() == 0);
    while (q1->send((void *)msg1, i, 1.0 ) == 0) {
        i++;
        assert(q1->pending() == i);
        printf("Should have %d used -- ", ++used);
        q1->show();
    }
    assert(q1->pending() == 4);

    want = 0;
    len = q1->receive(cbuf);
    assert(q1->pending() == 3);
    if ((len != want) || (strncmp(msg1, cbuf, len) != 0))
        printf("wanted:%d '%.*s'   got:%d '%.*s'\n", want, want, msg1, len, len, cbuf);

    want++;
    len = q1->receive(cbuf);
    assert(q1->pending() == 2);
    if ((len != want) || (strncmp(msg1, cbuf, len) != 0))
        printf("wanted:%d '%.*s'   got:%d '%.*s'\n", want, want, msg1, len, len, cbuf);
    q1->send((void *)msg1, i++, 1.0);

    want++;
    len = q1->receive(cbuf);
    assert(q1->pending() == 2);
    if ((len != want) || (strncmp(msg1, cbuf, len) != 0))
        printf("wanted:%d '%.*s'   got:%d '%.*s'\n", want, want, msg1, len, len, cbuf);
    q1->send((void *)msg1, i++, 1.0);
    assert(q1->pending() == 3);

    i = 3;
    while ((len = q1->receive(cbuf, 1.0)) >= 0) {
        assert(q1->pending() == --i);
        want++;
        if ((len != want) || (strncmp(msg1, cbuf, len) != 0))
            printf("wanted:%d '%.*s'   got:%d '%.*s'\n", want, want, msg1, len, len, cbuf);
    }
    assert(q1->pending() == 0);

    printf ("Test receiver with timeout.\n");
    for (i = 0 ; i < 4 ; i++)
        assert (q1->send((void *)msg1, i, 1.0) == 0);
    assert(q1->pending() == 4);
    for (i = 0 ; i < 4 ; i++)
        assert (q1->receive((void *)cbuf, 1.0) == (int)i);
    assert(q1->pending() == 0);
    assert (q1->receive((void *)cbuf, 1.0) < 0);
    assert(q1->pending() == 0);

    printf("Single receiver, single sender tests.\n");
    epicsThreadSetPriority(epicsThreadGetIdSelf(), epicsThreadPriorityHigh);
    epicsThreadCreate("Receiver one", epicsThreadPriorityMedium, epicsThreadGetStackSize(epicsThreadStackMedium), receiver, q1);
    for (pass = 1 ; pass <= 3 ; pass++) {
        switch (pass) {
        case 1: printf ("Should send/receive only 4 messages (sender priority > receiver priority).\n"); break;
        case 2: printf ("Should send/receive 5 to 10 messages (depends on how host handles thread priorities).\n"); break;
        case 3: printf ("Should send/receive 10 messages (sender pauses after sending).\n"); break;
        }
        for (i = 0 ; i < 10 ; i++) {
            if (q1->trySend((void *)msg1, i) < 0)
                break;
            if (pass >= 3)
                epicsThreadSleep(0.5);
        }
        printf ("Sent %d messages.\n", i);
        epicsThreadSetPriority(epicsThreadGetIdSelf(), epicsThreadPriorityLow);
        epicsThreadSleep(1.0);
    }

    /*
     * Single receiver, multiple sender tests
     */
    printf("Single receiver, multiple sender tests.\n");
    printf("This test takes 5 minutes to run.\n");
    printf("Test has succeeded if nothing appears between here....\n");
    epicsThreadCreate("Sender 1", epicsThreadPriorityLow, epicsThreadGetStackSize(epicsThreadStackMedium), sender, q1);
    epicsThreadCreate("Sender 2", epicsThreadPriorityMedium, epicsThreadGetStackSize(epicsThreadStackMedium), sender, q1);
    epicsThreadCreate("Sender 3", epicsThreadPriorityHigh, epicsThreadGetStackSize(epicsThreadStackMedium), sender, q1);
    epicsThreadCreate("Sender 4", epicsThreadPriorityHigh, epicsThreadGetStackSize(epicsThreadStackMedium), sender, q1);

    epicsThreadSleep(300.0);

    /*
     * Force out summaries
     */
    printf("......and here.\n");
    q1->trySend((void *)msg1, 0);
    epicsThreadSleep(1.0);
}
