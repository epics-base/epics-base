/*registry.c */

/* Author:  Marty Kraimer Date:    08JUN99 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#include "dbDefs.h"
#include "cantProceed.h"
#include "osiFindGlobalSymbol.h"
#include "gpHash.h"
#define epicsExportSharedSymbols
#include "registry.h"

static void *gphPvt = 0;

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
    pentry->userPvt = (void *)data;
    return(TRUE);
}

epicsShareFunc void * epicsShareAPI registryFind(
    void *registryID,const char *name)
{
    GPHENTRY *pentry;
    if(name==0) return(0);
    if(registryID==0) return(osiFindGlobalSymbol(name));
    if(!gphPvt) registryInit(0);
    pentry = gphFind(gphPvt,(char *)name,registryID);
    if(!pentry) return(0);
    return(pentry->userPvt);
}

epicsShareFunc void epicsShareAPI registryFree()
{
    if(!gphPvt) return;
    gphFreeMem(gphPvt);
}

epicsShareFunc int epicsShareAPI registryDump(void)
{
    if(!gphPvt) return(0);
    gphDump(gphPvt);
    return(0);
}
