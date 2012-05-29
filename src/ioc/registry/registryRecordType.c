/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* registryRecordType.c */

/* Author:  Marty Kraimer Date:    08JUN99 */

#define epicsExportSharedSymbols
#include "registry.h"
#include "registryRecordType.h"

static void * const registryID = "record type";


epicsShareFunc int epicsShareAPI registryRecordTypeAdd(
    const char *name, const recordTypeLocation *prtl)
{
    return registryAdd(registryID, name, (void *)prtl);
}

epicsShareFunc recordTypeLocation * epicsShareAPI registryRecordTypeFind(
    const char *name)
{
    return registryFind(registryID, name);
}
