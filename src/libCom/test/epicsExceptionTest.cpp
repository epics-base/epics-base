
//
// Verify that the local c++ exception mechanism maches the ANSI/ISO standard.
// Author: Jeff Hill
//

#include <new>
#include <iostream>

#include <limits.h>
#include <assert.h>

#include "epicsThread.h"

using namespace std;

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
        new char [LONG_MAX];
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
}
