/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* registryDeviceSupport.c */

/* Author:  Marty Kraimer Date:    08JUN99 */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>

#include "dbBase.h"
#include "devSup.h"
#define epicsExportSharedSymbols
#include "registry.h"
#include "registryDeviceSupport.h"

const char *deviceSupport = "device support";
static void *registryID = (void *)&deviceSupport;


epicsShareFunc int epicsShareAPI registryDeviceSupportAdd(
    const char *name,const struct dset *pdset)
{
    return(registryAdd(registryID,name,(void *)pdset));
}

epicsShareFunc struct dset * epicsShareAPI registryDeviceSupportFind(
    const char *name)
{
    return((struct dset *)registryFind(registryID,name));
}
