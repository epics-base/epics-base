/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

//
// Verify that the local c++ exception mechanism matches the ANSI/ISO standard.
// Author: Jeff Hill
//

#include <new>
#include <iostream>
#include <cstdio>
#if defined(__GNUC__) && (__GNUC__<2 || (__GNUC__==2 && __GNUC_MINOR__<=96))
#include <climits>
#else
#include <limits>
#endif

#include "epicsUnitTest.h"
#include "epicsThread.h"
#include "testMain.h"

using namespace std;

#if defined ( _MSC_VER )

#   if _MSC_VER >= 1900
        // Can't use const
        static size_t unsuccessfulNewSize = numeric_limits < size_t > :: max ();
#   elif _MSC_VER > 1310
        static const size_t unsuccessfulNewSize = numeric_limits < size_t > :: max ();
#   else
        // Bug in the older MS implementation of new
        static const size_t unsuccessfulNewSize = numeric_limits < size_t > :: max () - 100;
#   endif

    // No %z modifier support for printing a size_t
#   define Z_MODIFIER ""

#elif defined(vxWorks)

    // Don't try to use numeric_limits < size_t >
    static const size_t unsuccessfulNewSize = UINT_MAX - 15u;

    // No %z modifier support for printing a size_t
#   define Z_MODIFIER ""

#else

#   if defined(__GNUC__) && (__GNUC__ >= 6)
        // Can't use const
        static size_t unsuccessfulNewSize = numeric_limits < size_t > :: max ();
#   else
        static const size_t unsuccessfulNewSize = numeric_limits < size_t > :: max ();
#   endif

    // passing a size_t to printf() needs "%zu"
#   define Z_MODIFIER "z"

#endif

class exThread : public epicsThreadRunable {
public:
    exThread ();
    void waitForCompletion ();
    ~exThread() {};
private:
    epicsThread thread;
    bool done;
    void run ();
};

static void epicsExceptionTestPrivate ()
{
    try {
        char * p = new char [unsuccessfulNewSize];
        testFail("new char[%" Z_MODIFIER "u] returned %p", unsuccessfulNewSize, p);
    }
    catch ( const bad_alloc & ) {
        testPass("new char[%" Z_MODIFIER "u] threw", unsuccessfulNewSize);
    }
    catch ( ... ) {
        testFail("new: threw wrong type");
    }
    try {
        char * p = new ( nothrow ) 
            char [unsuccessfulNewSize];
        testOk(p == 0, "new (nothrow) returned %p", p);
    }
    catch( ... ) {
        testFail("new (nothrow): threw");
    }
}

exThread::exThread () :
    thread ( *this, "testExceptions", epicsThreadGetStackSize(epicsThreadStackSmall) ),
        done ( false )
{
    this->thread.start ();
}

void exThread::run ()
{
    epicsExceptionTestPrivate ();
    this->done = true;
}

void exThread::waitForCompletion ()
{
    while ( ! this->done ) {
        epicsThreadSleep ( 0.1 );
    }
}

MAIN(epicsExceptionTest)
{
    testPlan(4);
    epicsExceptionTestPrivate ();
    
    exThread athread;
    athread.waitForCompletion ();
    return testDone();
}
