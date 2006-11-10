/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* epicsThreadPerform.cpp */

/*          sleep accuracy and sleep quantum tests by Jeff Hill */
/*          epicsThreadGetIdSelf performance by Jeff Hill */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <math.h>

#include "epicsThread.h"
#include "epicsTime.h"
#include "errlog.h"
#include "testMain.h"

static void testPriority(const char *who)
{
    epicsThreadId id;
    unsigned int  oldPriority,newPriority;

    id = epicsThreadGetIdSelf();
    oldPriority = epicsThreadGetPriority(id);
    epicsThreadSetPriority(id,epicsThreadPriorityMax);
    newPriority = epicsThreadGetPriority(id);
    epicsThreadSetPriority(id,oldPriority);
    printf("testPriority %s\n    id %p old %u new %u\n",
        who,id,oldPriority,newPriority);
}

extern "C" void testPriorityThread(void *arg)
{
    testPriority("thread");
}

static void epicsThreadPriorityTest()
{
    testPriority("main error expected from epicsThreadSetPriority");
    epicsThreadCreate("testPriorityThread",epicsThreadPriorityMedium,
        epicsThreadGetStackSize(epicsThreadStackMedium),testPriorityThread,0);
    epicsThreadSleep(0.5);
}


static double threadSleepMeasureDelayError ( const double & delay )
{
    epicsTime beg = epicsTime::getCurrent();
    epicsThreadSleep ( delay );
    epicsTime end = epicsTime::getCurrent();
    double meas = end - beg;
    double error = fabs ( delay - meas );
    return error;
}

static double measureSleepQuantum (
    const unsigned iterations, const double testInterval )
{
    double errorSum = 0.0;
    printf ( "Estimating sleep quantum" );
    fflush ( stdout );
    for ( unsigned i = 0u; i < iterations; i++ ) {
        // try to guarantee a uniform probability density function
        // by intentionally burning some CPU until we are less
        // likely to be aligned with the schedualing clock
        double interval = rand ();
        interval /= RAND_MAX;
        interval *= testInterval;
        epicsTime start = epicsTime::getCurrent ();
        epicsTime current = start;
        while ( current - start < interval ) {
            current = epicsTime::getCurrent ();
        }
        errorSum += threadSleepMeasureDelayError ( testInterval );
        if ( i % ( iterations / 10 ) == 0 ) {
            printf ( "." );
            fflush ( stdout );
        }
    }
    printf ( "done\n" );

    // with a uniform probability density function the
    // sleep delay error mean is one half of the quantum
    double quantumEstimate = 2 * errorSum / iterations;
    return quantumEstimate;
}

static void threadSleepQuantumTest ()
{
    const double quantum = epicsThreadSleepQuantum ();

    double quantumEstimate = measureSleepQuantum ( 10, 10 * quantum );
    quantumEstimate = measureSleepQuantum ( 100, 2 * quantumEstimate );

    double quantumError = fabs ( quantumEstimate - quantum ) / quantum;
    const char * pTol = 0;
    if ( quantumError > 0.1 ) {
        pTol = "10%";
    }
    else if ( quantumError > 0.01 ) {
        pTol = "1%";
    }
    else if ( quantumError > 0.001 ) {
        pTol = "0.1%";
    }
    if ( pTol ) {
        printf (
            "The epicsThreadSleepQuantum() call returns %f sec.\n",
                quantum );
        printf (
            "This doesnt match the quantum estimate "
            "of %f sec within %s.\n",
                quantumEstimate, pTol );
    }
}


static void threadSleepTest ()
{
    static const int iterations = 20;
    const double quantum = epicsThreadSleepQuantum ();
    double errorSum = threadSleepMeasureDelayError ( 0.0 );
    for ( int i = 0u; i < iterations; i++ ) {
        double delay = ldexp ( 1.0 , -i );
        double error =
            threadSleepMeasureDelayError ( delay );
        errorSum += error;
        if ( error > quantum + quantum / 8.0 ) {
            printf ( "epicsThreadSleep ( %10f ) delay err %10f sec\n",
                delay, error );
        }
    }
    double averageError = errorSum / ( iterations + 1 );
    if ( averageError > quantum ) {
        printf ( "Average sleep delay error was %f sec\n", averageError );
    }
}

static void epicsThreadGetIdSelfPerfTest ()
{
    static const unsigned N = 10000;
    static const double microSecPerSec = 1e6;
    epicsTime begin = epicsTime::getCurrent ();
    for ( unsigned i = 0u; i < N; i++ ) {
        epicsThreadGetIdSelf ();
        epicsThreadGetIdSelf ();
        epicsThreadGetIdSelf ();
        epicsThreadGetIdSelf ();
        epicsThreadGetIdSelf ();

        epicsThreadGetIdSelf ();
        epicsThreadGetIdSelf ();
        epicsThreadGetIdSelf ();
        epicsThreadGetIdSelf ();
        epicsThreadGetIdSelf ();
    };
    epicsTime end = epicsTime::getCurrent ();
    printf ( "It takes %f micro sec to call epicsThreadGetIdSelf ()\n",
        microSecPerSec * ( end - begin ) / (10 * N) );
}


static epicsThreadPrivate < bool > priv;

static inline void callItTenTimes ()
{
    bool *pFlag;
    pFlag = priv.get ();
    pFlag = priv.get ();
    pFlag = priv.get ();
    pFlag = priv.get ();
    pFlag = priv.get ();
    pFlag = priv.get ();
    pFlag = priv.get ();
    pFlag = priv.get ();
    pFlag = priv.get ();
    pFlag = priv.get ();
}

static inline void callItTenTimesSquared ()
{
    callItTenTimes ();
    callItTenTimes ();
    callItTenTimes ();
    callItTenTimes ();
    callItTenTimes ();
    callItTenTimes ();
    callItTenTimes ();
    callItTenTimes ();
    callItTenTimes ();
    callItTenTimes ();
}

static void timeEpicsThreadPrivateGet ()
{
    priv.set ( 0 );

    epicsTime begin = epicsTime::getCurrent ();
    static const unsigned N = 1000u;
    for ( unsigned i = 0u; i < N; i++ ) {
        callItTenTimesSquared ();
    }
    double delay = epicsTime::getCurrent() - begin;
    delay /= N * 100u; // convert to sec per call
    delay *= 1e6; // convert to micro sec
    printf("epicsThreadPrivateGet() takes %f microseconds\n", delay);
}


MAIN(epicsThreadPerform)
{
    epicsThreadPriorityTest ();
    threadSleepQuantumTest ();
    threadSleepTest ();
    epicsThreadGetIdSelfPerfTest ();
    timeEpicsThreadPrivateGet ();
    return 0;
}
