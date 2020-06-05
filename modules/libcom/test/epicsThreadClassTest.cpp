/*************************************************************************\
* Copyright (c) 2020 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* epicsThreadClassTest.cpp */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "dbDefs.h"
#include "epicsAssert.h"
#include "epicsThread.h"
#include "epicsUnitTest.h"
#include "testMain.h"

/* Key to the char's that define the test case actions:
 *
 * Upper case letters are for parent thread actions
 *   B - Parent calls thread->start() and waits for child to start
 *   D - Parent deletes thread. This waits for child to return if it hasn't yet
 *   E - Parent calls thread->exitWait(), this may wait for child to return
 *   S - Parent sleeps for SLEEP_TIME seconds
 *   T - Parent sends sync trigger to child (w)
 *   W - Parent waits for sync trigger from child (t)
 *   X - Parent calls thread->exitWait(0)
 *
 * Lower case letters are for child thread actions
 *   d - Child deletes thread
 *   e - Child calls thread->exitWait()
 *   r - Child returns
 *   s - Child sleeps for SLEEP_TIME seconds
 *   t - Child sends sync trigger to parent (W)
 *   w - Child waits for sync trigger from child (T)
 *
 * Note that it is possible to write test cases that can hang,
 * segfault, or that trigger errors from thread APIs.
 */

// The test cases

const char * const cases[] = {
 // These cases don't start the thread:
    "D",        // Parent deletes thread
    "ED",       // Parent does exitWait(), deletes thread

 // In these cases the parent deletes the thread
    "BrSD",     // Child returns; parent deletes thread
    "BsDr",     // Parent deletes thread; child returns
    "BrSED",    // Child returns; parent does exitWait(), deletes thread
    "BsErD",    // Parent does exitWait(); child returns; parent deletes thread
    "BsXDr",    // Parent does exitWait(0); parent deletes thread; child returns
    "BwXTDsr",  // Parent does exitWait(0); parent deletes thread; child returns
 // These are currently broken
//  "BetWSrD",  // Child does exitWait(); sync; child returns; parent deletes thread
//  "BetWsDr",  // Child does exitWait(); sync; parent deletes thread; child returns

 // In these cases the child deletes the thread
    "BdrS",     // Child deletes thread, returns
    "BedrS",    // Child does exitWait(), deletes thread, returns
    "BwXTSdr",  // Parent does exitWait(0); sync; child deletes thread, returns

    NULL        // Terminator
};

// How long to sleep for while the other thread works
#define SLEEP_TIME   1.0

class threadCase: public epicsThreadRunable {
public:
    threadCase(const char * const tcase);
    virtual ~threadCase();
    virtual void run();
    epicsThread *pthread;
    epicsEvent startEvt;
    epicsEvent childEvt;
    epicsEvent parentEvt;
private:
    const char * const name;
};

threadCase::threadCase(const char * const tcase) :
    pthread(new epicsThread(*this, tcase,
            epicsThreadGetStackSize(epicsThreadStackSmall))),
    name(tcase)
{
    testDiag("Constructing test case '%s'", name);
}

threadCase::~threadCase()
{
    testDiag("Destroying test case '%s'", name);
}

void threadCase::run()
{
    testDiag("Child running for '%s'", name);
    startEvt.signal();

    for (const char * pdo = name;
         const char tdo = *pdo;
         pdo++)
    {
        switch (tdo)
        {
        case 'd':
            testDiag("'%c': Child deleting epicsThread", tdo);
            delete pthread;
            pthread = NULL;
            break;

        case 'e':
            testDiag("'%c': Child calling exitWait()", tdo);
            assert(pthread);
            pthread->exitWait();
            break;

        case 's':
            testDiag("'%c': Child sleeping", tdo);
            epicsThreadSleep(SLEEP_TIME);
            break;

        case 't':
            testDiag("'%c': Child sending trigger", tdo);
            parentEvt.signal();
            break;

        case 'w':
            testDiag("'%c': Child awaiting trigger", tdo);
            childEvt.wait();
            break;

        case 'r':
            testDiag("'%c': Child returning", tdo);
            return;
        }
    }
    testFail("Test case '%s' is missing 'r'", name);
}

MAIN(epicsThreadClassTest)
{
    const int ntests = NELEMENTS(cases);
    testPlan(ntests - 1); // The last element is the NULL terminator

    for (const char * const * pcase = cases;
         const char * const tcase = *pcase;
         pcase++)
    {
        testDiag("======= Test case '%s' =======", tcase);
        threadCase thrCase(tcase);

        for (const char * pdo = tcase;
             const char tdo = *pdo;
             pdo++)
        {
            switch (tdo)
            {
            case 'B':
                testDiag("'%c': Parent starting child", tdo);
                assert(thrCase.pthread);
                thrCase.pthread->start();
                thrCase.startEvt.wait();
                break;

            case 'D':
                testDiag("'%c': Parent deleting epicsThread", tdo);
                assert(thrCase.pthread);
                delete thrCase.pthread;
                thrCase.pthread = NULL;
                break;

            case 'E':
                testDiag("'%c': Parent calling exitWait()", tdo);
                assert(thrCase.pthread);
                thrCase.pthread->exitWait();
                break;

            case 'X':
                testDiag("'%c': Parent calling exitWait(0)", tdo);
                assert(thrCase.pthread);
                thrCase.pthread->exitWait(0);
                break;

            case 'S':
                testDiag("'%c': Parent sleeping", tdo);
                epicsThreadSleep(SLEEP_TIME);
                break;

            case 'T':
                testDiag("'%c': Parent sending trigger", tdo);
                thrCase.childEvt.signal();
                break;

            case 'W':
                testDiag("'%c': Parent awaiting trigger", tdo);
                thrCase.parentEvt.wait();
                break;
            }
        }

        testPass("Test case '%s' passed", tcase);
    }

    return testDone();
}
