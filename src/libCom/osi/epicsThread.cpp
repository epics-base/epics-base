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

#include <stdio.h>
#include <stddef.h>
#include <float.h>

#define epicsExportSharedSymbols
#include "epicsThread.h"
#include "epicsAssert.h"

epicsThreadRunable::~epicsThreadRunable () {}
void epicsThreadRunable::stop() {};
void epicsThreadRunable::show(unsigned int) const {};

extern "C" void epicsThreadCallEntryPoint ( void * pPvt )
{
    epicsThread * pThread = 
        static_cast <epicsThread *> ( pPvt );
    bool waitRelease = false;
    pThread->pWaitReleaseFlag = & waitRelease;
    try {
        pThread->beginEvent.wait ();
        if ( ! pThread->cancel ) {
            pThread->runable.run ();
        }
        if ( ! waitRelease ) {
            pThread->exitWaitRelease ();
        }
        return;
    }
    catch ( const epicsThread::exitException & ) {
        if ( ! waitRelease ) {
            pThread->exitWaitRelease ();
        }
        return;
    }
    catch ( std::exception & except ) {
        char name [128];
        epicsThreadGetName ( pThread->id, name, sizeof ( name ) );
        errlogPrintf ( 
            "epicsThread: Unexpected C++ exception \"%s\" - terminating \"%s\"",
            except.what (), name );
        std::unexpected ();
    }
    catch ( ... ) {
        char name [128];
        epicsThreadGetName ( pThread->id, name, sizeof ( name ) );
        errlogPrintf ( 
            "epicsThread: Unknown C++ exception - terminating \"%s\"", name );
        std::unexpected ();
    }
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
    if ( ! this->terminated ) {
        this->exitEvent.wait ( delay );
    }
    return this->terminated;
}

void epicsThread::exitWaitRelease ()
{
    if ( this->isCurrentThread() ) {
        this->id = 0;
        this->terminated = true;
        *this->pWaitReleaseFlag = true;
        this->exitEvent.signal ();
    }
}

epicsThread::epicsThread ( epicsThreadRunable &r, const char *name,
    unsigned stackSize, unsigned priority ) :
        runable(r), pWaitReleaseFlag ( 0 ),
            cancel (false), terminated ( false )
{
    this->id = epicsThreadCreate ( name, priority, stackSize,
        epicsThreadCallEntryPoint, static_cast <void *> (this) );
}

epicsThread::~epicsThread ()
{
    if ( this->id ) {
        this->cancel = true;
        this->beginEvent.signal ();
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
}

void epicsThread::start ()
{
    this->beginEvent.signal ();
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


