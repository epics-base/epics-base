//
// $Id$
//
// Author: Jeff Hill
//

#include <stddef.h>
#define epicsExportSharedSymbols
#include "osiThread.h"

static void osiThreadCallEntryPoint (void *pPvt)
{
    osiThread *pThread = static_cast<osiThread *> (pPvt);
    pThread->entryPoint ();
}

osiThread::osiThread (const char *name, unsigned stackSize,
                    unsigned priority)
{
    this->id =  threadCreate (name, priority, stackSize,
        osiThreadCallEntryPoint, static_cast <void *> (this) );
}
