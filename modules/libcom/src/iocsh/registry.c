/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*registry.c */

/* Author:  Marty Kraimer Date:    08JUN99 */

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#define epicsExportSharedSymbols
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
    
epicsShareFunc int epicsShareAPI registrySetTableSize(int size)
{
    if(gphPvt) {
        printf("registryInit already called\n");
        return(-1);
    }
    registryInit(size);
    return(0);
}
    

epicsShareFunc int epicsShareAPI registryAdd(
    void *registryID,const char *name,void *data)
{
    GPHENTRY *pentry;
    if(!gphPvt) registryInit(0);
    pentry = gphAdd(gphPvt,name,registryID);
    if(!pentry) return(FALSE);
    pentry->userPvt = data;
    return(TRUE);
}

epicsShareFunc int epicsShareAPI registryChange(
    void *registryID,const char *name,void *data)
{
    GPHENTRY *pentry;
    if(!gphPvt) registryInit(0);
    pentry = gphFind(gphPvt,(char *)name,registryID);
    if(!pentry) return(FALSE);
    pentry->userPvt = data;
    return(TRUE);
}

epicsShareFunc void * epicsShareAPI registryFind(
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

epicsShareFunc void epicsShareAPI registryFree(void)
{
    if(!gphPvt) return;
    gphFreeMem(gphPvt);
    gphPvt = 0;
}

epicsShareFunc int epicsShareAPI registryDump(void)
{
    if(!gphPvt) return(0);
    gphDump(gphPvt);
    return(0);
}
