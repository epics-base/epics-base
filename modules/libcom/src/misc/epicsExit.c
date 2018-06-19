/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*epicsExit.c*/
/* 
 * Author: Marty Kraimer 
 * Date: 23AUG2004
 * Thread exit revisions: Jeff Hill
 * Date: 06Dec2006
 *
 * Note that epicsExitCallAtThreadExits is currently called directly from the 
 * thread entry wrapper in OS dependent code. That approach might not work 
 * correctly if the thread exits indirectly instead of just returning from
 * the function specified to epicsThreadCreate. For example the thread might 
 * exit via the exit() call. There might be OS dependent solutions for that
 * weakness.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#define epicsExportSharedSymbols
#include "ellLib.h"
#include "errlog.h"
#include "epicsThread.h"
#include "epicsMutex.h"
#include "cantProceed.h"
#include "epicsExit.h"

void epicsMutexCleanup(void);

typedef struct exitNode {
    ELLNODE         node;
    epicsExitFunc   func;
    void            *arg;
    char            name[1];
}exitNode;

typedef struct exitPvt {
    ELLLIST         list;
} exitPvt;

int atExitDebug = 0;

static epicsThreadOnceId exitPvtOnce = EPICS_THREAD_ONCE_INIT;
static epicsThreadOnceId exitLaterOnce = EPICS_THREAD_ONCE_INIT;
static exitPvt * pExitPvtPerProcess = 0;
static epicsMutexId exitPvtLock = 0;
static epicsThreadPrivateId exitPvtPerThread = 0;

static int exitLaterStatus;

static void destroyExitPvt(exitPvt * pep)
{
    ellFree ( &pep->list );
    free ( pep );
}

static exitPvt * createExitPvt(void)
{
    exitPvt * pep = calloc ( 1, sizeof ( * pep ) );
    if ( pep ) {
        ellInit ( &pep->list );
    }
    return pep;
}

static void exitPvtOnceFunc(void *pParm)
{
    exitPvtPerThread = epicsThreadPrivateCreate ();
    assert ( exitPvtPerThread );
    exitPvtLock = epicsMutexMustCreate ();
}

static void epicsExitInit(void)
{
    epicsThreadOnce ( & exitPvtOnce, exitPvtOnceFunc, 0 );
}

static void epicsExitCallAtExitsPvt(exitPvt *pep)
{
    exitNode *pexitNode;

    while ( ( pexitNode = (exitNode *) ellLast ( & pep->list ) ) ) {
        if (atExitDebug && pexitNode->name[0])
            fprintf(stderr, "atExit %s(%p)\n", pexitNode->name, pexitNode->arg);
        else if(atExitDebug)
            fprintf(stderr, "atExit %p(%p)\n", pexitNode->func, pexitNode->arg);
        pexitNode->func ( pexitNode->arg );
        ellDelete ( & pep->list, & pexitNode->node );
        free ( pexitNode );
    }
}

epicsShareFunc void epicsExitCallAtExits(void)
{
    exitPvt * pep = 0;

    epicsExitInit ();
    epicsMutexMustLock ( exitPvtLock );
    if ( pExitPvtPerProcess ) {
        pep = pExitPvtPerProcess;
        pExitPvtPerProcess = 0;
    }
    epicsMutexUnlock ( exitPvtLock );
    if ( pep ) {
        epicsExitCallAtExitsPvt ( pep );
        destroyExitPvt ( pep );
    }
    /* Handle specially to avoid circular reference */
    epicsMutexCleanup();
}

epicsShareFunc void epicsExitCallAtThreadExits(void)
{
    exitPvt * pep;

    epicsExitInit ();
    pep = epicsThreadPrivateGet ( exitPvtPerThread );
    if ( pep ) {
        epicsExitCallAtExitsPvt ( pep );
        destroyExitPvt ( pep );
        epicsThreadPrivateSet ( exitPvtPerThread, 0 );
    }
}

static int epicsAtExitPvt(exitPvt *pep, epicsExitFunc func, void *arg, const char *name)
{
    int status = -1;
    exitNode * pExitNode = calloc ( 1, sizeof( *pExitNode ) + (name?strlen(name):0) );

    if ( pExitNode ) {
        pExitNode->func = func;
        pExitNode->arg = arg;
        if(name)
            strcpy(pExitNode->name, name);
        ellAdd ( & pep->list, & pExitNode->node );
        status = 0;
    }
    return status;
}

epicsShareFunc int epicsAtThreadExit(epicsExitFunc func, void *arg)
{
    exitPvt * pep;

    epicsExitInit ();
    pep = epicsThreadPrivateGet ( exitPvtPerThread );
    if ( ! pep ) {
        pep = createExitPvt ();
        if ( ! pep ) {
            return -1;
        }
        epicsThreadPrivateSet ( exitPvtPerThread, pep );
    }
    return epicsAtExitPvt ( pep, func, arg, NULL );
}

epicsShareFunc int epicsAtExit3(epicsExitFunc func, void *arg, const char* name)
{
    int status = -1;

    epicsExitInit ();
    epicsMutexMustLock ( exitPvtLock );
    if ( !pExitPvtPerProcess ) {
        pExitPvtPerProcess = createExitPvt ();
    }
    if ( pExitPvtPerProcess ) {
        status = epicsAtExitPvt ( pExitPvtPerProcess, func, arg, name );
    }
    epicsMutexUnlock ( exitPvtLock );
    return status;
}

epicsShareFunc void epicsExit(int status)
{
    epicsExitCallAtExits();
    epicsThreadSleep(0.1);
    exit(status);
}

static void exitNow(void *junk)
{
    epicsExit(exitLaterStatus);
}

static void exitLaterOnceFunc(void *raw)
{
    int *status = raw;
    exitLaterStatus = *status;
    epicsThreadMustCreate("exitLater",
                      epicsThreadPriorityLow,
                      epicsThreadGetStackSize(epicsThreadStackSmall),
                      &exitNow, NULL);
}

epicsShareFunc void epicsExitLater(int status)
{
    epicsThreadOnce(&exitLaterOnce, &exitLaterOnceFunc, &status);
}

#include "epicsExport.h"

epicsExportAddress(int,atExitDebug);
