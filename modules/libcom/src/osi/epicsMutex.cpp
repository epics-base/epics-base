/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* epicsMutex.cpp */
/*  Author: Marty Kraimer and Jeff Hill Date: 03APR01   */

/*
 * NOTES:
 * 1) LOG_LAST_OWNER feature is normally commented out because
 * it slows down the system at run time, anfd because its not
 * currently safe to convert a thread id to a thread name because
 * the thread may have exited making the thread id invalid.
 */
#define EPICS_PRIVATE_API

#include <new>

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "epicsStdio.h"
#include "epicsThread.h"
#include "ellLib.h"
#include "errlog.h"
#include "epicsMutex.h"
#include "epicsMutexImpl.h"
#include "epicsThread.h"
#include "cantProceed.h"

static ELLLIST mutexList = ELLLIST_INIT;

/* Specially initialized to bootstrap initialization.
 * When supported (posix and !rtems) use statically initiallized mutex.
 * Otherwise, initialize via epicsMutexOsdSetup().
 */
struct epicsMutexParm epicsMutexGlobalLock = {ELLNODE_INIT, __FILE__, __LINE__};

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

epicsMutexId epicsStdCall epicsMutexOsiCreate(
    const char *pFileName,int lineno)
{
    epicsMutexOsdSetup();

    epicsMutexId ret = (epicsMutexId)calloc(1, sizeof(*ret));
    if(ret) {
        ret->pFileName = pFileName;
        ret->lineno = lineno;

        if(!epicsMutexOsdPrepare(ret)) {
            epicsMutexMustLock(&epicsMutexGlobalLock);
            ellAdd(&mutexList, &ret->node);
            (void)epicsMutexUnlock(&epicsMutexGlobalLock);

        } else {
            free(ret);
            ret = NULL;
        }

    }
    return ret;
}

epicsMutexId epicsStdCall epicsMutexOsiMustCreate(
    const char *pFileName,int lineno)
{
    epicsMutexId id = epicsMutexOsiCreate(pFileName,lineno);
    if(!id) {
        cantProceed("epicsMutexOsiMustCreate() fails at %s:%d\n",
                    pFileName, lineno);
    }
    return id;
}

void epicsStdCall epicsMutexDestroy(epicsMutexId pmutexNode)
{
    if(pmutexNode) {
        epicsMutexMustLock(&epicsMutexGlobalLock);
        ellDelete(&mutexList, &pmutexNode->node);
        (void)epicsMutexUnlock(&epicsMutexGlobalLock);
        epicsMutexOsdCleanup(pmutexNode);
        free(pmutexNode);
    }
}

void epicsStdCall epicsMutexShow(
    epicsMutexId pmutexNode, unsigned  int level)
{
    printf("epicsMutexId %p source %s line %d\n",
           (void *)pmutexNode, pmutexNode->pFileName,
           pmutexNode->lineno);
    if ( level > 0 ) {
        epicsMutexOsdShow(pmutexNode,level-1);
    }
}

void epicsStdCall epicsMutexShowAll(int onlyLocked,unsigned  int level)
{
    epicsMutexOsdSetup();

    printf("ellCount(&mutexList) %d\n", ellCount(&mutexList));
    epicsMutexOsdShowAll();
    epicsMutexMustLock(&epicsMutexGlobalLock);
    for(ELLNODE *cur =ellFirst(&mutexList); cur; cur = ellNext(cur)) {
        epicsMutexParm *lock = CONTAINER(cur, epicsMutexParm, node);
        if(onlyLocked) {
            // cycle through to test state
            if(epicsMutexTryLock(lock)==epicsMutexLockOK) {
                epicsMutexUnlock(lock);
                continue; // was not locked, skip
            }
        }
        epicsMutexShow(lock, level);
    }
    epicsMutexUnlock(&epicsMutexGlobalLock);
}

#if !defined(__GNUC__) || __GNUC__<4 || (__GNUC__==4 && __GNUC_MINOR__<8)
epicsMutex :: epicsMutex () :
    id ( epicsMutexCreate () )
{
    if ( this->id == 0 ) {
        throw mutexCreateFailed ();
    }
}
#endif

epicsMutex :: epicsMutex ( const char *pFileName, int lineno ) :
    id ( epicsMutexOsiCreate (pFileName, lineno) )
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

bool epicsMutex::tryLock ()
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


