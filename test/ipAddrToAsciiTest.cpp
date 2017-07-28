/*************************************************************************\
* Copyright (c) 2017 Michael Davidsaver
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#define EPICS_PRIVATE_API

#include "epicsMutex.h"
#include "epicsGuard.h"
#include "epicsThread.h"
#include "epicsEvent.h"
#include "ipAddrToAsciiAsynchronous.h"

#include "epicsUnitTest.h"
#include "testMain.h"

namespace {

typedef epicsGuard<epicsMutex> Guard;
typedef epicsGuardRelease<epicsMutex> UnGuard;

struct CB : public ipAddrToAsciiCallBack
{
    const char *name;
    epicsMutex mutex;
    epicsEvent starter, blocker, complete;
    bool started, cont, done;
    CB(const char *name) : name(name), started(false), cont(false), done(false) {}
    virtual ~CB() {}
    virtual void transactionComplete ( const char * pHostName )
    {
        Guard G(mutex);
        started = true;
        starter.signal();
        testDiag("In transactionComplete(%s) for %s", pHostName, name);
        while(!cont) {
            UnGuard U(G);
            if(!blocker.wait(2.0))
                break;
        }
        done = true;
        complete.signal();
    }
    void waitStart()
    {
        Guard G(mutex);
        while(!started) {
            UnGuard U(G);
            if(!starter.wait(2.0))
                break;
        }
    }
    void poke()
    {
        testDiag("Poke");
        Guard G(mutex);
        cont = true;
        blocker.signal();
    }
    void finish()
    {
        testDiag("Finish");
        Guard G(mutex);
        while(!done) {
            UnGuard U(G);
            if(!complete.wait(2.0))
                break;
        }
        testDiag("Finished");
    }
};

// ensure that lookup of 127.0.0.1 works
void doLookup(ipAddrToAsciiEngine& engine)
{
    testDiag("In doLookup");

    ipAddrToAsciiTransaction& trn(engine.createTransaction());
    CB cb("cb");
    osiSockAddr addr;
    addr.ia.sin_family = AF_INET;
    addr.ia.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.ia.sin_port = htons(42);

    testDiag("Start lookup");
    trn.ipAddrToAscii(addr, cb);
    cb.poke();
    cb.finish();
    testOk1(cb.cont);
    testOk1(cb.done);

    trn.release();
}

// Test cancel of pending transaction
void doCancel()
{
    testDiag("In doCancel");

    ipAddrToAsciiEngine& engine1(ipAddrToAsciiEngine::allocate());
    ipAddrToAsciiEngine& engine2(ipAddrToAsciiEngine::allocate());

    ipAddrToAsciiTransaction& trn1(engine1.createTransaction()),
                            & trn2(engine2.createTransaction());
    testOk1(&trn1!=&trn2);
    CB cb1("cb1"), cb2("cb2");

    osiSockAddr addr;
    addr.ia.sin_family = AF_INET;
    addr.ia.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.ia.sin_port = htons(42);

    // ensure that the worker thread is blocked with a transaction from engine1
    testDiag("Start lookup1");
    trn1.ipAddrToAscii(addr, cb1);
    testDiag("Wait start1");
    cb1.waitStart();

    testDiag("Start lookup2");
    trn2.ipAddrToAscii(addr, cb2);

    testDiag("release engine2, implicitly cancels lookup2");
    engine2.release();

    cb2.poke();
    testDiag("Wait for lookup2 timeout");
    cb2.finish();
    testOk1(!cb2.done);

    testDiag("Complete lookup1");
    cb1.poke();
    cb1.finish();
    testOk1(cb1.done);

    engine1.release();

    trn1.release();
    trn2.release();
}

} // namespace

MAIN(ipAddrToAsciiTest)
{
    testPlan(5);
    {
        ipAddrToAsciiEngine& engine(ipAddrToAsciiEngine::allocate());
        doLookup(engine);
        engine.release();
    }
    doCancel();
    // TODO: somehow test cancel of in-progress callback
    // allow time for any un-canceled transcations to crash us...
    epicsThreadSleep(1.0);

#ifdef __linux__
    ipAddrToAsciiEngine::cleanup();
#endif

    return testDone();
}
