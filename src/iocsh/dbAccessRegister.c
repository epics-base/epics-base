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
#include "iocsh.h"
#include "registryRecordType.h"
#include "dbAccessRegister.h"
#include "asSubRecordFunctions.h"
#include "iocsh.h"

/* dbLoadDatabase */
static const iocshArg dbLoadDatabaseArg0 = { "file name",iocshArgString};
static const iocshArg dbLoadDatabaseArg1 = { "path",iocshArgString};
static const iocshArg dbLoadDatabaseArg2 = { "substitutions",iocshArgString};
static const iocshArg * const dbLoadDatabaseArgs[3] =
{
    &dbLoadDatabaseArg0,&dbLoadDatabaseArg1,&dbLoadDatabaseArg2
};
static const iocshFuncDef dbLoadDatabaseFuncDef =
    {"dbLoadDatabase",3,dbLoadDatabaseArgs};
static void dbLoadDatabaseCallFunc(const iocshArgBuf *args)
{
    dbLoadDatabase(args[0].sval,args[1].sval,args[2].sval);
}

/* dbLoadRecords */
static const iocshArg dbLoadRecordsArg0 = { "file name",iocshArgString};
static const iocshArg dbLoadRecordsArg1 = { "substitutions",iocshArgString};
static const iocshArg * const dbLoadRecordsArgs[2] = {&dbLoadRecordsArg0,&dbLoadRecordsArg1};
static const iocshFuncDef dbLoadRecordsFuncDef = {"dbLoadRecords",2,dbLoadRecordsArgs};
static void dbLoadRecordsCallFunc(const iocshArgBuf *args)
{
    dbLoadRecords(args[0].sval,args[1].sval);
}

/* dbLoadTemplate */
static const iocshArg dbLoadTemplateArg0 = { "file name",iocshArgString};
static const iocshArg * const dbLoadTemplateArgs[1] = {&dbLoadTemplateArg0};
static const iocshFuncDef dbLoadTemplateFuncDef =
    {"dbLoadTemplate",1,dbLoadTemplateArgs};
static void dbLoadTemplateCallFunc(const iocshArgBuf *args)
{
    dbLoadTemplate(args[0].sval);
}

/* iocInit */
static const iocshFuncDef iocInitFuncDef =
    {"iocInit",0,0};
static void iocInitCallFunc(const iocshArgBuf *args)
{
    iocInit();
}

void epicsShareAPI dbAccessRegister(void)
{
    iocshRegister(&dbLoadDatabaseFuncDef,dbLoadDatabaseCallFunc);
    iocshRegister(&dbLoadRecordsFuncDef,dbLoadRecordsCallFunc);
    iocshRegister(&dbLoadTemplateFuncDef,dbLoadTemplateCallFunc);
    iocshRegister(&iocInitFuncDef,iocInitCallFunc);
    asSubRecordFunctionsRegister();
}
