/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* epicsMutex.c */
/*	Author: Marty Kraimer and Jeff Hill	Date: 03APR01	*/

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

struct epicsMutexParm {
    ELLNODE node;
    epicsMutexOSD * id;
    epicsThreadId lastOwner;
    const char *pFileName;
    int lineno;
};

STATIC epicsMutexOSD * epicsMutexGlobalLock;

epicsMutexId epicsShareAPI epicsMutexOsiCreate(
    const char *pFileName,int lineno)
{
    epicsMutexOSD * id;

    if(firstTime) {
        firstTime=0;
        ellInit(&mutexList);
        ellInit(&freeList);
        epicsMutexGlobalLock = epicsMutexOsdCreate();
    }
    id = epicsMutexOsdCreate();
    if(!id) {
        return 0;
    }
    epicsMutexLockStatus lockStat =
        epicsMutexOsdLock(epicsMutexGlobalLock);
    assert ( lockStat == epicsMutexLockOK );
    epicsMutexParm *pmutexNode = 
        reinterpret_cast < epicsMutexParm * > ( ellFirst(&freeList) );
    if(pmutexNode) {
        ellDelete(&freeList,&pmutexNode->node);
    } else {
        pmutexNode = static_cast < epicsMutexParm * > ( calloc(1,sizeof(epicsMutexParm)) );
    }
    pmutexNode->id = id;
    pmutexNode->lastOwner = 0;
    pmutexNode->pFileName = pFileName;
    pmutexNode->lineno = lineno;
    ellAdd(&mutexList,&pmutexNode->node);
    epicsMutexOsdUnlock(epicsMutexGlobalLock);
    return(pmutexNode);
}

epicsMutexId epicsShareAPI epicsMutexOsiMustCreate(
    const char *pFileName,int lineno)
{
    epicsMutexId id = epicsMutexOsiCreate(pFileName,lineno);
    assert(id);
    return(id );
}

void epicsShareAPI epicsMutexDestroy(epicsMutexId pmutexNode)
{
    epicsMutexLockStatus lockStat =
        epicsMutexOsdLock(epicsMutexGlobalLock);
    assert ( lockStat == epicsMutexLockOK );
    ellDelete(&mutexList,&pmutexNode->node);
    epicsMutexOsdDestroy(pmutexNode->id);
    ellAdd(&freeList,&pmutexNode->node);
    epicsMutexOsdUnlock(epicsMutexGlobalLock);
}

void epicsShareAPI epicsMutexUnlock(epicsMutexId pmutexNode)
{
    epicsMutexOsdUnlock(pmutexNode->id);
}

epicsMutexLockStatus epicsShareAPI epicsMutexLock(
    epicsMutexId pmutexNode)
{
    epicsMutexLockStatus status = 
        epicsMutexOsdLock(pmutexNode->id);
    if ( status == epicsMutexLockOK ) {
        pmutexNode->lastOwner = epicsThreadGetIdSelf();
    }
    return status;
}

epicsMutexLockStatus epicsShareAPI epicsMutexTryLock(
    epicsMutexId pmutexNode)
{
    epicsMutexLockStatus status = 
        epicsMutexOsdTryLock(pmutexNode->id);
    if ( status == epicsMutexLockOK ) {
        pmutexNode->lastOwner = epicsThreadGetIdSelf();
    }
    return status;
}

void epicsShareAPI epicsMutexShow(
    epicsMutexId pmutexNode, unsigned  int level)
{
    char threadName [255];
    if ( pmutexNode->lastOwner ) {
        epicsThreadGetName ( pmutexNode->lastOwner,
            threadName, sizeof ( threadName ) );
    }
    else {
        strcpy ( threadName, "<not used>" );
    }
    printf("epicsMutexId %p last owner \"%s\" source %s line %d\n",
        (void *)pmutexNode, threadName,
        pmutexNode->pFileName, pmutexNode->lineno);
    if ( level > 0 ) {
        epicsMutexOsdShow(pmutexNode->id,level-1);
    }
}

void epicsShareAPI epicsMutexShowAll(int onlyLocked,unsigned  int level)
{
    epicsMutexParm *pmutexNode;

    if(firstTime) return;
    printf("ellCount(&mutexList) %d ellCount(&freeList) %d\n",
        ellCount(&mutexList),ellCount(&freeList));
    epicsMutexLockStatus lockStat =
        epicsMutexOsdLock(epicsMutexGlobalLock);
    assert ( lockStat == epicsMutexLockOK );
    pmutexNode = reinterpret_cast < epicsMutexParm * > ( ellFirst(&mutexList) );
    while(pmutexNode) {
        if(onlyLocked) {
            epicsMutexLockStatus status;
            status = epicsMutexOsdTryLock(pmutexNode->id);
            if(status==epicsMutexLockOK) {
                epicsMutexOsdUnlock(pmutexNode->id);
                pmutexNode =
                    reinterpret_cast < epicsMutexParm * > 
                        ( ellNext(&pmutexNode->node) );
                continue;
            }
        }
        epicsMutexShow(pmutexNode, level);
        pmutexNode =
            reinterpret_cast < epicsMutexParm * > ( ellNext(&pmutexNode->node) );
    }
    epicsMutexOsdUnlock(epicsMutexGlobalLock);
}

epicsMutex :: epicsMutex () epicsThrows (( epicsMutex::mutexCreateFailed )) :
    id ( epicsMutexCreate () )
{
    if ( this->id == 0 ) {
        throw mutexCreateFailed ();
    }
}

epicsMutex ::~epicsMutex () 
    epicsThrows (())
{
    epicsMutexDestroy ( this->id );
}

void epicsMutex::lock ()
    epicsThrows (( epicsMutex::invalidMutex ))
{
    epicsMutexLockStatus status = epicsMutexLock ( this->id );
    if ( status != epicsMutexLockOK ) {
        throw invalidMutex ();
    }
}

bool epicsMutex::tryLock () // X aCC 361
    epicsThrows (( epicsMutex::invalidMutex ))
{
    epicsMutexLockStatus status = epicsMutexTryLock ( this->id );
    if ( status == epicsMutexLockOK ) {
        return true;
    } 
    else if ( status == epicsMutexLockTimeout ) {
        return false;
    } 
    else {
        throw invalidMutex ();
        return false; // never here, but compiler is happy
    }
}

void epicsMutex::unlock ()
    epicsThrows (())
{
    epicsMutexUnlock ( this->id );
}

void epicsMutex :: show ( unsigned level ) const 
    epicsThrows (())
{
    epicsMutexShow ( this->id, level );
}


