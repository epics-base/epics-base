/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
//
// $Id$
//
// Author: Jeff Hill
//

#include <stdexcept>
#include <typeinfo>

#include <stdio.h>
#include <stddef.h>
#include <float.h>

#define epicsExportSharedSymbols
#include "epicsTime.h"
#include "epicsThread.h"
#include "epicsAssert.h"
#include "epicsGuard.h"

epicsThreadRunable::~epicsThreadRunable () {}
void epicsThreadRunable::stop () {};
void epicsThreadRunable::show ( unsigned int ) const {};

extern "C" void epicsThreadCallEntryPoint ( void * pPvt )
{
    epicsThread * pThread = 
        static_cast <epicsThread *> ( pPvt );
    bool waitRelease = false;
    try {
        pThread->pWaitReleaseFlag = & waitRelease;
        if ( pThread->beginWait () ) {
            pThread->runable.run ();
        }
    }
    catch ( const epicsThread::exitException & ) {
    }
    catch ( std::exception & except ) {
        if ( ! waitRelease ) {
            epicsTime cur = epicsTime::getCurrent ();
            char date[64];
            cur.strftime ( date, sizeof ( date ), "%a %b %d %Y %H:%M:%S.%f");
            char name [128];
            epicsThreadGetName ( pThread->id, name, sizeof ( name ) );
            errlogPrintf ( 
                "epicsThread: Unexpected C++ exception \"%s\" with type \"%s\" in thread \"%s\" at %s\n",
                except.what (), typeid ( except ).name (), name, date );
            // this should behave as the C++ implementation intends when an 
            // exception isnt handled. If users dont like this behavior, they 
            // can install an application specific unexpected handler.
            std::unexpected (); 
        }
    }
    catch ( ... ) {
        if ( ! waitRelease ) {
            epicsTime cur = epicsTime::getCurrent ();
            char date[64];
            cur.strftime ( date, sizeof ( date ), "%a %b %d %Y %H:%M:%S.%f");
            char name [128];
            epicsThreadGetName ( pThread->id, name, sizeof ( name ) );
            errlogPrintf ( 
                "epicsThread: Unknown C++ exception in thread \"%s\" at %s\n",
                name, date );
            // this should behave as the C++ implementation intends when an 
            // exception isnt handled. If users dont like this behavior, they 
            // can install an application specific unexpected handler.
            std::unexpected ();
        }
    }
    if ( ! waitRelease ) {
        pThread->exitWaitRelease ();
    }
    return;
}

bool epicsThread::beginWait ()
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    while ( ! this->begin && ! this->cancel ) {
        epicsGuardRelease < epicsMutex > unguard ( guard );
        this->event.wait ();
    }
    return this->begin && ! this->cancel;
}

void epicsThread::exit ()
{
    throw exitException ();
}

void epicsThread::exitWait ()
{
    assert ( this->exitWait ( DBL_MAX ) );
}

bool epicsThread::exitWait (const double delay )
{
    {
        epicsTime exitWaitBegin = epicsTime::getCurrent ();
        epicsGuard < epicsMutex > guard ( this->mutex );
        double elapsed = 0.0;
        this->cancel = true;
        while ( ! this->terminated ) {
            epicsGuardRelease < epicsMutex > unguard ( guard );
            this->event.signal ();
            this->exitEvent.wait ( delay - elapsed );
            epicsTime current = epicsTime::getCurrent ();
            double exitWaitElapsed = current - exitWaitBegin;
            if ( exitWaitElapsed >= delay ) {
                break;
            }
        }
    }
    return this->terminated;
}

void epicsThread::exitWaitRelease ()
{
    if ( this->isCurrentThread() ) {
        epicsGuard < epicsMutex > guard ( this->mutex );
        if ( this->pWaitReleaseFlag ) {
            *this->pWaitReleaseFlag = true;
        }
        this->terminated = true;
        this->exitEvent.signal ();
        // once the terminated flag is set and we release the lock
        // then the "this" pointer must not be touched again
    }
}

epicsThread::epicsThread ( epicsThreadRunable &r, const char *name,
    unsigned stackSize, unsigned priority ) :
        runable(r), pWaitReleaseFlag ( 0 ),
            begin ( false ), cancel (false), terminated ( false )
{
    this->id = epicsThreadCreate ( name, priority, stackSize,
        epicsThreadCallEntryPoint, static_cast <void *> (this) );
}

epicsThread::~epicsThread ()
{
    while ( ! this->exitWait ( 10.0 )  ) {
        char nameBuf [256];
        this->getName ( nameBuf, sizeof ( nameBuf ) );
        fprintf ( stderr, 
            "epicsThread::~epicsThread(): "
            "blocking for thread \"%s\" to exit\n", 
            nameBuf );
        fprintf ( stderr, 
            "was epicsThread object destroyed before thread exit ?\n");
    }
}

void epicsThread::start ()
{
    {
        epicsGuard < epicsMutex > guard ( this->mutex );
        this->begin = true;
    }
    this->event.signal ();
}

bool epicsThread::isCurrentThread () const
{
    return ( epicsThreadGetIdSelf () == this->id );
}

void epicsThread::resume ()
{
    epicsThreadResume ( this->id );
}

void epicsThread::getName ( char *name, size_t size ) const
{
    epicsThreadGetName ( this->id, name, size );
}

epicsThreadId epicsThread::getId () const
{
    return this->id;
}

unsigned int epicsThread::getPriority () const
{
    return epicsThreadGetPriority (this->id);
}

void epicsThread::setPriority (unsigned int priority)
{
    epicsThreadSetPriority (this->id, priority);
}

bool epicsThread::priorityIsEqual (const epicsThread &otherThread) const
{
    if ( epicsThreadIsEqual (this->id, otherThread.id) ) {
        return true;
    }
    return false;
}

bool epicsThread::isSuspended () const
{
    if ( epicsThreadIsSuspended (this->id) ) {
        return true;
    }
    return false;
}

bool epicsThread::operator == (const epicsThread &rhs) const
{
    return (this->id == rhs.id);
}

void epicsThread::suspendSelf ()
{
    epicsThreadSuspendSelf ();
}

void epicsThread::sleep (double seconds)
{
    epicsThreadSleep (seconds);
}

//epicsThread & epicsThread::getSelf ()
//{
//    return * static_cast<epicsThread *> ( epicsThreadGetIdSelf () );
//}

const char *epicsThread::getNameSelf ()
{
    return epicsThreadGetNameSelf ();
}

bool epicsThread::isOkToBlock ()
{
    return static_cast<bool>(epicsThreadIsOkToBlock());
}

void epicsThread::setOkToBlock(bool isOkToBlock)
{
    epicsThreadSetOkToBlock(static_cast<int>(isOkToBlock));
}

extern "C" {
    static epicsThreadOnceId okToBlockOnce = EPICS_THREAD_ONCE_INIT;
    epicsThreadPrivateId okToBlockPrivate;
    typedef struct okToBlockStruct okToBlockStruct;
    struct okToBlockStruct {
        int okToBlock;
    };
    static okToBlockStruct okToBlockNo = {0};
    static okToBlockStruct okToBlockYes = {1};
    
    static void epicsThreadOnceIdInit(void *)
    {
        okToBlockPrivate = epicsThreadPrivateCreate();
    }
        
    
    int epicsShareAPI epicsThreadIsOkToBlock(void)
    {
        okToBlockStruct *pokToBlock;
        void *arg = 0;
        epicsThreadOnce(&okToBlockOnce,epicsThreadOnceIdInit,arg);
        pokToBlock = (okToBlockStruct*)epicsThreadPrivateGet(okToBlockPrivate);
        return (pokToBlock ? pokToBlock->okToBlock : 0);
    }
    
    void epicsShareAPI epicsThreadSetOkToBlock(int isOkToBlock)
    {
        okToBlockStruct *pokToBlock;
        void *arg = 0;
        epicsThreadOnce(&okToBlockOnce,epicsThreadOnceIdInit,arg);
        pokToBlock = (isOkToBlock) ? &okToBlockYes : &okToBlockNo;
        epicsThreadPrivateSet(okToBlockPrivate,pokToBlock);
    }
} // extern "C"
