/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* registryRecordType.c */

/* Author:  Marty Kraimer Date:    08JUN99 */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>

#include "dbBase.h"
#define epicsExportSharedSymbols
#include "registry.h"
#include "registryRecordType.h"

const char *recordType = "record type";
static void *registryID = (void *)&recordType;


epicsShareFunc int epicsShareAPI registryRecordTypeAdd(
    const char *name,const recordTypeLocation *prtl)
{
    return(registryAdd(registryID,name,(void *)prtl));
}

epicsShareFunc recordTypeLocation * epicsShareAPI registryRecordTypeFind(
    const char *name)
{
    return((recordTypeLocation *)registryFind(registryID,name));
}
