/* registryDeviceSupport.c */

/* Author:  Marty Kraimer Date:    08JUN99 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/
#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>

#include "registry.h"
#include "dbBase.h"
#include "devSup.h"
#define epicsExportSharedSymbols
#include "registryDeviceSupport.h"

const char *deviceSupport = "device support";
static void *registryID = &deviceSupport;


epicsShareFunc int epicsShareAPI registryDeviceSupportAdd(
    const char *name,struct dset *pdset)
{
    return(registryAdd(registryID,name,(void *)pdset));
}

epicsShareFunc struct dset * epicsShareAPI registryDeviceSupportFind(
    const char *name)
{
    return((struct dset *)registryFind(registryID,name));
}
