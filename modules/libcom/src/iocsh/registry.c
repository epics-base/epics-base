/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*registry.c */

/* Author:  Marty Kraimer Date:    08JUN99 */

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "dbDefs.h"
#include "cantProceed.h"
#include "epicsFindSymbol.h"
#include "gpHash.h"
#include "registry.h"

static struct gphPvt *gphPvt = 0;

static void registryInit(int tableSize)
{
    if(tableSize==0) tableSize = DEFAULT_TABLE_SIZE;
    gphInitPvt(&gphPvt,tableSize);
    if(!gphPvt) cantProceed("registry why did gphInitPvt fail\n");
}

LIBCOM_API int epicsStdCall registrySetTableSize(int size)
{
    if(gphPvt) {
        printf("registryInit already called\n");
        return(-1);
    }
    registryInit(size);
    return(0);
}


LIBCOM_API int epicsStdCall registryAdd(
    void *registryID,const char *name,void *data)
{
    GPHENTRY *pentry;
    if(!gphPvt) registryInit(0);
    pentry = gphAdd(gphPvt,name,registryID);
    if(!pentry) return(FALSE);
    pentry->userPvt = data;
    return(TRUE);
}

LIBCOM_API int epicsStdCall registryChange(
    void *registryID,const char *name,void *data)
{
    GPHENTRY *pentry;
    if(!gphPvt) registryInit(0);
    pentry = gphFind(gphPvt,(char *)name,registryID);
    if(!pentry) return(FALSE);
    pentry->userPvt = data;
    return(TRUE);
}

LIBCOM_API void * epicsStdCall registryFind(
    void *registryID,const char *name)
{
    GPHENTRY *pentry;
    if(name==0) return(0);
    if(registryID==0) return(epicsFindSymbol(name));
    if(!gphPvt) registryInit(0);
    pentry = gphFind(gphPvt,(char *)name,registryID);
    if(!pentry) return(0);
    return(pentry->userPvt);
}

LIBCOM_API void epicsStdCall registryFree(void)
{
    if(!gphPvt) return;
    gphFreeMem(gphPvt);
    gphPvt = 0;
}

LIBCOM_API int epicsStdCall registryDump(void)
{
    if(!gphPvt) return(0);
    gphDump(gphPvt);
    return(0);
}
