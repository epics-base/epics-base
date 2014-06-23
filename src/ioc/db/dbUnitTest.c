/*************************************************************************\
* Copyright (c) 2013 Brookhaven National Laboratory.
* Copyright (c) 2013 ITER Organization.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
 \*************************************************************************/

/*
 *  Author: Michael Davidsaver <mdavidsaver@bnl.gov>
 *          Ralph Lange <Ralph.Lange@gmx.de>
 */

#include <string.h>

#include "epicsUnitTest.h"
#include "osiFileName.h"
#include "dbmf.h"
#include "registry.h"
#define epicsExportSharedSymbols
#include "iocInit.h"
#include "initHooks.h"
#include "dbBase.h"
#include "dbAccess.h"
#include "dbStaticLib.h"

#include "dbUnitTest.h"

void testdbPrepare(void)
{
    /* No-op at the moment */
}

void testdbReadDatabase(const char* file,
                        const char* path,
                        const char* substitutions)
{
    if(!path)
        path = "." OSI_PATH_LIST_SEPARATOR ".." OSI_PATH_LIST_SEPARATOR
                "../O.Common" OSI_PATH_LIST_SEPARATOR "O.Common";
    if(dbReadDatabase(&pdbbase, file, path, substitutions))
        testAbort("Failed to load test database\ndbReadDatabase(%s,%s,%s)",
                  file, path, substitutions);
}

int testiocInit(void)
{
    return iocBuildNoCA() || iocRun();
}

int testiocShutdown(void)
{
    return iocShutdown();
}

void testdbCleanup(void)
{
    dbFreeBase(pdbbase);
    initHookFree();
    registryFree();
    pdbbase = NULL;
    dbmfFreeChunks();
}

long testdbPutField(const char* pv, short dbrType, ...)
{
    long ret;
    va_list ap;
    va_start(ap, dbrType);
    ret = testVdbPutField(pv, dbrType, ap);
    va_end(ap);
    return ret;
}

union anybuf {
    epicsAny val;
    char bytes[sizeof(epicsAny)];
};

long testVdbPutField(const char* pv, short dbrType, va_list ap)
{
    DBADDR addr;
    union anybuf pod;

    if(dbNameToAddr(pv, &addr))
        testAbort("Missing PV %s", pv);

    switch(dbrType) {
    case DBR_STRING: {
        const char *uarg = va_arg(ap,char*);
        epicsOldString buffer;
        strncpy(buffer, uarg, sizeof(buffer));
        buffer[sizeof(buffer)-1] = '\0';
        return dbPutField(&addr, dbrType, buffer, 1);
    }

#define OP(DBR,Type,mem) case DBR: {pod.val.mem = va_arg(ap,Type); break;}
    OP(DBR_CHAR, int, int8);
    OP(DBR_UCHAR, int, uInt8);
    OP(DBR_SHORT, int, int16);
    OP(DBR_USHORT, int, uInt16);
    OP(DBR_LONG, int, int32);
    OP(DBR_ULONG, unsigned int, uInt32);
    OP(DBR_FLOAT, double, float32);
    OP(DBR_DOUBLE, double, float64);
    OP(DBR_ENUM, int, enum16);
#undef OP
    default:
        testAbort("invalid DBR: dbPutField(%s, %d, ...)",
                  addr.precord->name, dbrType);
    }

    return dbPutField(&addr, dbrType, pod.bytes, 1);
}

dbCommon* testGetRecord(const char* pv)
{
    DBADDR addr;

    if(dbNameToAddr(pv, &addr))
        testAbort("Missing PV %s", pv);

    return addr.precord;
}
