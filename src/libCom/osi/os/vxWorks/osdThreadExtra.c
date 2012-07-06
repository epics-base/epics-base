/*************************************************************************\
* Copyright (c) 2012 ITER Organization
*
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Author:  Ralph Lange   Date:  26 Jun 2012 */

/* Null default thread hooks for all platforms that do not do anything special */

#define epicsExportSharedSymbols
#include "epicsThread.h"

epicsShareDef EPICS_THREAD_HOOK_ROUTINE epicsThreadHookDefault;
epicsShareDef EPICS_THREAD_HOOK_ROUTINE epicsThreadHookMain;
