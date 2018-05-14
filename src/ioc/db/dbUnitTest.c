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

#include "dbmf.h"
#include "epicsUnitTest.h"
#include "osiFileName.h"
#include "registry.h"
#include "epicsThread.h"

#define epicsExportSharedSymbols
#include "dbAccess.h"
#include "dbBase.h"
#include "dbStaticLib.h"
#include "dbUnitTest.h"
#include "initHooks.h"
#include "iocInit.h"

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

void testIocInitOk(void)
{
    if(iocBuildIsolated() || iocRun())
        testAbort("Failed to start up test database");
}

void testIocShutdownOk(void)
{
    if(iocShutdown())
        testAbort("Failed to shutdown test database");
}

void testdbCleanup(void)
{
    dbFreeBase(pdbbase);
    initHookFree();
    registryFree();
    pdbbase = NULL;
    dbmfFreeChunks();
}

union anybuf {
    epicsAny val;
    char valStr[MAX_STRING_SIZE];
    char bytes[sizeof(epicsAny)];
};

long testdbVPutField(const char* pv, short dbrType, va_list ap)
{
    DBADDR addr;
    union anybuf pod;

    if(dbNameToAddr(pv, &addr)) {
        testFail("Missing PV %s", pv);
        return S_dbLib_recNotFound;
    }

    switch(dbrType) {
    case DBR_STRING: {
        const char *uarg = va_arg(ap,char*);
        strncpy(pod.valStr, uarg, sizeof(pod.valStr));
        pod.valStr[sizeof(pod.valStr)-1] = '\0';
        return dbPutField(&addr, dbrType, pod.valStr, 1);
    }

    /* The Type parameter takes into consideration
     * the C language rules for promotion of argument types
     * in variadic functions.
     */
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
        testFail("invalid DBR: dbPutField(\"%s\", %d, ...)",
                  addr.precord->name, dbrType);
        return S_db_badDbrtype;
    }

    return dbPutField(&addr, dbrType, pod.bytes, 1);
}

void testdbPutFieldOk(const char* pv, int dbrType, ...)
{
    long ret;
    va_list ap;

    va_start(ap, dbrType);
    ret = testdbVPutField(pv, dbrType, ap);
    va_end(ap);

    testOk(ret==0, "dbPutField(%s, %d, ...) == %ld", pv, dbrType, ret);
}

void testdbPutFieldFail(long status, const char* pv, int dbrType, ...)
{
    long ret;
    va_list ap;

    va_start(ap, dbrType);
    ret = testdbVPutField(pv, dbrType, ap);
    va_end(ap);

    if(ret==status)
        testPass("dbPutField(\"%s\", %d, ...) == %ld", pv, dbrType, status);
    else
        testFail("dbPutField(\"%s\", %d, ...) != %ld (%ld)", pv, dbrType, status, ret);
}

void testdbGetFieldEqual(const char* pv, int dbrType, ...)
{
    va_list ap;

    va_start(ap, dbrType);
    testdbVGetFieldEqual(pv, dbrType, ap);
    va_end(ap);
}

void testdbVGetFieldEqual(const char* pv, short dbrType, va_list ap)
{
    DBADDR addr;
    long nReq = 1;
    union anybuf pod;
    long status;

    if(dbNameToAddr(pv, &addr)) {
        testFail("Missing PV %s", pv);
        return;
    }

    status = dbGetField(&addr, dbrType, pod.bytes, NULL, &nReq, NULL);
    if(status) {
        testFail("dbGetField(\"%s\",%d,...) returns %ld", pv, dbrType, status);
        return;
    } else if(nReq==0) {
        testFail("dbGetField(\"%s\", %d, ...) -> zero length", pv, dbrType);
        return;
    }

    switch(dbrType) {
    case DBR_STRING: {
        const char *expect = va_arg(ap, char*);
        testOk(strcmp(expect, pod.valStr)==0,
               "dbGetField(\"%s\", %d) -> \"%s\" == \"%s\"",
               pv, dbrType, expect, pod.valStr);
        break;
    }
#define OP(DBR,Type,mem,pat) case DBR: {Type expect = va_arg(ap,Type); \
    testOk(expect==pod.val.mem, "dbGetField(\"%s\", %d) -> " pat " == " pat, \
        pv, dbrType, expect, (Type)pod.val.mem); break;}

    OP(DBR_CHAR, int, int8, "%d");
    OP(DBR_UCHAR, int, uInt8, "%d");
    OP(DBR_SHORT, int, int16, "%d");
    OP(DBR_USHORT, int, uInt16, "%d");
    OP(DBR_LONG, int, int32, "%d");
    OP(DBR_ULONG, unsigned int, uInt32, "%u");
    OP(DBR_FLOAT, double, float32, "%e");
    OP(DBR_DOUBLE, double, float64, "%e");
    OP(DBR_ENUM, int, enum16, "%d");
#undef OP
    }
}

void testdbGetArrFieldEqual(const char* pv, short dbfType, long nRequest, unsigned long cnt, const void *pbufraw)
{
    DBADDR addr;
    const long vSize = dbValueSize(dbfType);
    const long nStore = vSize * nRequest;
    long status;
    char *gbuf, *gstore;
    const char *pbuf = pbufraw;

    if(dbNameToAddr(pv, &addr)) {
        testFail("Missing PV \"%s\"", pv);
        return;
    }

    gbuf = gstore = malloc(nStore);
    if(!gbuf && nStore!=0) { /* note that malloc(0) is allowed to return NULL on success */
        testFail("Allocation failed esize=%ld total=%ld", vSize, nStore);
        return;
    }

    status = dbGetField(&addr, dbfType, gbuf, NULL, &nRequest, NULL);
    if (status) {
        testFail("dbGetField(\"%s\", %d, ...) -> %#lx", pv, dbfType, status);

    } else {
        unsigned match = nRequest==cnt;
        long n, N = nRequest < cnt ? nRequest : cnt;

        if(!match)
            testDiag("Length mis-match.  expected=%lu actual=%lu", cnt, nRequest);

        for(n=0; n<N; n++, gbuf+=vSize, pbuf+=vSize) {

            switch(dbfType) {
            case DBR_STRING: {
                const char *expect = (const char*)pbuf,
                           *actual = (const char*)gbuf;
                /* expected (pbuf) is allowed to omit storage for trailing nils for last element */
                unsigned int eq = strncmp(expect, actual, MAX_STRING_SIZE)==0 && actual[MAX_STRING_SIZE-1]=='\0';
                match &= eq;
                if(!eq)
                    testDiag("[%lu] = expected=\"%s\" actual=\"%s\"", n, expect, actual);
                break;
            }
#define OP(DBR,Type,pat) case DBR: {Type expect = *(Type*)pbuf, actual = *(Type*)gbuf; assert(vSize==sizeof(Type)); match &= expect==actual; \
    if(expect!=actual) testDiag("[%lu] expected=" pat " actual=" pat, n, expect, actual); break;}

            OP(DBR_CHAR, char, "%c");
            OP(DBR_UCHAR, unsigned char, "%u");
            OP(DBR_SHORT, short, "%d");
            OP(DBR_USHORT, unsigned short, "%u");
            OP(DBR_LONG, int, "%d");
            OP(DBR_ULONG, unsigned int, "%u");
            OP(DBR_FLOAT, float, "%e");
            OP(DBR_DOUBLE, double, "%e");
            OP(DBR_ENUM, int, "%d");
#undef OP
            }
        }

        testOk(match, "dbGetField(\"%s\", dbrType=%d, nRequest=%ld ...) match", pv, dbfType, nRequest);
    }

    free(gstore);
}

dbCommon* testdbRecordPtr(const char* pv)
{
    DBADDR addr;

    if(dbNameToAddr(pv, &addr))
        testAbort("Missing record %s", pv);

    return addr.precord;
}

static
epicsMutexId test_global;

static
epicsThreadOnceId test_global_once = EPICS_THREAD_ONCE_INIT;

static
void test_global_init(void* ignored)
{
    test_global = epicsMutexMustCreate();
}

void testGlobalLock(void)
{
    epicsThreadOnce(&test_global_once, &test_global_init, NULL);
    epicsMutexMustLock(test_global);
}

void testGlobalUnlock(void)
{
    epicsMutexUnlock(test_global);
}
