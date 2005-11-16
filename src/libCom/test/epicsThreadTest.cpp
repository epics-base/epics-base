/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* epicsThreadTest.cpp */

/* Author:  Marty Kraimer Date:    26JAN2000  */
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

static epicsThreadPrivate<int> privateKey;

class myThread: public epicsThreadRunable {
public:
    myThread(int arg,const char *name);
    virtual ~myThread();
    virtual void run();
    epicsThread thread;
private:
    int *argvalue;
};

myThread::myThread(int arg,const char *name) :
    thread(*this,name,epicsThreadGetStackSize(epicsThreadStackSmall),50+arg),
    argvalue(0)
{
    argvalue = new int;
    *argvalue = arg;
    thread.start();
}

myThread::~myThread() {delete argvalue;}

void myThread::run()
{
    int myPrivate = *argvalue;
    privateKey.set(argvalue);
    errlogPrintf("threadFunc %d starting argvalue %p\n",myPrivate,argvalue);
    epicsThreadSleep(2.0);
    argvalue = privateKey.get();
    errlogPrintf("threadFunc %d stopping argvalue %p\n",myPrivate,argvalue);
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

extern "C" void threadTest(int ntasks,int verbose)
{
    myThread **papmyThread;
    int i;
    char **name;
    int startPriority,minPriority,maxPriority;
    int errVerboseSave = errVerbose;

    epicsThreadPriorityTest();
    epicsThreadGetIdSelfPerfTest ();

    threadSleepTest();
    threadSleepQuantumTest();

    errVerbose = verbose;
    errlogInit(4096);
    papmyThread = (myThread **)calloc(ntasks,sizeof(myThread *));
    name = (char **)calloc(ntasks,sizeof(char **));
    errlogPrintf("threadTest starting\n");
    for(i=0; i<ntasks; i++) {
        name[i] = (char *)calloc(10,sizeof(char));
        sprintf(name[i],"task%d",i);
        papmyThread[i] = new myThread(i,name[i]);
        errlogPrintf("threadTest created %d myThread %p\n",i,papmyThread[i]);
        startPriority = papmyThread[i]->thread.getPriority();
        papmyThread[i]->thread.setPriority(epicsThreadPriorityMin);
        minPriority = papmyThread[i]->thread.getPriority();
        papmyThread[i]->thread.setPriority(epicsThreadPriorityMax);
        maxPriority = papmyThread[i]->thread.getPriority();
        papmyThread[i]->thread.setPriority(50+i);
        if(i==0)errlogPrintf("startPriority %d minPriority %d maxPriority %d\n",
            startPriority,minPriority,maxPriority);
    }
    epicsThreadSleep(.1);
    epicsThreadShowAll(0);
    epicsThreadSleep(5.0);
    errlogPrintf("epicsThreadTest returning\n");
    epicsThreadSleep(.5);
    errVerbose = errVerboseSave;
}
