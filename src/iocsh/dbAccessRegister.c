/* dbAccessRegister.c */
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

#include "dbAccess.h" 
#include "iocInit.h"
#include "dbLoadTemplate.h"
#define epicsExportSharedSymbols
#include "ioccrf.h"
#include "registryRecordType.h"
#include "dbAccessRegister.h"
#include "ioccrf.h"

#ifdef __rtems__
# define dbLoadDatabase dbLoadDatabaseRTEMS
# define dbLoadRecords dbLoadRecordsRTEMS
#endif

/* dbLoadDatabase */
static ioccrfArg dbLoadDatabaseArg0 = { "file name",ioccrfArgString,0};
static ioccrfArg dbLoadDatabaseArg1 = { "path",ioccrfArgString,0};
static ioccrfArg dbLoadDatabaseArg2 = { "substitutions",ioccrfArgString,0};
static ioccrfArg *dbLoadDatabaseArgs[3] =
{
    &dbLoadDatabaseArg0,&dbLoadDatabaseArg1,&dbLoadDatabaseArg2
};
static ioccrfFuncDef dbLoadDatabaseFuncDef =
    {"dbLoadDatabase",3,dbLoadDatabaseArgs};
static void dbLoadDatabaseCallFunc(ioccrfArg **args)
{
    dbLoadDatabase(
        (char *)args[0]->value,(char *)args[1]->value,(char *)args[2]->value);
}

/* dbLoadRecords */
static ioccrfArg dbLoadRecordsArg0 = { "file name",ioccrfArgString,0};
static ioccrfArg dbLoadRecordsArg1 = { "substitutions",ioccrfArgString,0};
static ioccrfArg *dbLoadRecordsArgs[2] = {&dbLoadRecordsArg0,&dbLoadRecordsArg1};
static ioccrfFuncDef dbLoadRecordsFuncDef = {"dbLoadRecords",2,dbLoadRecordsArgs};
static void dbLoadRecordsCallFunc(ioccrfArg **args)
{
    dbLoadRecords((char *)args[0]->value,(char *)args[1]->value);
}

/* dbLoadTemplate */
static ioccrfArg dbLoadTemplateArg0 = { "file name",ioccrfArgString,0};
static ioccrfArg *dbLoadTemplateArgs[1] = {&dbLoadTemplateArg0};
static ioccrfFuncDef dbLoadTemplateFuncDef =
    {"dbLoadTemplate",1,dbLoadTemplateArgs};
static void dbLoadTemplateCallFunc(ioccrfArg **args)
{
    dbLoadTemplate((char *)args[0]->value);
}

/* iocInit */
static ioccrfFuncDef iocInitFuncDef =
    {"iocInit",0,0};
static void iocInitCallFunc(ioccrfArg **args)
{
    iocInit();
}

void epicsShareAPI dbAccessRegister(void)
{
    ioccrfRegister(&dbLoadDatabaseFuncDef,dbLoadDatabaseCallFunc);
    ioccrfRegister(&dbLoadRecordsFuncDef,dbLoadRecordsCallFunc);
    ioccrfRegister(&dbLoadTemplateFuncDef,dbLoadTemplateCallFunc);
    ioccrfRegister(&iocInitFuncDef,iocInitCallFunc);
}
