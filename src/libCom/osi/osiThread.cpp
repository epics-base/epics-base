//
// $Id$
//
// Author: Jeff Hill
//

#include <stdio.h>
#include <stddef.h>

static void osiThreadCallEntryPoint ( void *pPvt ); // for gnu warning

#define epicsExportSharedSymbols
#include "osiThread.h"

static void osiThreadCallEntryPoint ( void *pPvt )
{
    osiThread *pThread = static_cast <osiThread *> ( pPvt );
    pThread->begin.wait ();
    if ( ! pThread->cancel ) {
        pThread->entryPoint ();
    }
    pThread->exit.signal ();
}

osiThread::osiThread ( const char *name, unsigned stackSize, unsigned priority ) :
    cancel (false)
{
    this->id = threadCreate ( name, priority, stackSize,
        osiThreadCallEntryPoint, static_cast <void *> (this) );
}

osiThread::~osiThread ()
{
    if ( this->id ) {
        this->cancel = true;
        this->begin.signal ();
        while ( ! this->exit.wait ( 5.0 ) ) {
            printf ("osiThread::~osiThread ():"
                    " Warning, thread object destroyed before thread exit \n");
        }
    }
}

void osiThread::start ()
{
    this->begin.signal ();
}

