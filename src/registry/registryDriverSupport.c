/* registryDriverSupport.c */

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
#include "drvSup.h"
#define epicsExportSharedSymbols
#include "registryDriverSupport.h"

const char *driverSupport = "driver support";
static void *registryID = &driverSupport;


epicsShareFunc int epicsShareAPI registryDriverSupportAdd(
    const char *name,struct drvet *pdrvet)
{
    return(registryAdd(registryID,name,(void *)pdrvet));
}

epicsShareFunc struct drvet * epicsShareAPI registryDriverSupportFind(
    const char *name)
{
    return((struct drvet *)registryFind(registryID,name));
}
