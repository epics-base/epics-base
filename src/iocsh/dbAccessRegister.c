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

#include "ioccrf.h"
#include "dbAccess.h"
#include "dbLoadTemplate.h"
#include "registryRecordType.h"
#define epicsExportSharedSymbols
#include "dbAccessRegister.h"

/* dbLoadDatabase */
ioccrfArg dbLoadDatabaseArg0 = { "file name",ioccrfArgString,0};
ioccrfArg dbLoadDatabaseArg1 = { "path",ioccrfArgString,0};
ioccrfArg dbLoadDatabaseArg2 = { "substitutions",ioccrfArgString,0};
ioccrfArg *dbLoadDatabaseArgs[3] =
{
    &dbLoadDatabaseArg0,&dbLoadDatabaseArg1,&dbLoadDatabaseArg2
};
ioccrfFuncDef dbLoadDatabaseFuncDef = {"dbLoadDatabase",3,dbLoadDatabaseArgs};
void dbLoadDatabaseCallFunc(ioccrfArg **args)
{
    dbLoadDatabase(
        (char *)args[0]->value,(char *)args[1]->value,(char *)args[2]->value);
}

/* dbLoadRecords */
ioccrfArg dbLoadRecordsArg0 = { "file name",ioccrfArgString,0};
ioccrfArg dbLoadRecordsArg1 = { "substitutions",ioccrfArgString,0};
ioccrfArg *dbLoadRecordsArgs[2] = {&dbLoadRecordsArg0,&dbLoadRecordsArg1};
ioccrfFuncDef dbLoadRecordsFuncDef = {"dbLoadRecords",2,dbLoadRecordsArgs};
void dbLoadRecordsCallFunc(ioccrfArg **args)
{
    dbLoadRecords((char *)args[0]->value,(char *)args[1]->value);
}

/* dbLoadTemplate */
ioccrfArg dbLoadTemplateArg0 = { "file name",ioccrfArgString,0};
ioccrfArg *dbLoadTemplateArgs[1] = {&dbLoadTemplateArg0};
ioccrfFuncDef dbLoadTemplateFuncDef = {"dbLoadTemplate",1,dbLoadTemplateArgs};
void dbLoadTemplateCallFunc(ioccrfArg **args)
{
    dbLoadTemplate((char *)args[0]->value);
}

/* registerRecordDeviceDriver */
ioccrfArg registerRecordDeviceDriverArg0 = { "pdbbase",ioccrfArgPdbbase,0};
ioccrfArg *registerRecordDeviceDriverArgs[1] = {&registerRecordDeviceDriverArg0};
ioccrfFuncDef registerRecordDeviceDriverFuncDef =
    {"registerRecordDeviceDriver",1,registerRecordDeviceDriverArgs};
void registerRecordDeviceDriverCallFunc(ioccrfArg **args)
{
    registerRecordDeviceDriver(pdbbase);
}

/* iocInit */
ioccrfFuncDef iocInitFuncDef =
    {"iocInit",0,0};
void iocInitCallFunc(ioccrfArg **args)
{
    iocInit();
}

void epicsShareAPI dbAccessRegisterInit(void)
{
    ioccrfRegister(&dbLoadDatabaseFuncDef,dbLoadDatabaseCallFunc);
    ioccrfRegister(&dbLoadRecordsFuncDef,dbLoadRecordsCallFunc);
    ioccrfRegister(&dbLoadTemplateFuncDef,dbLoadTemplateCallFunc);
    ioccrfRegister(&registerRecordDeviceDriverFuncDef,
        registerRecordDeviceDriverCallFunc);
    ioccrfRegister(&iocInitFuncDef,iocInitCallFunc);
}
