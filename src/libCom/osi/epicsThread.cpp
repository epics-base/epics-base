//
// $Id$
//
// Author: Jeff Hill
//

#include <stdio.h>
#include <stddef.h>
#include <float.h>

#define epicsExportSharedSymbols
#include "epicsThread.h"

epicsThreadRunable::~epicsThreadRunable () {}
void epicsThreadRunable::stop() {};
void epicsThreadRunable::show(unsigned int) const {};

void epicsThreadCallEntryPoint ( void * pPvt )
{
    bool waitRelease = false;
    epicsThread * pThread = static_cast <epicsThread *> ( pPvt );
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
    catch ( ... ) {
        throw;
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
                "blocking for thread \"%s\" to exit", 
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
