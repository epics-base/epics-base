/* registryFunction.c */

/* Author:  Marty Kraimer Date:    01MAY2000 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/
#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>

#define epicsExportSharedSymbols
#include "registry.h"
#include "registryFunction.h"

const char *function = "function";
static void *registryID = (void *)&function;


epicsShareFunc int epicsShareAPI registryFunctionAdd(
    const char *name,REGISTRYFUNCTION func)
{
    return(registryAdd(registryID,name,(void *)func));
}

epicsShareFunc REGISTRYFUNCTION epicsShareAPI registryFunctionFind(
    const char *name)
{
    return((REGISTRYFUNCTION)registryFind(registryID,name));
}
