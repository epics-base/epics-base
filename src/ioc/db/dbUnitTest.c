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

#define EPICS_PRIVATE_API

#include "dbmf.h"
#include "epicsUnitTest.h"
#include "osiFileName.h"
#include "osiUnistd.h"
#include "registry.h"
#include "epicsEvent.h"

#define epicsExportSharedSymbols
#include "dbAccess.h"
#include "dbBase.h"
#include "dbChannel.h"
#include "dbEvent.h"
#include "dbStaticLib.h"
#include "dbUnitTest.h"
#include "initHooks.h"
#include "iocInit.h"

static dbEventCtx testEvtCtx;
static epicsMutexId testEvtLock;
static ELLLIST testEvtList; /* holds testMonitor::node */

struct testMonitor {
    ELLNODE node;
    dbEventSubscription sub;
    dbChannel *chan;
    epicsEventId event;
    unsigned count;
};

void testdbPrepare(void)
{
    if(!testEvtLock)
        testEvtLock = epicsMutexMustCreate();
}

void testdbReadDatabase(const char* file,
                        const char* path,
                        const char* substitutions)
{
    if(!path)
        path = "." OSI_PATH_LIST_SEPARATOR ".." OSI_PATH_LIST_SEPARATOR
                "../O.Common" OSI_PATH_LIST_SEPARATOR "O.Common";
    if(dbReadDatabase(&pdbbase, file, path, substitutions)) {
        char buf[100];
        const char *cwd = getcwd(buf, sizeof(buf));
        if(!cwd)
            cwd = "<directory too long>";
        testAbort("Failed to load test database\ndbReadDatabase(%s,%s,%s)\n from: \"%s\"",
                  file, path, substitutions, cwd);
    }
}

void testIocInitOk(void)
{
    if(iocBuildIsolated() || iocRun())
        testAbort("Failed to start up test database");
    if(!(testEvtCtx=db_init_events()))
        testAbort("Failed to initialize test dbEvent context");
    if(DB_EVENT_OK!=db_start_events(testEvtCtx, "CAS-test", NULL, NULL, epicsThreadPriorityCAServerLow))
        testAbort("Failed to start test dbEvent context");
}

void testIocShutdownOk(void)
{
    epicsMutexMustLock(testEvtLock);
    if(ellCount(&testEvtList))
        testDiag("Warning, testing monitors still active at testIocShutdownOk()");
    epicsMutexUnlock(testEvtLock);

    db_close_events(testEvtCtx);
    testEvtCtx = NULL;
    if(iocShutdown())
        testAbort("Failed to shutdown test database");
}

void testdbCleanup(void)
{
    dbFreeBase(pdbbase);
    db_cleanup_events();
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

    if (dbNameToAddr(pv, &addr)) {
        testFail("Missing PV \"%s\"", pv);
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
    OP(DBR_INT64, long long, int64);
    OP(DBR_UINT64, unsigned long long, uInt64);
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

void testdbPutFieldOk(const char* pv, short dbrType, ...)
{
    long ret;
    va_list ap;

    va_start(ap, dbrType);
    ret = testdbVPutField(pv, dbrType, ap);
    va_end(ap);

    testOk(ret==0, "dbPutField(\"%s\", %d, ...) -> %#lx (%s)", pv, dbrType, ret, errSymMsg(ret));
}

void testdbPutFieldFail(long status, const char* pv, short dbrType, ...)
{
    long ret;
    va_list ap;

    va_start(ap, dbrType);
    ret = testdbVPutField(pv, dbrType, ap);
    va_end(ap);

    testOk(ret==status, "dbPutField(\"%s\", %d, ...) -> %#lx (%s) == %#lx (%s)",
           pv, dbrType, status, errSymMsg(status), ret, errSymMsg(ret));
}

void testdbGetFieldEqual(const char* pv, short dbrType, ...)
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
        testFail("Missing PV \"%s\"", pv);
        return;
    }

    status = dbGetField(&addr, dbrType, pod.bytes, NULL, &nReq, NULL);
    if (status) {
        testFail("dbGetField(\"%s\", %d, ...) -> %#lx (%s)", pv, dbrType, status, errSymMsg(status));
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
    OP(DBR_INT64, long long, int64, "%lld");
    OP(DBR_UINT64, unsigned long long, uInt64, "%llu");
    OP(DBR_FLOAT, double, float32, "%e");
    OP(DBR_DOUBLE, double, float64, "%e");
    OP(DBR_ENUM, int, enum16, "%d");
#undef OP
    default:
        testFail("dbGetField(\"%s\", %d) -> unsupported dbf", pv, dbrType);
    }
}

void testdbPutArrFieldOk(const char* pv, short dbrType, unsigned long count, const void *pbuf)
{
    DBADDR addr;
    long status;

    if (dbNameToAddr(pv, &addr)) {
        testFail("Missing PV \"%s\"", pv);
        return;
    }

    status = dbPutField(&addr, dbrType, pbuf, count);

    testOk(status==0, "dbPutField(\"%s\", dbr=%d, count=%lu, ...) -> %ld", pv, dbrType, count, status);
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
            OP(DBR_INT64, long long, "%lld");
            OP(DBR_UINT64, unsigned long long, "%llu");
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

    if (dbNameToAddr(pv, &addr))
        testAbort("Missing record \"%s\"", pv);

    return addr.precord;
}

static
void testmonupdate(void *user_arg, struct dbChannel *chan,
                   int eventsRemaining, struct db_field_log *pfl)
{
    testMonitor *mon = user_arg;

    epicsMutexMustLock(testEvtLock);
    mon->count++;
    epicsMutexUnlock(testEvtLock);
    epicsEventMustTrigger(mon->event);
}

testMonitor* testMonitorCreate(const char* pvname, unsigned mask, unsigned opt)
{
    long status;
    testMonitor *mon;
    dbChannel *chan;
    assert(testEvtCtx);

    mon = callocMustSucceed(1, sizeof(*mon), "testMonitorCreate");

    mon->event = epicsEventMustCreate(epicsEventEmpty);

    chan = mon->chan = dbChannelCreate(pvname);
    if(!chan)
        testAbort("testMonitorCreate - dbChannelCreate(\"%s\") fails", pvname);
    if(!!(status=dbChannelOpen(chan)))
        testAbort("testMonitorCreate - dbChannelOpen(\"%s\") fails w/ %ld", pvname, status);

    mon->sub = db_add_event(testEvtCtx, chan, &testmonupdate, mon, mask);
    if(!mon->sub)
        testAbort("testMonitorCreate - db_add_event(\"%s\") fails", pvname);

    db_event_enable(mon->sub);

    epicsMutexMustLock(testEvtLock);
    ellAdd(&testEvtList, &mon->node);
    epicsMutexUnlock(testEvtLock);

    return mon;
}

void testMonitorDestroy(testMonitor *mon)
{
    if(!mon) return;

    db_event_disable(mon->sub);

    epicsMutexMustLock(testEvtLock);
    ellDelete(&testEvtList, &mon->node);
    epicsMutexUnlock(testEvtLock);

    db_cancel_event(mon->sub);

    dbChannelDelete(mon->chan);

    epicsEventDestroy(mon->event);

    free(mon);
}

void testMonitorWait(testMonitor *mon)
{
    static const double delay = 60.0;

    switch(epicsEventWaitWithTimeout(mon->event, delay))
    {
    case epicsEventOK:
        return;
    case epicsEventWaitTimeout:
    default:
        testAbort("testMonitorWait() exceeded %g second timeout", delay);
    }
}

unsigned testMonitorCount(testMonitor *mon, unsigned reset)
{
    unsigned count;
    epicsMutexMustLock(testEvtLock);
    count = mon->count;
    if(reset) {
        mon->count = 0;
        epicsEventWaitWithTimeout(mon->event, 0); /* clear the event */
    }
    epicsMutexUnlock(testEvtLock);
    return count;
}

