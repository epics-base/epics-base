/*************************************************************************\
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Which requestType leads dbProcessNotify() to process the record, per the
 * three rules stated in dbNotify.h.
 */

#include <string.h>

#include "dbAccess.h"
#include "dbChannel.h"
#include "dbNotify.h"
#include "epicsEvent.h"
#include "errlog.h"

#include "dbUnitTest.h"
#include "testMain.h"

void dbTestIoc_registerRecordDeviceDriver(struct dbBase *);

typedef struct {
    processNotify notify;
    epicsEventId done;
    int puts;               /* putCallback calls */
    int gets;               /* getCallback calls */
    int didPut;             /* what putCallback reports back */
    /* Read in doneCallback: dbNotifyCancel() overwrites notify.status. */
    notifyStatus status;
    int wasProcessed;
} request;

static int putCallback(processNotify *ppn, notifyPutType type)
{
    request *r = (request *) ppn->usrPvt;
    epicsUInt8 one = 1;

    r->puts++;
    if (!r->didPut)
        return 0;
    if (dbChannelPut(ppn->chan, DBR_UCHAR, &one, 1))
        ppn->status = notifyError;
    return 1;
}

static void getCallback(processNotify *ppn, notifyGetType type)
{
    request *r = (request *) ppn->usrPvt;
    epicsUInt8 scratch;
    long count = 1;

    r->gets++;
    dbChannelGet(ppn->chan, DBR_UCHAR, &scratch, NULL, &count, NULL);
}

static void doneCallback(processNotify *ppn)
{
    request *r = (request *) ppn->usrPvt;

    r->status = ppn->status;
    r->wasProcessed = ppn->wasProcessed;
    epicsEventMustTrigger(r->done);
}

/* One complete request against pv, run to its doneCallback. */
static void run(request *r, const char *what, const char *pv,
    notifyRequestType type, int didPut)
{
    memset(r, 0, sizeof(*r));
    r->didPut = didPut;
    r->done = epicsEventMustCreate(epicsEventEmpty);
    r->notify.requestType = type;
    r->notify.putCallback = putCallback;
    r->notify.getCallback = getCallback;
    r->notify.doneCallback = doneCallback;
    r->notify.usrPvt = r;
    r->notify.chan = dbChannelCreate(pv);
    if (!r->notify.chan || dbChannelOpen(r->notify.chan))
        testAbort("dbChannelCreate(\"%s\") failed", pv);

    testDiag("%s: %s", what, pv);
    dbProcessNotify(&r->notify);
    testOk(epicsEventWaitWithTimeout(r->done, 10.0) == epicsEventWaitOK,
        "doneCallback ran");

    dbNotifyCancel(&r->notify);
    dbChannelDelete(r->notify.chan);
    epicsEventDestroy(r->done);
}

MAIN(dbNotifyTest)
{
    request r;

    testPlan(19);

    testdbPrepare();
    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);
    dbTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("dbNotifyTest.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    run(&r, "rule 1, process request on a passive record",
        "passive.VAL", processRequest, 0);
    testOk1(r.wasProcessed == 1);
    testOk1(r.status == notifyOK);
    testOk(r.puts == 0 && r.gets == 0,
        "processRequest calls neither the put nor the get callback");

    run(&r, "rule 1 does not apply when the record is not passive",
        "scanned.VAL", processRequest, 0);
    testOk1(r.wasProcessed == 0);
    testOk1(r.status == notifyOK);

    run(&r, "rule 2, a put through PROC",
        "passive.PROC", putProcessRequest, 1);
    testOk1(r.wasProcessed == 1);
    testOk1(r.puts == 1);

    run(&r, "rule 2 needs the put to have happened",
        "passive.PROC", putProcessRequest, 0);
    testOk1(r.wasProcessed == 0);
    testOk1(r.puts == 1);

    run(&r, "rule 2 needs PROC or a process-passive field",
        "passive.VAL", putProcessRequest, 1);
    testOk1(r.wasProcessed == 0);
    testOk1(r.puts == 1);

    run(&r, "rule 3, processGet on a passive record",
        "passive.VAL", processGetRequest, 0);
    testOk1(r.wasProcessed == 1);
    testOk1(r.gets == 1);

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
