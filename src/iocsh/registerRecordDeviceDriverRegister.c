/* registerRecordDeviceDriverRegister.c */
/* Author:  Marty Kraimer Date: 04MAY2000 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "dbAccess.h"
#include "ioccrf.h"
#include "registerRecordDeviceDriverRegister.h"

/* registerRecordDeviceDriver */
static ioccrfArg registerRecordDeviceDriverArg0 =
    { "pdbbase",ioccrfArgPdbbase,0};
static ioccrfArg *registerRecordDeviceDriverArgs[1] =
    {&registerRecordDeviceDriverArg0};
static ioccrfFuncDef registerRecordDeviceDriverFuncDef =
    {"registerRecordDeviceDriver",1,registerRecordDeviceDriverArgs};
static void registerRecordDeviceDriverCallFunc(ioccrfArg **args)
{
    registerRecordDeviceDriver(pdbbase);
}

void registerRecordDeviceDriverRegister(void)
{
    ioccrfRegister(
        &registerRecordDeviceDriverFuncDef,registerRecordDeviceDriverCallFunc);
}
