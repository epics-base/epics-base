/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*epicsExit.c*/
/* Author: Marty Kraimer */
/*   Date: 23AUG2004     */

#include <stdlib.h>
#include <errno.h>

#include "ellLib.h"
#include "epicsThread.h"
#include "epicsMutex.h"
#include "cantProceed.h"
#define epicsExportSharedSymbols
#include "epicsExit.h"

typedef void (*epicsExitFunc)(void *arg);

typedef struct exitNode {
    ELLNODE       node;
    epicsExitFunc func;
    void          *arg;
}exitNode;

typedef struct exitPvt {
    ELLLIST      list;
    epicsMutexId lock;
    int          epicsExitNcalls;
}exitPvt;
static exitPvt exitPvtInstance;
static exitPvt *pexitPvt = &exitPvtInstance;
static epicsThreadOnceId createOnce = EPICS_THREAD_ONCE_INIT;

static void atExit(void) {epicsExit(0);}

static void createExitPvt(void)
{
    pexitPvt->lock = epicsMutexMustCreate();
    ellInit(&pexitPvt->list);
    atexit(atExit);
}

epicsShareFunc void epicsShareAPI epicsExitCallAtExits(void)
{
    exitNode *pexitNode;
    
    epicsThreadOnce(&createOnce,(EPICSTHREADFUNC)createExitPvt,0);
    epicsMutexMustLock(pexitPvt->lock);
    if(pexitPvt->epicsExitNcalls++>0) {
        epicsMutexUnlock(pexitPvt->lock);
        return;
    }
    epicsMutexUnlock(pexitPvt->lock);
    /* remove exitNodes in reverse order that they were added*/
    while((pexitNode = (exitNode *)ellLast(&pexitPvt->list))) {
        ellDelete(&pexitPvt->list,&pexitNode->node);
        pexitNode->func(pexitNode->arg);
        free(pexitNode);
    }
    /* epicsMutexDestroy is not called because of possible race conditions*/
    /*epicsMutexDestroy(pexitPvt->lock);*/
}

epicsShareFunc void epicsShareAPI epicsExit(int status)
{
    epicsExitCallAtExits();
    epicsThreadSleep(1.0);
    /* for vxWorks exit only terminates calling thread*/
#ifndef vxWorks
    exit(status);
#endif
}

epicsShareFunc int epicsShareAPI epicsAtExit(epicsExitFunc func, void *arg)
{
    exitNode *pexitNode;
    
    epicsThreadOnce(&createOnce,(EPICSTHREADFUNC)createExitPvt,0);
    pexitNode = callocMustSucceed(1,sizeof(exitNode),"epicsAtExit");
    pexitNode->func = func;
    pexitNode->arg = arg;
    epicsMutexMustLock(pexitPvt->lock);
    if(pexitPvt->epicsExitNcalls>0) {
        epicsMutexUnlock(pexitPvt->lock);
        free(pexitNode);
        return -1;
    }
    ellAdd(&pexitPvt->list,&pexitNode->node);
    epicsMutexUnlock(pexitPvt->lock);
    return 0;
}
