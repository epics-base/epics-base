/* iocshRegisterCommon.c */
/* Author:  Marty Kraimer Date: 26APR2000 */

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

#define epicsExportSharedSymbols
#include "dbStaticRegister.h"
#include "iocUtilRegister.h"
#include "dbTestRegister.h"
#include "dbBkptRegister.h"
#include "dbCaTestRegister.h"
#include "caTestRegister.h"
#include "dbAccessRegister.h"
#include "asTestRegister.h"
#include "envRegister.h"
#include "iocshRegisterCommon.h"
#include "osiRegister.h"
#include "iocsh.h"

void epicsShareAPI iocshRegisterCommon(void)
{
    osiRegister();
    iocUtilRegister();
    dbStaticRegister();
    dbTestRegister();
    dbBkptRegister();
    dbCaTestRegister();
    caTestRegister();
    dbAccessRegister();
    asTestRegister();
    envRegister();
}
