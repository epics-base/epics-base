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

/* dbLoadDatabase */
static const ioccrfArg dbLoadDatabaseArg0 = { "file name",ioccrfArgString};
static const ioccrfArg dbLoadDatabaseArg1 = { "path",ioccrfArgString};
static const ioccrfArg dbLoadDatabaseArg2 = { "substitutions",ioccrfArgString};
static const ioccrfArg *dbLoadDatabaseArgs[3] =
{
    &dbLoadDatabaseArg0,&dbLoadDatabaseArg1,&dbLoadDatabaseArg2
};
static const ioccrfFuncDef dbLoadDatabaseFuncDef =
    {"dbLoadDatabase",3,dbLoadDatabaseArgs};
static void dbLoadDatabaseCallFunc(const ioccrfArgBuf *args)
{
    dbLoadDatabase(args[0].sval,args[1].sval,args[2].sval);
}

/* dbLoadRecords */
static const ioccrfArg dbLoadRecordsArg0 = { "file name",ioccrfArgString};
static const ioccrfArg dbLoadRecordsArg1 = { "substitutions",ioccrfArgString};
static const ioccrfArg *dbLoadRecordsArgs[2] = {&dbLoadRecordsArg0,&dbLoadRecordsArg1};
static const ioccrfFuncDef dbLoadRecordsFuncDef = {"dbLoadRecords",2,dbLoadRecordsArgs};
static void dbLoadRecordsCallFunc(const ioccrfArgBuf *args)
{
    dbLoadRecords(args[0].sval,args[1].sval);
}

/* dbLoadTemplate */
static const ioccrfArg dbLoadTemplateArg0 = { "file name",ioccrfArgString};
static const ioccrfArg *dbLoadTemplateArgs[1] = {&dbLoadTemplateArg0};
static const ioccrfFuncDef dbLoadTemplateFuncDef =
    {"dbLoadTemplate",1,dbLoadTemplateArgs};
static void dbLoadTemplateCallFunc(const ioccrfArgBuf *args)
{
    dbLoadTemplate(args[0].sval);
}

/* iocInit */
static const ioccrfFuncDef iocInitFuncDef =
    {"iocInit",0,0};
static void iocInitCallFunc(const ioccrfArgBuf *args)
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
