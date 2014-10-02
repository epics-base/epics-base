/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* osi/os/RTEMS/osdFindSymbol.c */

/* GESYS could support this, but must use compile-time detection
 * as the code must build for non-GESYS systems as well.
 */

#define epicsExportSharedSymbols
#include "epicsFindSymbol.h"

epicsShareFunc void * epicsLoadLibrary(const char *name)
{
    return 0;
}

epicsShareFunc const char *epicsLoadError(void)
{
    return "epicsLoadLibrary not implemented";
}

epicsShareFunc void * epicsShareAPI epicsFindSymbol(const char *name)
{
    return 0;
}
