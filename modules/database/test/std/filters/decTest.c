/*************************************************************************\
* Copyright (c) 2019 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Authors: Ralph Lange <Ralph.Lange@bessy.de>,
 *           Andrew Johnson <anj@anl.gov>
 */

#include <string.h>

#include "dbStaticLib.h"
#include "dbAccessDefs.h"
#include "db_field_log.h"
#include "dbCommon.h"
#include "dbChannel.h"
#include "registry.h"
#include "chfPlugin.h"
#include "errlog.h"
#include "dbmf.h"
#include "epicsUnitTest.h"
#include "dbUnitTest.h"
#include "epicsTime.h"
#include "testMain.h"
#include "osiFileName.h"

void filterTest_registerRecordDeviceDriver(struct dbBase *);

static int fl_equal(const db_field_log *pfl1, const db_field_log *pfl2) {
    return !(memcmp(pfl1, pfl2, sizeof(db_field_log)));
}

static void fl_setup(dbChannel *chan, db_field_log *pfl, long val) {
    struct dbCommon  *prec = dbChannelRecord(chan);

    memset(pfl, 0, sizeof(db_field_log));
    pfl->ctx  = dbfl_context_event;
    pfl->type = dbfl_type_val;
    pfl->stat = prec->stat;
    pfl->sevr = prec->sevr;
    pfl->time = prec->time;
    pfl->field_type  = DBF_LONG;
    pfl->field_size  = sizeof(epicsInt32);
    pfl->no_elements = 1;
    /*
     * use memcpy to avoid a bus error on
     * union copy of char in the db at an odd
     * address
     */
    memcpy(&pfl->u.v.field,
           dbChannelField(chan),
           dbChannelFieldSize(chan));
    pfl->u.v.field.dbf_long = val;
}

static void testHead (char* title) {
    testDiag("--------------------------------------------------------");
    testDiag("%s", title);
    testDiag("--------------------------------------------------------");
}

static void mustDrop(dbChannel *pch, db_field_log *pfl, char* m) {
    int oldFree = db_available_logs();
    db_field_log *pfl2 = dbChannelRunPreChain(pch, pfl);
    int newFree = db_available_logs();

    testOk(NULL == pfl2, "filter drops field_log (%s)", m);
    testOk(newFree == oldFree + 1, "field_log was freed - %d+1 => %d",
        oldFree, newFree);

    db_delete_field_log(pfl2);
}

static void mustPass(dbChannel *pch, db_field_log *pfl, char* m) {
    int oldFree = db_available_logs();
    db_field_log *pfl2 = dbChannelRunPreChain(pch, pfl);
    int newFree = db_available_logs();

    testOk(pfl == pfl2, "filter passes field_log (%s)", m);
    testOk(newFree == oldFree, "field_log was not freed - %d => %d",
        oldFree, newFree);

    db_delete_field_log(pfl2);
}

static void checkAndOpenChannel(dbChannel *pch, const chFilterPlugin *plug) {
    ELLNODE *node;
    chFilter *filter;
    chPostEventFunc *cb_out = NULL;
    void *arg_out = NULL;
    db_field_log fl, fl1;

    testDiag("Test filter structure and open channel");

    testOk((ellCount(&pch->filters) == 1), "channel has one plugin");

    fl_setup(pch, &fl, 1);
    fl1 = fl;
    node = ellFirst(&pch->filters);
    filter = CONTAINER(node, chFilter, list_node);
    plug->fif->channel_register_pre(filter, &cb_out, &arg_out, &fl1);
    testOk(cb_out && arg_out,
        "register_pre registers one filter with argument");
    testOk(fl_equal(&fl1, &fl),
        "register_pre does not change field_log data type");

    testOk(!(dbChannelOpen(pch)), "dbChannel with plugin dec opened");
    node = ellFirst(&pch->pre_chain);
    filter = CONTAINER(node, chFilter, pre_node);
    testOk((ellCount(&pch->pre_chain) == 1 && filter->pre_arg != NULL),
        "dec has one filter with argument in pre chain");
    testOk((ellCount(&pch->post_chain) == 0),
        "sync has no filter in post chain");
}

MAIN(decTest)
{
    dbChannel *pch;
    const chFilterPlugin *plug;
    char myname[] = "dec";
    db_field_log *pfl[10];
    int i, logsFree, logsFinal;
    dbEventCtx evtctx;

    testPlan(104);

    testdbPrepare();

    testdbReadDatabase("filterTest.dbd", NULL, NULL);

    filterTest_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("xRecord.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    evtctx = db_init_events();

    plug = dbFindFilter(myname, strlen(myname));
    if (!plug)
        testAbort("Plugin '%s' not registered", myname);
    testPass("plugin '%s' registered correctly", myname);

    /* N < 1 */
    testOk(!(pch = dbChannelCreate("x.VAL{dec:{n:-1}}")),
           "dbChannel with dec (n=-1) failed");
    testOk(!(pch = dbChannelCreate("x.VAL{dec:{n:0}}")),
           "dbChannel with dec (n=0) failed");
    /* Bad parms */
    testOk(!(pch = dbChannelCreate("x.VAL{dec:{}}")),
           "dbChannel with dec (no parm) failed");
    testOk(!(pch = dbChannelCreate("x.VAL{dec:{x:true}}")),
           "dbChannel with dec (x=true) failed");

    /* No Decimation (N=1) */

    testHead("No Decimation (n=1)");
    testOk(!!(pch = dbChannelCreate("x.VAL{dec:{n:1}}")),
           "dbChannel with plugin dec (n=1) created");

    /* Start the free-list */
    db_delete_field_log(db_create_read_log(pch));
    logsFree = db_available_logs();
    testDiag("%d field_logs on free-list", logsFree);

    checkAndOpenChannel(pch, plug);

    for (i = 0; i < 5; i++) {
        pfl[i] = db_create_read_log(pch);
        fl_setup(pch, pfl[i], 10 + i);
    }

    testDiag("Test event stream");

    mustPass(pch, pfl[0], "i=0");
    mustPass(pch, pfl[1], "i=1");
    mustPass(pch, pfl[2], "i=2");
    mustPass(pch, pfl[3], "i=3");
    mustPass(pch, pfl[4], "i=4");

    dbChannelDelete(pch);

    testDiag("%d field_logs on free-list", db_available_logs());

    /* Decimation (N=2) */

    testHead("Decimation (n=2)");
    testOk(!!(pch = dbChannelCreate("x.VAL{dec:{n:2}}")),
           "dbChannel with plugin dec (n=2) created");

    checkAndOpenChannel(pch, plug);

    for (i = 0; i < 10; i++) {
        pfl[i] = db_create_read_log(pch);
        fl_setup(pch, pfl[i], 20 + i);
    }

    testDiag("Test event stream");

    mustPass(pch, pfl[0], "i=0");
    mustDrop(pch, pfl[1], "i=1");
    mustPass(pch, pfl[2], "i=2");
    mustDrop(pch, pfl[3], "i=3");
    mustPass(pch, pfl[4], "i=4");
    mustDrop(pch, pfl[5], "i=5");
    mustPass(pch, pfl[6], "i=6");
    mustDrop(pch, pfl[7], "i=7");
    mustPass(pch, pfl[8], "i=8");
    mustDrop(pch, pfl[9], "i=9");

    dbChannelDelete(pch);

    testDiag("%d field_logs on free-list", db_available_logs());

    /* Decimation (N=3) */

    testHead("Decimation (n=3)");
    testOk(!!(pch = dbChannelCreate("x.VAL{dec:{n:3}}")),
           "dbChannel with plugin dec (n=3) created");

    checkAndOpenChannel(pch, plug);

    for (i = 0; i < 10; i++) {
        pfl[i] = db_create_read_log(pch);
        fl_setup(pch, pfl[i], 30 + i);
    }

    testDiag("Test event stream");

    mustPass(pch, pfl[0], "i=0");
    mustDrop(pch, pfl[1], "i=1");
    mustDrop(pch, pfl[2], "i=2");
    mustPass(pch, pfl[3], "i=3");
    mustDrop(pch, pfl[4], "i=4");
    mustDrop(pch, pfl[5], "i=5");
    mustPass(pch, pfl[6], "i=6");
    mustDrop(pch, pfl[7], "i=7");
    mustDrop(pch, pfl[8], "i=8");
    mustPass(pch, pfl[9], "i=9");

    dbChannelDelete(pch);

    testDiag("%d field_logs on free-list", db_available_logs());

    /* Decimation (N=4) */

    testHead("Decimation (n=4)");
    testOk(!!(pch = dbChannelCreate("x.VAL{dec:{n:4}}")),
           "dbChannel with plugin dec (n=4) created");

    checkAndOpenChannel(pch, plug);

    for (i = 0; i < 10; i++) {
        pfl[i] = db_create_read_log(pch);
        fl_setup(pch, pfl[i], 40 + i);
    }

    testDiag("Test event stream");

    mustPass(pch, pfl[0], "i=0");
    mustDrop(pch, pfl[1], "i=1");
    mustDrop(pch, pfl[2], "i=2");
    mustDrop(pch, pfl[3], "i=3");
    mustPass(pch, pfl[4], "i=4");
    mustDrop(pch, pfl[5], "i=5");
    mustDrop(pch, pfl[6], "i=6");
    mustDrop(pch, pfl[7], "i=7");
    mustPass(pch, pfl[8], "i=8");
    mustDrop(pch, pfl[9], "i=9");

    dbChannelDelete(pch);

    logsFinal = db_available_logs();
    testOk(logsFree == logsFinal, "%d field_logs on free-list", logsFinal);

    db_close_events(evtctx);

    testIocShutdownOk();

    testdbCleanup();

    return testDone();
}
