/* registryRecordType.c */

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
#define epicsExportSharedSymbols
#include "registryRecordType.h"

const char *recordType = "record type";
static void *registryID = &recordType;


epicsShareFunc int epicsShareAPI registryRecordTypeAdd(
    const char *name,recordTypeLocation *prtl)
{
    return(registryAdd(registryID,name,(void *)prtl));
}

epicsShareFunc recordTypeLocation * epicsShareAPI registryRecordTypeFind(
    const char *name)
{
    return((recordTypeLocation *)registryFind(registryID,name));
}
