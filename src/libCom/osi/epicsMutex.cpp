/* epicsMutex.c */
/*	Author: Marty Kraimer and Jeff Hill	Date: 03APR01	*/
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

(C)  COPYRIGHT 1991 Regents of the University of California,
and the University of Chicago Board of Governors.

This software was developed under a United States Government license
described on the COPYRIGHT_Combined file included as part
of this distribution.
**********************************************************************/

#include <new>

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define epicsExportSharedSymbols
#include "epicsThread.h"
#include "ellLib.h"
#include "errlog.h"
#include "epicsMutex.h"
#include "epicsThread.h"

#define STATIC static

STATIC int firstTime = 1;
STATIC ELLLIST mutexList;
STATIC ELLLIST freeList;

typedef struct mutexNode {
    ELLNODE node;
    epicsMutexId id;
    const char *pFileName;
    int lineno;
}mutexNode;

STATIC epicsMutexId lock;

epicsMutexId epicsShareAPI epicsMutexOsiCreate(
    const char *pFileName,int lineno)
{
    epicsMutexId id;
    mutexNode *pmutexNode;

    if(firstTime) {
        firstTime=0;
        ellInit(&mutexList);
        ellInit(&freeList);
        lock = epicsMutexOsdCreate();
    }
    epicsMutexMustLock(lock);
    id = epicsMutexOsdCreate();
    if(id) {
        pmutexNode = (mutexNode *)ellFirst(&freeList);
        if(pmutexNode) {
            ellDelete(&freeList,&pmutexNode->node);
        } else {
            pmutexNode = static_cast <mutexNode *> ( calloc(1,sizeof(mutexNode)) );
        }
        pmutexNode->id = id;
        pmutexNode->pFileName = pFileName;
        pmutexNode->lineno = lineno;
        ellAdd(&mutexList,&pmutexNode->node);
    }
    epicsMutexUnlock(lock);
    return(id);
}

epicsMutexId epicsShareAPI epicsMutexOsiMustCreate(
    const char *pFileName,int lineno)
{
    epicsMutexId id = epicsMutexOsiCreate(pFileName,lineno);
    assert(id);
    return(id );
}

void epicsShareAPI epicsMutexDestroy(epicsMutexId id)
{
    mutexNode *pmutexNode;

    epicsMutexMustLock(lock);
    pmutexNode = (mutexNode *)ellLast(&mutexList);
    while(pmutexNode) {
        if(id==pmutexNode->id) {
            ellDelete(&mutexList,&pmutexNode->node);
            ellAdd(&freeList,&pmutexNode->node);
            epicsMutexOsdDestroy(pmutexNode->id);
            epicsMutexUnlock(lock);
            return;
        }
        pmutexNode = (mutexNode *)ellPrevious(&pmutexNode->node);
    }
    epicsMutexUnlock(lock);
    errlogPrintf("epicsMutexDestroy Did not find epicsMutexId\n");
}

void epicsShareAPI epicsMutexShowAll(int onlyLocked,unsigned  int level)
{
    mutexNode *pmutexNode;

    if(firstTime) return;
    printf("ellCount(&mutexList) %d ellCount(&freeList) %d\n",
        ellCount(&mutexList),ellCount(&freeList));
    epicsMutexMustLock(lock);
    pmutexNode = (mutexNode *)ellFirst(&mutexList);
    while(pmutexNode) {
        if(onlyLocked) {
            epicsMutexLockStatus status;
            status = epicsMutexTryLock(pmutexNode->id);
            if(status==epicsMutexLockOK) {
                epicsMutexUnlock(pmutexNode->id);
                pmutexNode = (mutexNode *)ellNext(&pmutexNode->node);
                continue;
            }
        }
        printf("epicsMutexId %p source %s line %d\n",
            (void *)pmutexNode->id,pmutexNode->pFileName,pmutexNode->lineno);
        epicsMutexShow(pmutexNode->id,level);
        pmutexNode = (mutexNode *)ellNext(&pmutexNode->node);
    }
    epicsMutexUnlock(lock);
}


epicsMutex :: epicsMutex () :
    id ( epicsMutexCreate () )
{
    if ( this->id == 0 ) {
        throw std::bad_alloc ();
    }
}

epicsMutex ::~epicsMutex ()
{
    epicsMutexDestroy ( this->id );
}

void epicsMutex :: lock ()
{
    epicsMutexLockStatus status = epicsMutexLock ( this->id );
    if ( status != epicsMutexLockOK ) {
        throwWithLocation ( invalidSemaphore () );
    }
}

bool epicsMutex :: lock ( double timeOut )
{
    epicsMutexLockStatus status = epicsMutexLockWithTimeout ( this->id, timeOut );
    if (status==epicsMutexLockOK) {
        return true;
    } else if (status==epicsMutexLockTimeout) {
        return false;
    } else {
        throwWithLocation ( invalidSemaphore () );
    }
    return false; // never here, compiler is happy
}

bool epicsMutex :: tryLock ()
{
    epicsMutexLockStatus status = epicsMutexTryLock ( this->id );
    if ( status == epicsMutexLockOK ) {
        return true;
    } else if ( status == epicsMutexLockTimeout ) {
        return false;
    } else {
        throwWithLocation ( invalidSemaphore () );
    }
    return false; // never here, but compiler is happy
}

void epicsMutex :: unlock ()
{
    epicsMutexUnlock ( this->id );
}

void epicsMutex :: show ( unsigned level ) const
{
    epicsMutexShow ( this->id, level );
}