//
// $Id$
//
// Author: Jeff Hill
//

#include <stdio.h>
#include <stddef.h>

static void osiThreadCallEntryPoint (void *pPvt); // for gnu warning

#define epicsExportSharedSymbols
#include "osiThread.h"

static void osiThreadCallEntryPoint (void *pPvt)
{
    osiThread *pThread = static_cast<osiThread *> (pPvt);
    pThread->entryPoint ();
    pThread->exit.signal ();
}


osiThread::osiThread () : id(0), exit()
{
}

osiThread::~osiThread ()
{
    while ( !this->exit.wait (5.0) ) {
        printf ("osiThread::~osiThread ():"
                " Warning, thread object destroyed before thread exit \n");
    }
}

void osiThread::start(const char *name, unsigned stackSize, unsigned priority)
{
    id = threadCreate (name, priority, stackSize,
        osiThreadCallEntryPoint, static_cast <void *> (this) );
}

