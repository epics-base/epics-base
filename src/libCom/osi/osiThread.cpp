//
// $Id$
//
// Author: Jeff Hill
//

#include <stdio.h>
#include <stddef.h>

#define epicsExportSharedSymbols
#include "osiThread.h"

static void osiThreadCallEntryPoint (void *pPvt)
{
    osiThread *pThread = static_cast<osiThread *> (pPvt);
    pThread->entryPoint ();
    pThread->exit.signal ();
}

osiThread::osiThread (const char *name, unsigned stackSize,
                    unsigned priority)
{
    this->id =  threadCreate (name, priority, stackSize,
        osiThreadCallEntryPoint, static_cast <void *> (this) );
}

osiThread::~osiThread ()
{
    while ( !this->exit.wait (5.0) ) {
        printf ("osiThread::~osiThread (): Warning, thread object destroyed before thread exit \n");
    }
}
