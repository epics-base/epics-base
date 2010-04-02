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

/*
 * NOTES:
 * 1) LOG_LAST_OWNER feature is normally commented out because 
 * it slows down the system at run time, anfd because its not 
 * currently safe to convert a thread id to a thread name because
 * the thread may have exited making the thread id invalid.
 */

#include <new>

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define epicsExportSharedSymbols
#include "epicsStdioRedirect.h"
#include "epicsThread.h"
#include "ellLib.h"
#include "errlog.h"
#include "epicsMutex.h"
#include "epicsThread.h"

static int firstTime = 1;
static ELLLIST mutexList;
static ELLLIST freeList;

struct epicsMutexParm {
    ELLNODE node;
    epicsMutexOSD * id;
#   ifdef LOG_LAST_OWNER
        epicsThreadId lastOwner;
#   endif
    const char *pFileName;
    int lineno;
};

static epicsMutexOSD * epicsMutexGlobalLock;


// vxWorks 5.4 gcc fails during compile when I use std::exception
using namespace std;

// exception payload
class epicsMutex::mutexCreateFailed : public exception
{
    const char * what () const throw ();
};

const char * epicsMutex::mutexCreateFailed::what () const throw ()
{
    return "epicsMutex::mutexCreateFailed()";
}

// exception payload
class epicsMutex::invalidMutex : public exception
{
    const char * what () const throw ();
};

const char * epicsMutex::invalidMutex::what () const throw ()
{
    return "epicsMutex::invalidMutex()";
}


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
#   ifdef LOG_LAST_OWNER
        pmutexNode->lastOwner = 0;
#   endif
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
#   ifdef LOG_LAST_OWNER
        if ( status == epicsMutexLockOK ) {
            pmutexNode->lastOwner = epicsThreadGetIdSelf();
        }
#   endif
    return status;
}

epicsMutexLockStatus epicsShareAPI epicsMutexTryLock(
    epicsMutexId pmutexNode)
{
    epicsMutexLockStatus status = 
        epicsMutexOsdTryLock(pmutexNode->id);
#   ifdef LOG_LAST_OWNER
        if ( status == epicsMutexLockOK ) {
            pmutexNode->lastOwner = epicsThreadGetIdSelf();
        }
#   endif
    return status;
}

void epicsShareAPI epicsMutexShow(
    epicsMutexId pmutexNode, unsigned  int level)
{
#   ifdef LOG_LAST_OWNER
        char threadName [255];
        if ( pmutexNode->lastOwner ) {
#           error currently not safe to fetch name for stale thread 
            epicsThreadGetName ( pmutexNode->lastOwner,
                threadName, sizeof ( threadName ) );
        }
        else {
            strcpy ( threadName, "<not used>" );
        }
        printf("epicsMutexId %p last owner \"%s\" source %s line %d\n",
            (void *)pmutexNode, threadName,
            pmutexNode->pFileName, pmutexNode->lineno);
#   else
        printf("epicsMutexId %p source %s line %d\n",
            (void *)pmutexNode, pmutexNode->pFileName, 
            pmutexNode->lineno);
#   endif
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

epicsMutex :: epicsMutex () :
    id ( epicsMutexCreate () )
{
    if ( this->id == 0 ) {
        throw mutexCreateFailed ();
    }
}

epicsMutex ::~epicsMutex () 
{
    epicsMutexDestroy ( this->id );
}

void epicsMutex::lock ()
{
    epicsMutexLockStatus status = epicsMutexLock ( this->id );
    if ( status != epicsMutexLockOK ) {
        throw invalidMutex ();
    }
}

bool epicsMutex::tryLock () // X aCC 361
{
    epicsMutexLockStatus status = epicsMutexTryLock ( this->id );
    if ( status == epicsMutexLockOK ) {
        return true;
    } 
    else if ( status != epicsMutexLockTimeout ) {
        throw invalidMutex ();
    }
    return false;
}

void epicsMutex::unlock ()
{
    epicsMutexUnlock ( this->id );
}

void epicsMutex :: show ( unsigned level ) const 
{
    epicsMutexShow ( this->id, level );
}

static epicsThreadPrivate < epicsDeadlockDetectMutex >  
    * pCurrentMutexLevel = 0;

static epicsThreadOnceId epicsDeadlockDetectMutexInit = EPICS_THREAD_ONCE_INIT;

extern "C"
void epicsDeadlockDetectMutexInitFunc ( void * ) 
{
    pCurrentMutexLevel = new epicsThreadPrivate < epicsDeadlockDetectMutex > ();
}

epicsDeadlockDetectMutex::
    epicsDeadlockDetectMutex ( hierarchyLevel_t level ) : 
    hierarchyLevel ( level ), pPreviousLevel ( 0 )
{
    epicsThreadOnce ( & epicsDeadlockDetectMutexInit, 
        epicsDeadlockDetectMutexInitFunc, 0 );
}

epicsDeadlockDetectMutex::~epicsDeadlockDetectMutex ()
{
}

void epicsDeadlockDetectMutex::show ( unsigned level ) const
{
    this->mutex.show ( level );
}

void epicsDeadlockDetectMutex::lock ()
{
    epicsDeadlockDetectMutex * pPrev = pCurrentMutexLevel->get();
    if ( pPrev && pPrev != this ) {
        if ( pPrev->hierarchyLevel >= this->hierarchyLevel ) {
            errlogPrintf ( "!!!! Deadlock Vulnerability Detected !!!! "
                "at level %u and moving to level %u\n",
                pPrev->hierarchyLevel,
                this->hierarchyLevel );
        }
    }
    this->mutex.lock ();
    if ( pPrev && pPrev != this ) {
        pCurrentMutexLevel->set ( this );
        this->pPreviousLevel = pPrev;
    }
}

void epicsDeadlockDetectMutex::unlock ()
{
    pCurrentMutexLevel->set ( this->pPreviousLevel );
    this->mutex.unlock ();
}

bool epicsDeadlockDetectMutex::tryLock ()
{
    bool success = this->mutex.tryLock ();
    if ( success ) {
        this->pPreviousLevel = pCurrentMutexLevel->get();
        pCurrentMutexLevel->set ( this );
    }
    return success;
}


