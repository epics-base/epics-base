/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

//
// Verify that the local c++ exception mechanism maches the ANSI/ISO standard.
// Author: Jeff Hill
//

#include <new>
#include <iostream>
#include <limits>

#include <assert.h>
#include <stdio.h>

#include "epicsThread.h"

using namespace std;

#if defined(__BORLANDC__) && defined(__linux__)
namespace  std  {
const nothrow_t  nothrow ;
}
#endif

class myThread : public epicsThreadRunable {
public:
    myThread ();
    void waitForCompletion ();
private:
    epicsThread thread;
    bool done;
    void run ();
};

static void epicsExceptionTestPrivate ()
{
    int excep = false;
    try {
        new char [numeric_limits<DST_T>::max (size_t)];
        assert ( 0 );
    }
    catch ( const bad_alloc & ) {
        excep = true;
    }
    catch ( ... ) {
        assert ( 0 );
    }
    try {
        char * p = new ( nothrow ) char [LONG_MAX];
        assert ( p == 0);
    }
    catch( ... ) {
        assert ( 0 );
    }
}

myThread::myThread () :
    thread ( *this, "testExceptions", epicsThreadGetStackSize(epicsThreadStackSmall) ),
        done ( false )
{
    this->thread.start ();
}

void myThread::run ()
{
    epicsExceptionTestPrivate ();
    this->done = true;
}

void myThread::waitForCompletion ()
{
    while ( ! this->done ) {
        epicsThreadSleep ( 0.1 );
    }
}

void epicsExceptionTest ()
{
    myThread athread;

    epicsExceptionTestPrivate ();

    athread.waitForCompletion ();

    printf ( "Test Complete.\n" );
}
