/* epicsThreadTest.cpp */

/* Author:  Marty Kraimer Date:    26JAN2000  */
/*          sleep accuracy tests by Jeff Hill */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

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

static void threadSleepTest()
{
    for ( unsigned i = 0u; i < 20; i++ ) {
        epicsTime beg = epicsTime::getCurrent();
        double delay = ldexp ( 1.0 , -i );
        epicsThreadSleep ( delay );
        epicsTime end = epicsTime::getCurrent();
        printf ( "epicsThreadSleep ( %g ) finished after %g sec\n", 
            delay, end - beg );
    }
}

extern "C" void threadTest(int ntasks,int verbose)
{
    myThread **papmyThread;
    int i;
    char **name;
    int startPriority,minPriority,maxPriority;
    int errVerboseSave = errVerbose;

    threadSleepTest();

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
