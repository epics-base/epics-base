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

void epicsThreadCallEntryPoint ( void *pPvt )
{
    epicsThread *pThread = static_cast <epicsThread *> ( pPvt );
    pThread->begin.wait ();
    if ( ! pThread->cancel ) {
        pThread->runable.run ();
    }
    pThread->id = 0;
    pThread->terminated = true;
    pThread->exit.signal ();
}

void epicsThread::exitWait ()
{
    assert ( this->exitWait ( DBL_MAX ) );
}

bool epicsThread::exitWait ( double delay )
{
    if ( ! this->terminated ) {
        this->exit.wait ( delay );
    }
    return this->terminated;
}

epicsThread::epicsThread (epicsThreadRunable &r, const char *name, unsigned stackSize, unsigned priority ) :
    runable(r), cancel (false), terminated ( false )
{
    this->id = epicsThreadCreate ( name, priority, stackSize,
        epicsThreadCallEntryPoint, static_cast <void *> (this) );
}

epicsThread::~epicsThread ()
{
    if ( this->id ) {
        this->cancel = true;
        this->begin.signal ();
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
    this->begin.signal ();
}

bool epicsThread::isCurrentThread () const
{
    return ( epicsThreadGetIdSelf () == this->id );
}
