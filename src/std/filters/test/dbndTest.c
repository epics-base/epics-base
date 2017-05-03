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
#include "db_field_log.h"
#include "dbCommon.h"
#include "registry.h"
#include "errlog.h"
#include "chfPlugin.h"
#include "epicsUnitTest.h"
#include "dbUnitTest.h"
#include "epicsTime.h"
#include "dbmf.h"
#include "testMain.h"
#include "osiFileName.h"

#define PATTERN 0x55

void filterTest_registerRecordDeviceDriver(struct dbBase *);

static db_field_log fl;

static int fl_equal(const db_field_log *pfl1, const db_field_log *pfl2) {
    return !(memcmp(pfl1, pfl2, sizeof(db_field_log)));
}

static void fl_setup(dbChannel *chan, db_field_log *pfl) {
    struct dbCommon  *prec = dbChannelRecord(chan);

    pfl->ctx  = dbfl_context_read;
    pfl->type = dbfl_type_val;
    pfl->stat = prec->stat;
    pfl->sevr = prec->sevr;
    pfl->time = prec->time;
    pfl->field_type  = dbChannelFieldType(chan);
    pfl->no_elements = dbChannelElements(chan);
    /*
     * use memcpy to avoid a bus error on
     * union copy of char in the db at an odd
     * address
     */
    memcpy(&pfl->u.v.field,
           dbChannelField(chan),
           dbChannelFieldSize(chan));
}

static void changeValue(db_field_log *pfl2, long val) {
    pfl2->u.v.field.dbf_long = val;
    testDiag("new value: %ld", val);
}

static void mustPassOnce(dbChannel *pch, db_field_log *pfl2, char* m, double d, long val) {
    db_field_log *pfl;

    changeValue(pfl2, val);
    testDiag("mode=%s delta=%g filter must pass once", m, d);
    pfl = dbChannelRunPreChain(pch, pfl2);
    testOk(pfl2 == pfl, "call 1 does not drop or replace field_log");
    testOk(fl_equal(pfl, pfl2), "call 1 does not change field_log data");
    pfl = dbChannelRunPreChain(pch, pfl2);
    testOk(NULL == pfl, "call 2 drops field_log");
}

static void mustDrop(dbChannel *pch, db_field_log *pfl2, char* m, double d, long val) {
    db_field_log *pfl;

    changeValue(pfl2, val);
    testDiag("mode=%s delta=%g filter must drop", m, d);
    pfl = dbChannelRunPreChain(pch, pfl2);
    testOk(NULL == pfl, "call 1 drops field_log");
}

static void mustPassTwice(dbChannel *pch, db_field_log *pfl2, char* m, double d, long val) {
    db_field_log *pfl;

    changeValue(pfl2, val);
    testDiag("mode=%s delta=%g filter must pass twice", m, d);
    pfl = dbChannelRunPreChain(pch, pfl2);
    testOk(pfl2 == pfl, "call 1 does not drop or replace field_log");
    testOk(fl_equal(pfl, pfl2), "call 1 does not change field_log data");
    pfl = dbChannelRunPreChain(pch, pfl2);
    testOk(pfl2 == pfl, "call 2 does not drop or replace field_log");
    testOk(fl_equal(pfl, pfl2), "call 2 does not change field_log data");
}

static void testHead (char* title) {
    testDiag("--------------------------------------------------------");
    testDiag("%s", title);
    testDiag("--------------------------------------------------------");
}

MAIN(dbndTest)
{
    dbChannel *pch;
    chFilter *filter;
    const chFilterPlugin *plug;
    char dbnd[] = "dbnd";
    ELLNODE *node;
    chPostEventFunc *cb_out = NULL;
    void *arg_out = NULL;
    db_field_log *pfl2;
    db_field_log fl1;
    dbEventCtx evtctx;

    testPlan(59);

    testdbPrepare();

    testdbReadDatabase("filterTest.dbd", NULL, NULL);

    filterTest_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("xRecord.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    evtctx = db_init_events();

    testOk(!!(plug = dbFindFilter(dbnd, strlen(dbnd))), "plugin dbnd registered correctly");

    testOk(!!(pch = dbChannelCreate("x.VAL{\"dbnd\":{}}")), "dbChannel with plugin dbnd (delta=0) created");
    testOk((ellCount(&pch->filters) == 1), "channel has one plugin");

    memset(&fl, PATTERN, sizeof(fl));
    fl1 = fl;
    node = ellFirst(&pch->filters);
    filter = CONTAINER(node, chFilter, list_node);
    plug->fif->channel_register_pre(filter, &cb_out, &arg_out, &fl1);
    testOk(!!(cb_out) && !!(arg_out), "register_pre registers one filter with argument");
    testOk(fl_equal(&fl1, &fl), "register_pre does not change field_log data type");

    testOk(!(dbChannelOpen(pch)), "dbChannel with plugin dbnd opened");
    node = ellFirst(&pch->pre_chain);
    filter = CONTAINER(node, chFilter, pre_node);
    testOk((ellCount(&pch->pre_chain) == 1 && filter->pre_arg != NULL),
           "dbnd has one filter with argument in pre chain");
    testOk((ellCount(&pch->post_chain) == 0), "dbnd has no filter in post chain");

    /* Field logs of type ref and rec: pass any update */

    testHead("Field logs of type ref and rec");
    fl1.type = dbfl_type_rec;
    mustPassTwice(pch, &fl1, "abs field_log=rec", 0., 0);

    fl1.type = dbfl_type_ref;
    mustPassTwice(pch, &fl1, "abs field_log=ref", 0., 0);

    /* Delta = 0: pass any change */

    testHead("Delta = 0: pass any change");
    pfl2 = db_create_read_log(pch);
    testDiag("new field_log from record");
    fl_setup(pch, pfl2);

    mustPassOnce(pch, pfl2, "abs", 0., 0);

    pfl2 = db_create_read_log(pch);
    testDiag("new field_log from record");
    fl_setup(pch, pfl2);

    mustPassOnce(pch, pfl2, "abs", 0., 1);

    dbChannelDelete(pch);

    /* Delta = -1: pass any update */

    testHead("Delta = -1: pass any update");
    testOk(!!(pch = dbChannelCreate("x.VAL{\"dbnd\":{\"d\":-1.0}}")), "dbChannel with plugin dbnd (delta=-1) created");
    testOk(!(dbChannelOpen(pch)), "dbChannel with plugin dbnd opened");

    pfl2 = db_create_read_log(pch);
    testDiag("new field_log from record");
    fl_setup(pch, pfl2);

    mustPassTwice(pch, pfl2, "abs", -1., 0);
    mustPassTwice(pch, pfl2, "abs", -1., 1);

    db_delete_field_log(pfl2);
    dbChannelDelete(pch);

    /* Delta = absolute */

    testHead("Delta = absolute");
    testOk(!!(pch = dbChannelCreate("x.VAL{\"dbnd\":{\"d\":3}}")), "dbChannel with plugin dbnd (delta=3) created");
    testOk(!(dbChannelOpen(pch)), "dbChannel with plugin dbnd opened");

    pfl2 = db_create_read_log(pch);
    testDiag("new field_log from record");
    fl_setup(pch, pfl2);

    mustPassOnce(pch, pfl2, "abs", 3., 1);

    pfl2 = db_create_read_log(pch);
    testDiag("new field_log from record");
    fl_setup(pch, pfl2);

    mustDrop(pch, pfl2, "abs", 3., 3);

    pfl2 = db_create_read_log(pch);
    testDiag("new field_log from record");
    fl_setup(pch, pfl2);

    mustDrop(pch, pfl2, "abs", 3., 4);

    pfl2 = db_create_read_log(pch);
    testDiag("new field_log from record");
    fl_setup(pch, pfl2);

    mustPassOnce(pch, pfl2, "abs", 3., 5);

    dbChannelDelete(pch);

    /* Delta = relative */

    testHead("Delta = relative");
    testOk(!!(pch = dbChannelCreate("x.VAL{\"dbnd\":{\"m\":\"rel\",\"d\":50}}")),
           "dbChannel with plugin dbnd (mode=rel, delta=50) created");
    testOk(!(dbChannelOpen(pch)), "dbChannel with plugin dbnd opened");

    pfl2 = db_create_read_log(pch);
    testDiag("new field_log from record");
    fl_setup(pch, pfl2);

    mustPassOnce(pch, pfl2, "rel", 50., 1);

    pfl2 = db_create_read_log(pch);
    testDiag("new field_log from record");
    fl_setup(pch, pfl2);

    mustPassOnce(pch, pfl2, "rel", 50., 2);

    pfl2 = db_create_read_log(pch);
    testDiag("new field_log from record");
    fl_setup(pch, pfl2);

    mustDrop(pch, pfl2, "rel", 50., 3);

    pfl2 = db_create_read_log(pch);
    testDiag("new field_log from record");
    fl_setup(pch, pfl2);

    mustPassOnce(pch, pfl2, "rel", 50., 4);

    pfl2 = db_create_read_log(pch);
    testDiag("new field_log from record");
    fl_setup(pch, pfl2);

    mustDrop(pch, pfl2, "rel", 50., 5);

    pfl2 = db_create_read_log(pch);
    testDiag("new field_log from record");
    fl_setup(pch, pfl2);

    mustDrop(pch, pfl2, "rel", 50., 6);

    pfl2 = db_create_read_log(pch);
    testDiag("new field_log from record");
    fl_setup(pch, pfl2);

    mustPassOnce(pch, pfl2, "rel", 50., 7);

    dbChannelDelete(pch);

    db_close_events(evtctx);

    testIocShutdownOk();

    testdbCleanup();

    return testDone();
}
