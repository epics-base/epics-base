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
        pThread->beginWait ();
        if ( ! pThread->cancel ) {
            pThread->runable.run ();
        }
    }
    catch ( const epicsThread::exitException & ) {
    }
    catch ( std::exception & except ) {
        if ( ! waitRelease ) {
            char name [128];
            epicsThreadGetName ( pThread->id, name, sizeof ( name ) );
            errlogPrintf ( 
                "epicsThread: Unexpected C++ exception \"%s\" with type \"%s\" - terminating thread \"%s\"",
                except.what (), typeid ( except ).name (), name );
        }
    }
    catch ( ... ) {
        if ( ! waitRelease ) {
            char name [128];
            epicsThreadGetName ( pThread->id, name, sizeof ( name ) );
            errlogPrintf ( 
                "epicsThread: Unknown C++ exception - terminating thread \"%s\"", name );
        }
    }
    if ( ! waitRelease ) {
        pThread->exitWaitRelease ();
    }
    return;
}

void epicsThread::beginWait ()
{
    {
        epicsGuard < epicsMutex > guard ( this->mutex );
        while ( ! this->begin ) {
            epicsGuardRelease < epicsMutex > unguard ( guard );
            this->event.wait ();
        }
        // the event mechanism is used for other purposes
    }
    this->event.signal ();
}

void epicsThread::exit ()
{
    throw exitException ();
}

void epicsThread::exitWait ()
{
    assert ( this->exitWait ( DBL_MAX ) );
}

bool epicsThread::exitWait ( double delay )
{
    {
        epicsTime exitWaitBegin = epicsTime::getCurrent ();
        epicsGuard < epicsMutex > guard ( this->mutex );
        double elapsed = 0.0;
        while ( ! this->terminated ) {
            epicsGuardRelease < epicsMutex > unguard ( guard );
            this->event.wait ( delay - elapsed );
            epicsTime current = epicsTime::getCurrent ();
            double exitWaitElapsed = current - exitWaitBegin;
            if ( exitWaitElapsed >= delay ) {
                break;
            }
        }
    }
    // the event mechanism is used for other purposes
    this->event.signal ();
    return this->terminated;
}

void epicsThread::exitWaitRelease ()
{
    if ( this->isCurrentThread() ) {
        if ( this->pWaitReleaseFlag ) {
            *this->pWaitReleaseFlag = true;
        }
        {
            // once the terminated flag is set and we release the lock
            // then the "this" pointer must not be touched again
            epicsGuard < epicsMutex > guard ( this->mutex );
            this->terminated = true;
            this->event.signal ();
        }
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
    {
        epicsGuard < epicsMutex > guard ( this->mutex );
        if ( this->terminated ) {
            return;
        }
        this->cancel = true;
    }
    
    this->event.signal ();

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
    epicsThreadResume (this->id);
}

void epicsThread::getName (char *name, size_t size) const
{
    epicsThreadGetName (this->id, name, size);
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
    return static_cast<int>(epicsThreadIsOkToBlock());
}

void epicsThread::setOkToBlock(bool isOkToBlock)
{
    epicsThreadSetOkToBlock(static_cast<int>(isOkToBlock));
}

