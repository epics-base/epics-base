/*************************************************************************\
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <Ralph.Lange@bessy.de>
 */

#include <string.h>

#include "dbStaticLib.h"
#include "dbAccessDefs.h"
#include "chfPlugin.h"
#include "errlog.h"
#include "epicsUnitTest.h"
#include "dbUnitTest.h"
#include "registry.h"
#include "dbmf.h"
#include "epicsTime.h"
#include "testMain.h"
#include "osiFileName.h"

#define PATTERN 0x55

void filterTest_registerRecordDeviceDriver(struct dbBase *);

static db_field_log fl;

static int fl_equal(const db_field_log *pfl1, const db_field_log *pfl2) {
    return !(memcmp(pfl1, pfl2, sizeof(db_field_log)));
}

static int fl_equal_ex_ts(const db_field_log *pfl1, const db_field_log *pfl2) {
    db_field_log fl1 = *pfl1;

    fl1.time = pfl2->time;
    return fl_equal(&fl1, pfl2);
}

MAIN(tsTest)
{
    dbChannel *pch;
    chFilter *filter;
    const chFilterPlugin *plug;
    char ts[] = "ts";
    ELLNODE *node;
    chPostEventFunc *cb_out = NULL;
    void *arg_out = NULL;
    db_field_log fl1;
    db_field_log *pfl2;
    epicsTimeStamp stamp, now;
    dbEventCtx evtctx;

    testPlan(12);

    testdbPrepare();

    testdbReadDatabase("filterTest.dbd", NULL, NULL);

    filterTest_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("xRecord.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    evtctx = db_init_events();

    testOk(!!(plug = dbFindFilter(ts, strlen(ts))), "plugin ts registered correctly");

    testOk(!!(pch = dbChannelCreate("x.VAL{\"ts\":{}}")), "dbChannel with plugin ts created");
    testOk((ellCount(&pch->filters) == 1), "channel has one plugin");

    memset(&fl, PATTERN, sizeof(fl));
    fl1 = fl;
    node = ellFirst(&pch->filters);
    filter = CONTAINER(node, chFilter, list_node);
    plug->fif->channel_register_pre(filter, &cb_out, &arg_out, &fl1);
    testOk(!!(cb_out) && !(arg_out), "register_pre registers one filter w/o argument");
    testOk(fl_equal(&fl1, &fl), "register_pre does not change field_log data type");

    testOk(!(dbChannelOpen(pch)), "dbChannel with plugin ts opened");
    node = ellFirst(&pch->pre_chain);
    filter = CONTAINER(node, chFilter, pre_node);
    testOk((ellCount(&pch->pre_chain) == 1 && filter->pre_arg == NULL),
           "ts has one filter w/o argument in pre chain");
    testOk((ellCount(&pch->post_chain) == 0), "ts has no filter in post chain");

    memset(&fl, PATTERN, sizeof(fl));
    fl1 = fl;
    pfl2 = dbChannelRunPreChain(pch, &fl1);
    testOk(pfl2 == &fl1, "ts filter does not drop or replace field_log");
    testOk(fl_equal_ex_ts(&fl1, pfl2), "ts filter does not change field_log data");

    testOk(!!(pfl2 = db_create_read_log(pch)), "create field log from channel");
    stamp = pfl2->time;
    db_delete_field_log(pfl2);

    pfl2 = dbChannelRunPreChain(pch, &fl1);
    epicsTimeGetCurrent(&now);
    testOk(epicsTimeDiffInSeconds(&pfl2->time, &stamp) >= 0 &&
        epicsTimeDiffInSeconds(&now, &pfl2->time) >= 0,
        "ts filter sets time stamp to \"now\"");

    dbChannelDelete(pch);

    db_close_events(evtctx);

    testIocShutdownOk();

    testdbCleanup();

    return testDone();
}
