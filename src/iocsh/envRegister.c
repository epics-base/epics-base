/* envRegister.c */
/* Author:  Marty Kraimer Date: 19MAY2000 */

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

#include "envDefs.h"
#define epicsExportSharedSymbols
#include "ioccrf.h"
#include "envRegister.h"

/* epicsPrtEnvParams */
static ioccrfFuncDef epicsPrtEnvParamsFuncDef = {"epicsPrtEnvParams",0,0};
static void epicsPrtEnvParamsCallFunc(ioccrfArg **args) { epicsPrtEnvParams();}

void epicsShareAPI envRegister(void)
{
    ioccrfRegister(&epicsPrtEnvParamsFuncDef,epicsPrtEnvParamsCallFunc);
}
