/* epicsMutexTest.c */

/* 
 * Author:  Marty Kraimer Date:    26JAN2000 
 *          Jeff Hill (added mutex performance test )
 */

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

#include "epicsTime.h"
#include "epicsThread.h"
#include "epicsMutex.h"
#include "epicsEvent.h"
#include "errlog.h"


typedef struct info {
    int        threadnum;
    epicsMutexId mutex;
    int        quit;
}info;

static void mutexThread(void *arg)
{
    info *pinfo = (info *)arg;
    time_t tp;
    printf("mutexThread %d starting time %ld\n",pinfo->threadnum,time(&tp));
    while(1) {
        epicsMutexLockStatus status;
        if(pinfo->quit) {
            printf("mutexThread %d returning time %ld\n",
                pinfo->threadnum,time(&tp));
            return;
        }
        status = epicsMutexLock(pinfo->mutex);
        if(status!=epicsMutexLockOK) {
            printf("task %d epicsMutexLock returned %d  time %ld\n",
                pinfo->threadnum,(int)status,time(&tp));
        }
        printf("mutexThread %d epicsMutexLock time %ld\n",
            pinfo->threadnum,time(&tp));
        epicsThreadSleep(.1);
        epicsMutexUnlock(pinfo->mutex);
        epicsThreadSleep(.9);
    }
}

inline void lockPair ( epicsMutex &mutex )
{
    mutex.lock ();
    mutex.unlock ();
}

inline void tenLockPairs ( epicsMutex &mutex )
{
    lockPair ( mutex );
    lockPair ( mutex );
    lockPair ( mutex );
    lockPair ( mutex );
    lockPair ( mutex );
    lockPair ( mutex );
    lockPair ( mutex );
    lockPair ( mutex );
    lockPair ( mutex );
    lockPair ( mutex );
}

inline void tenLockPairsSquared ( epicsMutex &mutex )
{
    tenLockPairs ( mutex );
    tenLockPairs ( mutex );
    tenLockPairs ( mutex );
    tenLockPairs ( mutex );
    tenLockPairs ( mutex );
    tenLockPairs ( mutex );
    tenLockPairs ( mutex );
    tenLockPairs ( mutex );
    tenLockPairs ( mutex );
    tenLockPairs ( mutex );
}

void epicsMutexPerformance ()
{
    epicsMutex mutex;

    epicsTime begin = epicsTime::getCurrent ();
    static const unsigned N = 1000;
    for ( unsigned i = 0; i < N; i++ ) {
        tenLockPairsSquared ( mutex );
    }
    double delay = epicsTime::getCurrent () -  begin;
    delay /= N * 100u; // convert to delay per lock pair
    delay *= 1e6; // convert to micro seconds
    printf ( "One lock pair completes in %f micro sec\n", delay );
}

struct verifyTryLock {
    epicsMutexId mutex;
    epicsEventId done;
};

static void verifyTryLockThread ( void *pArg )
{
    struct verifyTryLock *pVerify = 
        ( struct verifyTryLock * ) pArg;
    epicsMutexLockStatus status;

    status = epicsMutexTryLock ( pVerify->mutex );
    assert ( status == epicsMutexLockTimeout );
    epicsEventSignal ( pVerify->done );
}

void verifyTryLock ()
{
    struct verifyTryLock verify;
    epicsMutexLockStatus status;

    verify.mutex = epicsMutexMustCreate ();
    verify.done = epicsEventMustCreate ( epicsEventEmpty );

    status = epicsMutexTryLock ( verify.mutex );
    assert ( status == epicsMutexLockOK );

    epicsThreadCreate ( "verifyTryLockThread", 40, 
        epicsThreadGetStackSize(epicsThreadStackSmall),
        verifyTryLockThread, &verify );

    epicsEventWait ( verify.done );

    epicsMutexUnlock ( verify.mutex );
    epicsMutexDestroy ( verify.mutex );
    epicsEventDestroy ( verify.done );
}

extern "C" void epicsMutexTest(int nthreads,int verbose)
{
    unsigned int stackSize;
    epicsThreadId *id;
    int i;
    char **name;
    void **arg;
    info **pinfo;
    epicsMutexId mutex;
    int status;
    time_t tp;
    int errVerboseSave = errVerbose;

    epicsMutexPerformance ();
    verifyTryLock ();

    errVerbose = verbose;
    mutex = epicsMutexMustCreate();
    printf("calling epicsMutexLock(mutex) time %ld\n",time(&tp));
    status = epicsMutexLock(mutex);
    if(status) printf("status %d\n",status);
    printf("calling epicsMutexLockWithTimeout(mutex,2.0) time %ld\n",time(&tp));
    status = epicsMutexLockWithTimeout(mutex,2.0);
    if(status) printf("status %d\n",status);
    printf("calling epicsMutexTryLock(mutex) time %ld\n",time(&tp));
    status = epicsMutexTryLock(mutex);
    if(status) printf("status %d\n",status);
    epicsMutexShow(mutex,1);
    printf("calling epicsMutexUnlock() time %ld\n",time(&tp));
    epicsMutexUnlock(mutex);
    printf("calling epicsMutexUnlock() time %ld\n",time(&tp));
    epicsMutexUnlock(mutex);
    printf("calling epicsMutexUnlock() time %ld\n",time(&tp));
    epicsMutexUnlock(mutex);
    epicsMutexShow(mutex,1);

    if(nthreads<=0) {
        errVerbose = errVerboseSave;
        return;
    }
    id = (epicsThreadId *)calloc(nthreads,sizeof(epicsThreadId));
    name = (char **)calloc(nthreads,sizeof(char *));
    arg = (void **)calloc(nthreads,sizeof(void *));
    pinfo = (info **)calloc(nthreads,sizeof(info *));
    stackSize = epicsThreadGetStackSize(epicsThreadStackSmall);
    for(i=0; i<nthreads; i++) {
        name[i] = (char *)calloc(10,sizeof(char));
        sprintf(name[i],"task%d",i);
        pinfo[i] = (info *)calloc(1,sizeof(info));
        pinfo[i]->threadnum = i;
        pinfo[i]->mutex = mutex;
        arg[i] = pinfo[i];
        id[i] = epicsThreadCreate(name[i],40,stackSize,
                                  mutexThread,
                                  arg[i]);
        printf("semTest created mutexThread %d id %p time %ld\n",
            i, id[i],time(&tp));
    }
    epicsThreadSleep(5.0);
    printf("semTest setting quit time %ld\n",time(&tp));
    for(i=0; i<nthreads; i++) {
        pinfo[i]->quit = 1;
    }
    epicsThreadSleep(2.0);
    errVerbose = errVerboseSave;
}
