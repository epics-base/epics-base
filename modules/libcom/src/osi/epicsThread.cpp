/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
//
// Author: Jeff Hill
//

#include <exception>
#include <typeinfo>

#include <stdio.h>
#include <stddef.h>
#include <float.h>
#include <string.h>

// The following is required for Solaris builds
#undef __EXTENSIONS__

#define epicsExportSharedSymbols
#include "epicsAlgorithm.h"
#include "epicsTime.h"
#include "epicsThread.h"
#include "epicsAssert.h"
#include "epicsGuard.h"
#include "errlog.h"

using namespace std;

epicsThreadRunable::~epicsThreadRunable () {}
void epicsThreadRunable::run () {}
void epicsThreadRunable::show ( unsigned int ) const {}

class epicsThread :: unableToCreateThread :
    public std :: exception {
public:
    const char * what () const throw ();
};

const char * epicsThread ::
    unableToCreateThread :: what () const throw ()
{
    return "unable to create thread";
}

void epicsThread :: printLastChanceExceptionMessage (
    const char * pExceptionTypeName,
    const char * pExceptionContext )
{
    char date[64];
    try {
        epicsTime cur = epicsTime :: getCurrent ();
        cur.strftime ( date, sizeof ( date ), "%a %b %d %Y %H:%M:%S.%f");
    }
    catch ( ... ) {
        strcpy ( date, "<UKN DATE>" );
    }
    char name [128];
    epicsThreadGetName ( this->id, name, sizeof ( name ) );
    errlogPrintf (
        "epicsThread: Unexpected C++ exception \"%s\" "
        "with type \"%s\" in thread \"%s\" at %s\n",
        pExceptionContext, pExceptionTypeName, name, date );
    errlogFlush ();
    // This behavior matches the C++ implementation when an exception
    // isn't handled by the thread code. Users can install their own
    // application-specific unexpected handler if preferred.
    std::unexpected ();
}

extern "C" void epicsThreadCallEntryPoint ( void * pPvt )
{
    epicsThread * pThread =
        static_cast <epicsThread *> ( pPvt );
    bool threadDestroyed = false;
    try {
        pThread->pThreadDestroyed = & threadDestroyed;
        if ( pThread->beginWait () ) {
            pThread->runable.run ();
            // The run() routine may have destroyed the epicsThread
            // object by now; pThread can only be used below here
            // when the threadDestroyed flag is false.
        }
    }
    catch ( const epicsThread::exitException & ) {
    }
    catch ( std :: exception & except ) {
        if ( ! threadDestroyed ) {
            pThread->printLastChanceExceptionMessage (
                typeid ( except ).name (), except.what () );
        }
    }
    catch ( ... ) {
        if ( ! threadDestroyed ) {
            pThread->printLastChanceExceptionMessage (
                "catch ( ... )", "Non-standard C++ exception" );
        }
    }
    if ( ! threadDestroyed ) {
        epicsGuard < epicsMutex > guard ( pThread->mutex );
        pThread->pThreadDestroyed = NULL;
        pThread->terminated = true;
        pThread->exitEvent.signal ();
        // After the terminated flag is set and guard's destructor
        // releases the lock, pThread must never be used again.
    }
}

bool epicsThread::beginWait () throw ()
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

void epicsThread::exitWait () throw ()
{
    bool success = this->exitWait ( DBL_MAX );
    assert ( success );
}

bool epicsThread::exitWait ( const double delay ) throw ()
{
    try {
        // When called (usually by a destructor) in the context of
        // the managed thread we can't wait for the thread to exit.
        // Set the threadDestroyed flag and return success.
        if ( this->isCurrentThread() ) {
            if ( this->pThreadDestroyed ) {
                *this->pThreadDestroyed = true;
            }
            return true;
        }
        epicsTime exitWaitBegin = epicsTime::getCurrent ();
        double exitWaitElapsed = 0.0;
        epicsGuard < epicsMutex > guard ( this->mutex );
        this->cancel = true;
        while ( ! this->terminated && exitWaitElapsed < delay ) {
            epicsGuardRelease < epicsMutex > unguard ( guard );
            this->event.signal ();
            this->exitEvent.wait ( delay - exitWaitElapsed );
            epicsTime current = epicsTime::getCurrent ();
            exitWaitElapsed = current - exitWaitBegin;
        }
    }
    catch ( std :: exception & except ) {
        errlogPrintf (
            "epicsThread::exitWait(): Unexpected exception "
            " \"%s\"\n",
            except.what () );
        epicsThreadSleep ( epicsMin ( delay, 5.0 ) );
    }
    catch ( ... ) {
        errlogPrintf (
            "Non-standard unexpected exception in "
            "epicsThread::exitWait()\n" );
        epicsThreadSleep ( epicsMin ( delay, 5.0 ) );
    }
    // the event mechanism is used for other purposes
    this->event.signal ();
    return this->terminated;
}

epicsThread::epicsThread (
    epicsThreadRunable & runableIn, const char * pName,
        unsigned stackSize, unsigned priority ) :
    runable ( runableIn ), id ( 0 ), pThreadDestroyed ( 0 ),
    begin ( false ), cancel ( false ), terminated ( false )
{
    this->id = epicsThreadCreate (
        pName, priority, stackSize, epicsThreadCallEntryPoint,
        static_cast < void * > ( this ) );
    if ( ! this->id ) {
        throw unableToCreateThread ();
    }
}

epicsThread::~epicsThread () throw ()
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

void epicsThread::start () throw ()
{
    {
        epicsGuard < epicsMutex > guard ( this->mutex );
        this->begin = true;
    }
    this->event.signal ();
}

bool epicsThread::isCurrentThread () const throw ()
{
    return ( epicsThreadGetIdSelf () == this->id );
}

void epicsThread::resume () throw ()
{
    epicsThreadResume ( this->id );
}

void epicsThread::getName ( char *name, size_t size ) const throw ()
{
    epicsThreadGetName ( this->id, name, size );
}

epicsThreadId epicsThread::getId () const throw ()
{
    return this->id;
}

unsigned int epicsThread::getPriority () const throw ()
{
    return epicsThreadGetPriority (this->id);
}

void epicsThread::setPriority (unsigned int priority) throw ()
{
    epicsThreadSetPriority (this->id, priority);
}

bool epicsThread::priorityIsEqual (const epicsThread &otherThread) const throw ()
{
    if ( epicsThreadIsEqual (this->id, otherThread.id) ) {
        return true;
    }
    return false;
}

bool epicsThread::isSuspended () const throw ()
{
    if ( epicsThreadIsSuspended (this->id) ) {
        return true;
    }
    return false;
}

bool epicsThread::operator == (const epicsThread &rhs) const throw ()
{
    return (this->id == rhs.id);
}

void epicsThread::suspendSelf () throw ()
{
    epicsThreadSuspendSelf ();
}

void epicsThread::sleep (double seconds) throw ()
{
    epicsThreadSleep (seconds);
}

const char *epicsThread::getNameSelf () throw ()
{
    return epicsThreadGetNameSelf ();
}

bool epicsThread::isOkToBlock () throw ()
{
    return epicsThreadIsOkToBlock() != 0;
}

void epicsThread::setOkToBlock(bool isOkToBlock) throw ()
{
    epicsThreadSetOkToBlock(static_cast<int>(isOkToBlock));
}

void epicsThreadPrivateBase::throwUnableToCreateThreadPrivate ()
{
    throw epicsThreadPrivateBase::unableToCreateThreadPrivate ();
}

void epicsThread :: show ( unsigned level ) const throw ()
{
    ::printf ( "epicsThread at %p\n", this->id );
    if ( level > 0u ) {
        epicsThreadShow ( this->id, level - 1 );
        if ( level > 1u ) {
            ::printf ( "pThreadDestroyed = %p\n", this->pThreadDestroyed );
            ::printf ( "begin = %c, cancel = %c, terminated = %c\n",
                this->begin ? 'T' : 'F',
                this->cancel ? 'T' : 'F',
                this->terminated ? 'T' : 'F' );
            this->runable.show ( level - 2u );
            this->mutex.show ( level - 2u );
            ::printf ( "general purpose event\n" );
            this->event.show ( level - 2u );
            ::printf ( "exit event\n" );
            this->exitEvent.show ( level - 2u );
        }
    }
}

extern "C" {
    static epicsThreadOnceId okToBlockOnce = EPICS_THREAD_ONCE_INIT;
    epicsThreadPrivateId okToBlockPrivate;
    static const int okToBlockNo = 0;
    static const int okToBlockYes = 1;

    static void epicsThreadOnceIdInit(void *)
    {
        okToBlockPrivate = epicsThreadPrivateCreate();
    }

    int epicsShareAPI epicsThreadIsOkToBlock(void)
    {
        const int *pokToBlock;
        epicsThreadOnce(&okToBlockOnce, epicsThreadOnceIdInit, NULL);
        pokToBlock = (int *) epicsThreadPrivateGet(okToBlockPrivate);
        return (pokToBlock ? *pokToBlock : 0);
    }

    void epicsShareAPI epicsThreadSetOkToBlock(int isOkToBlock)
    {
        const int *pokToBlock;
        epicsThreadOnce(&okToBlockOnce, epicsThreadOnceIdInit, NULL);
        pokToBlock = (isOkToBlock) ? &okToBlockYes : &okToBlockNo;
        epicsThreadPrivateSet(okToBlockPrivate, (void *)pokToBlock);
    }

    epicsThreadId epicsShareAPI epicsThreadMustCreate (
        const char *name, unsigned int priority, unsigned int stackSize,
        EPICSTHREADFUNC funptr,void *parm)
    {
        epicsThreadId id = epicsThreadCreate (
            name, priority, stackSize, funptr, parm );
        assert ( id );
        return id;
    }
} // extern "C"

// Ensure the main thread gets a unique ID
epicsThreadId epicsThreadMainId = epicsThreadGetIdSelf();
