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
#include <epicsMessageQueue.h>
#include <epicsThread.h>
#include <epicsAssert.h>

const char *msg1 = "1234567890This is a very long message.";

static void
receiver(void *arg)
{
    epicsMessageQueue *q = (epicsMessageQueue *)arg;
    char cbuf[80];
    int expectmsg[4];
    unsigned int len;
    int sender, msgNum;

    for (sender = 1 ; sender <= 4 ; sender++)
        expectmsg[sender-1] = 1;
    for (;;) {
        cbuf[0] = '\0';
        q->receive(cbuf, &len);
        if ((sscanf(cbuf, "Sender %d -- %d", &sender, &msgNum) == 2)
         && (sender >= 1)
         && (sender <= 4)) {
            if (expectmsg[sender-1] != msgNum)
                printf("%s received %d '%.*s' -- expected %d\n", epicsThreadGetNameSelf(), len, len, cbuf, expectmsg[sender-1]);
            expectmsg[sender-1] = msgNum + 1;
            epicsThreadSleep(0.001 * (random() % 20));
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
    unsigned int len;
    int i = 0;

    for (;;) {
        len = sprintf(cbuf, "%s -- %d.", epicsThreadGetNameSelf(), ++i);
        while (q->send((void *)cbuf, len) == false)
            epicsThreadSleep(0.005 * (random() % 5));
        epicsThreadSleep(0.005 * (random() % 20));
    }
}

void
epicsMessageQueueTest()
{
    int i;
    char cbuf[80];
    unsigned int len;
    int pass;
    int used;
    unsigned int want;

    epicsMessageQueue *q1 = new epicsMessageQueue(4, 20);

    /*
     * Simple single-thread tests
     */
    i = 0;
    used = 0;
    assert(q1->isEmpty());
    assert(!q1->isFull());
    while (q1->send((void *)msg1, i ) == true) {
        i++;
        printf("Should have %d used -- ", ++used);
        assert(!q1->isEmpty());
        q1->show();
    }
    assert(!q1->isEmpty());
    assert(q1->isFull());

    want = 0;
    q1->receive(cbuf, &len);
    assert(!q1->isEmpty());
    assert(!q1->isFull());
    if ((len != want) || (strncmp(msg1, cbuf, len) != 0))
        printf("wanted:%d '%.*s'   got:%d '%.*s'\n", want, want, msg1, len, len, cbuf);
    printf("Should have %d used -- ", --used);
    q1->show();

    want++;
    q1->receive(cbuf, &len);
    if ((len != want) || (strncmp(msg1, cbuf, len) != 0))
        printf("wanted:%d '%.*s'   got:%d '%.*s'\n", want, want, msg1, len, len, cbuf);
    printf("Should have %d used -- ", --used);
    q1->show();
    q1->send((void *)msg1, i++);
    printf("Should have %d used -- ", ++used);
    q1->show();

    want++;
    q1->receive(cbuf, &len);
    if ((len != want) || (strncmp(msg1, cbuf, len) != 0))
        printf("wanted:%d '%.*s'   got:%d '%.*s'\n", want, want, msg1, len, len, cbuf);
    printf("Should have %d used -- ", --used);
    q1->show();
    q1->send((void *)msg1, i++);
    printf("Should have %d used -- ", ++used);
    q1->show();

    while (q1->receive(cbuf, &len, 1.0) == true) {
        printf("Should have %d used -- ", --used);
        q1->show();
        want++;
        if ((len != want) || (strncmp(msg1, cbuf, len) != 0))
            printf("wanted:%d '%.*s'   got:%d '%.*s'\n", want, want, msg1, len, len, cbuf);
    }
    assert(q1->isEmpty());
    assert(!q1->isFull());

    /*
     * Single receiver, single sender tests
     */
    epicsThreadCreate("Receiver one", epicsThreadPriorityMedium, epicsThreadStackMedium, receiver, q1);
    for (pass = 1 ; pass <= 3 ; pass++) {
        epicsThreadSetPriority(epicsThreadGetIdSelf(), pass == 1 ?
                                                        epicsThreadPriorityHigh :
                                                        epicsThreadPriorityLow);
        switch (pass) {
        case 1: printf ("Should send/receive only 4 messages (sender priority > receiver priority).\n"); break;
        case 2: printf ("Should send/receive 4 to 10 messages (depends on how host handles thread priorities).\n"); break;
        case 3: printf ("Should send/receive 10 messages (sender pauses after sending).\n"); break;
        }
        for (i = 0 ; i < 10 ; i++) {
            if (q1->send((void *)msg1, i) == false)
                break;
            if (pass >= 3)
                epicsThreadSleep(0.5);
        }
        printf ("Sent %d messages.\n", i);
        epicsThreadSleep(1.0);
    }

    /*
     * Single receiver, multiple sender tests
     */
    printf("This test takes another 5 minutes to finish.\n");
    printf("Test has succeeded if nothing appears between here....\n");
    epicsThreadCreate("Sender 1", epicsThreadPriorityLow, epicsThreadStackMedium, sender, q1);
    epicsThreadCreate("Sender 2", epicsThreadPriorityMedium, epicsThreadStackMedium, sender, q1);
    epicsThreadCreate("Sender 3", epicsThreadPriorityHigh, epicsThreadStackMedium, sender, q1);
    epicsThreadCreate("Sender 4", epicsThreadPriorityHigh, epicsThreadStackMedium, sender, q1);

    epicsThreadSleep(300.0);

    /*
     * Force out summaries
     */
    printf("......and here.\n");
    q1->send((void *)msg1, 0);
    epicsThreadSleep(1.0);
}
