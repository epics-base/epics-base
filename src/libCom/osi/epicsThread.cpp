//
// $Id$
//
// Author: Jeff Hill
//

#include <stdio.h>
#include <stddef.h>



#define epicsExportSharedSymbols
#include "epicsThread.h"

void epicsThreadRunable::stop() {};

static void epicsThreadCallEntryPoint ( void *pPvt )
{
    epicsThread *pThread = static_cast <epicsThread *> ( pPvt );
    pThread->begin.wait ();
    if ( ! pThread->cancel ) {
        pThread->runable.run ();
    }
    pThread->id = 0;
    pThread->exit.signal ();
}

epicsThread::epicsThread (epicsThreadRunable &r, const char *name, unsigned stackSize, unsigned priority ) :
    runable(r), cancel (false)
{
    this->id = epicsThreadCreate ( name, priority, stackSize,
        epicsThreadCallEntryPoint, static_cast <void *> (this) );
}

epicsThread::~epicsThread ()
{
    if ( this->id ) {
        this->cancel = true;
        this->begin.signal ();
        while ( ! this->exit.wait ( 5.0 ) ) {
            printf ("epicsThread::~epicsThread ():"
                    " Warning, thread object destroyed before thread exit \n");
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
