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
#include "dbChannel.h"
#include "registry.h"
#include "chfPlugin.h"
#include "errlog.h"
#include "dbmf.h"
#include "epicsUnitTest.h"
#include "dbUnitTest.h"
#include "epicsTime.h"
#include "dbState.h"
#include "testMain.h"
#include "osiFileName.h"

#define PATTERN 0x55

void filterTest_registerRecordDeviceDriver(struct dbBase *);

static db_field_log fl;
static dbStateId red;

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

/*
 * Use mustDrop() and mustPass() to test filters with no memory
 * of previous field_log pointers.
 */
static void mustDrop(dbChannel *pch, db_field_log *pfl, char* m) {
    int oldFree = db_available_logs();
    db_field_log *pfl2 = dbChannelRunPreChain(pch, pfl);
    int newFree = db_available_logs();

    testOk(NULL == pfl2, "filter drops field_log (%s)", m);
    testOk(newFree == oldFree + 1, "a field_log was freed - %d+1 => %d",
        oldFree, newFree);

    db_delete_field_log(pfl2);
}

static void mustPass(dbChannel *pch, db_field_log *pfl, char* m) {
    int oldFree = db_available_logs();
    db_field_log *pfl2 = dbChannelRunPreChain(pch, pfl);
    int newFree = db_available_logs();

    testOk(pfl == pfl2, "filter passes field_log (%s)", m);
    testOk(newFree == oldFree, "no field_logs were freed - %d => %d",
        oldFree, newFree);

    db_delete_field_log(pfl2);
}

/*
 * Use mustStash() and mustSwap() to test filters that save
 * field_log pointers and return them later.
 *
 * mustStash() expects the filter to save the current pointer
 *      (freeing any previously saved pointer) and return NULL.
 * mustSwap() expects the filter to return the previously
 *      saved pointer and save the current pointer.
 */
static db_field_log *stashed;

static void streamReset(void) {
    stashed = NULL;
}

static void mustStash(dbChannel *pch, db_field_log *pfl, char* m) {
    int oldFree = db_available_logs();
    db_field_log *pfl2 = dbChannelRunPreChain(pch, pfl);
    int newFree = db_available_logs();

    testOk(NULL == pfl2, "filter stashes field_log (%s)", m);
    if (stashed) {
        testOk(newFree == oldFree + 1, "a field_log was freed - %d+1 => %d",
            oldFree, newFree);
    }
    else {
        testOk(newFree == oldFree, "no field_logs were freed - %d => %d",
            oldFree, newFree);
    }
    stashed = pfl;
    db_delete_field_log(pfl2);
}

static void mustSwap(dbChannel *pch, db_field_log *pfl, char* m) {
    int oldFree = db_available_logs();
    db_field_log *pfl2 = dbChannelRunPreChain(pch, pfl);
    int newFree = db_available_logs();

    testOk(stashed == pfl2, "filter returns stashed field log (%s)", m);
    testOk(newFree == oldFree, "no field_logs were freed - %d => %d",
        oldFree, newFree);

    stashed = pfl;
    db_delete_field_log(pfl2);
}

static void mustPassTwice(dbChannel *pch, db_field_log *pfl, char* m) {
    int oldFree = db_available_logs(), newFree;
    db_field_log *pfl2;

    testDiag("%s: filter must pass twice", m);
    pfl2 = dbChannelRunPreChain(pch, pfl);
    testOk(pfl2 == pfl, "call 1 does not drop or replace field_log");
    pfl2 = dbChannelRunPreChain(pch, pfl);
    testOk(pfl2 == pfl, "call 2 does not drop or replace field_log");
    newFree = db_available_logs();
    testOk(newFree == oldFree, "no field_logs were freed - %d => %d",
        oldFree, newFree);
}

static void checkCtxRead(dbChannel *pch, dbStateId id) {
    fl.ctx = dbfl_context_read;
    dbStateClear(id);
    mustPassTwice(pch, &fl, "ctx='read', state=FALSE");
    dbStateSet(id);
    mustPassTwice(pch, &fl, "ctx='read', state=TRUE");
    dbStateClear(id);
    mustPassTwice(pch, &fl, "ctx='read', state=FALSE");
    fl.ctx = dbfl_context_event;
}

static void checkAndOpenChannel(dbChannel *pch, const chFilterPlugin *plug) {
    ELLNODE *node;
    chFilter *filter;
    chPostEventFunc *cb_out = NULL;
    void *arg_out = NULL;
    db_field_log fl1;

    testDiag("Test filter structure and open channel");

    testOk((ellCount(&pch->filters) == 1), "channel has one plugin");

    fl1 = fl;
    node = ellFirst(&pch->filters);
    filter = CONTAINER(node, chFilter, list_node);
    plug->fif->channel_register_pre(filter, &cb_out, &arg_out, &fl1);
    testOk(!!(cb_out) && !!(arg_out), "register_pre registers one filter with argument");
    testOk(fl_equal(&fl1, &fl), "register_pre does not change field_log data type");

    testOk(!(dbChannelOpen(pch)), "dbChannel with plugin sync opened");
    node = ellFirst(&pch->pre_chain);
    filter = CONTAINER(node, chFilter, pre_node);
    testOk((ellCount(&pch->pre_chain) == 1 && filter->pre_arg != NULL),
           "sync has one filter with argument in pre chain");
    testOk((ellCount(&pch->post_chain) == 0), "sync has no filter in post chain");

    checkCtxRead(pch, red);
}

MAIN(syncTest)
{
    dbChannel *pch;
    const chFilterPlugin *plug;
    char myname[] = "sync";
    db_field_log *pfl[10];
    int i, logsFree, logsFinal;
    dbEventCtx evtctx;

    testPlan(214);

    testdbPrepare();

    testdbReadDatabase("filterTest.dbd", NULL, NULL);

    filterTest_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("xRecord.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    evtctx = db_init_events();

    testOk(!!(plug = dbFindFilter(myname, strlen(myname))), "plugin %s registered correctly", myname);
    testOk(!!(red = dbStateCreate("red")), "state 'red' created successfully");

    /* nonexisting state */
    testOk(!(pch = dbChannelCreate("x.VAL{\"sync\":{\"m\":\"while\",\"s\":\"blue\"}}")),
           "dbChannel with sync (m='while' s='blue') (nonex state) failed");
    /* missing state */
    testOk(!(pch = dbChannelCreate("x.VAL{\"sync\":{\"m\":\"while\"}}")),
           "dbChannel with sync (m='while') (no state) failed");
    /* missing mode */
    testOk(!(pch = dbChannelCreate("x.VAL{\"sync\":{\"s\":\"red\"}}")),
           "dbChannel with sync (s='red') (no mode) failed");

    /* mode WHILE */

    testHead("Mode WHILE  (m='while', s='red')");
    testOk(!!(pch = dbChannelCreate("x.VAL{\"sync\":{\"m\":\"while\",\"s\":\"red\"}}")),
           "dbChannel with plugin sync (m='while' s='red') created");

    /* Start the free-list */
    db_delete_field_log(db_create_read_log(pch));
    logsFree = db_available_logs();
    testDiag("%d field_logs on free-list", logsFree);

    checkAndOpenChannel(pch, plug);

    for (i = 0; i < 9; i++) {
        pfl[i] = db_create_read_log(pch);
        fl_setup(pch, pfl[i], 120 + i);
    }

    testDiag("Test event stream");

    dbStateClear(red);
    mustDrop(pch, pfl[0], "state=FALSE, log0");
    mustDrop(pch, pfl[1], "state=FALSE, log1");
    mustDrop(pch, pfl[2], "state=FALSE, log2");
    dbStateSet(red);
    mustPass(pch, pfl[3], "state=TRUE, log3");
    mustPass(pch, pfl[4], "state=TRUE, log4");
    mustPass(pch, pfl[5], "state=TRUE, log5");
    dbStateClear(red);
    mustDrop(pch, pfl[6], "state=FALSE, log6");
    mustDrop(pch, pfl[7], "state=FALSE, log7");
    mustDrop(pch, pfl[8], "state=FALSE, log8");

    dbChannelDelete(pch);

    testDiag("%d field_logs on free-list", db_available_logs());

    /* mode UNLESS */

    testHead("Mode UNLESS  (m='unless', s='red')");
    testOk(!!(pch = dbChannelCreate("x.VAL{\"sync\":{\"m\":\"unless\",\"s\":\"red\"}}")),
           "dbChannel with plugin sync (m='unless' s='red') created");

    checkAndOpenChannel(pch, plug);

    for (i = 0; i < 9; i++) {
        pfl[i] = db_create_read_log(pch);
        fl_setup(pch, pfl[i], 120 + i);
    }

    testDiag("Test event stream");

    dbStateClear(red);
    mustPass(pch, pfl[0], "state=FALSE, log0");
    mustPass(pch, pfl[1], "state=FALSE, log1");
    mustPass(pch, pfl[2], "state=FALSE, log2");
    dbStateSet(red);
    mustDrop(pch, pfl[3], "state=TRUE, log3");
    mustDrop(pch, pfl[4], "state=TRUE, log4");
    mustDrop(pch, pfl[5], "state=TRUE, log5");
    dbStateClear(red);
    mustPass(pch, pfl[6], "state=FALSE, log6");
    mustPass(pch, pfl[7], "state=FALSE, log7");
    mustPass(pch, pfl[8], "state=FALSE, log8");

    dbChannelDelete(pch);

    testDiag("%d field_logs on free-list", db_available_logs());

    /* mode BEFORE */

    testHead("Mode BEFORE  (m='before', s='red')");
    testOk(!!(pch = dbChannelCreate("x.VAL{\"sync\":{\"m\":\"before\",\"s\":\"red\"}}")),
           "dbChannel with plugin sync (m='before' s='red') created");

    checkAndOpenChannel(pch, plug);

    for (i = 0; i < 10; i++) {
        pfl[i] = db_create_read_log(pch);
        fl_setup(pch, pfl[i], 120 + i);
    }

    testDiag("Test event stream");

    streamReset();
    dbStateClear(red);
    mustStash(pch, pfl[0], "state=FALSE, log0");
    mustStash(pch, pfl[1], "state=FALSE, log1");
    mustStash(pch, pfl[2], "state=FALSE, log2");
    dbStateSet(red);
    mustSwap(pch, pfl[3], "state=TRUE, log3");
    mustStash(pch, pfl[4], "state=TRUE, log4");
    mustStash(pch, pfl[5], "state=TRUE, log5");
    mustStash(pch, pfl[6], "state=TRUE, log6");
    dbStateClear(red);
    mustStash(pch, pfl[7], "state=FALSE, log7");
    mustStash(pch, pfl[8], "state=FALSE, log8");
    mustStash(pch, pfl[9], "state=FALSE, log9");

    dbChannelDelete(pch);

    testDiag("%d field_logs on free-list", db_available_logs());

    /* mode FIRST */

    testHead("Mode FIRST  (m='first', s='red')");
    testOk(!!(pch = dbChannelCreate("x.VAL{\"sync\":{\"m\":\"first\",\"s\":\"red\"}}")),
           "dbChannel with plugin sync (m='first' s='red') created");

    checkAndOpenChannel(pch, plug);

    for (i = 0; i < 9; i++) {
        pfl[i] = db_create_read_log(pch);
        fl_setup(pch, pfl[i], 120 + i);
    }

    testDiag("Test event stream");

    streamReset();
    dbStateClear(red);
    mustDrop(pch, pfl[0], "state=FALSE, log0");
    mustDrop(pch, pfl[1], "state=FALSE, log1");
    mustDrop(pch, pfl[2], "state=FALSE, log2");
    dbStateSet(red);
    mustPass(pch, pfl[3], "state=TRUE, log3");
    mustDrop(pch, pfl[4], "state=TRUE, log4");
    mustDrop(pch, pfl[5], "state=TRUE, log5");
    dbStateClear(red);
    mustDrop(pch, pfl[6], "state=FALSE, log6");
    mustDrop(pch, pfl[7], "state=FALSE, log7");
    mustDrop(pch, pfl[8], "state=FALSE, log8");

    dbChannelDelete(pch);

    testDiag("%d field_logs on free-list", db_available_logs());

    /* mode LAST */

    testHead("Mode LAST  (m='last', s='red')");
    testOk(!!(pch = dbChannelCreate("x.VAL{\"sync\":{\"m\":\"last\",\"s\":\"red\"}}")),
           "dbChannel with plugin sync (m='last' s='red') created");

    checkAndOpenChannel(pch, plug);

    for (i = 0; i < 10; i++) {
        pfl[i] = db_create_read_log(pch);
        fl_setup(pch, pfl[i], 120 + i);
    }

    testDiag("Test event stream");

    streamReset();
    dbStateClear(red);
    mustStash(pch, pfl[0], "state=FALSE, log0");
    mustStash(pch, pfl[1], "state=FALSE, log1");
    mustStash(pch, pfl[2], "state=FALSE, log2");
    dbStateSet(red);
    mustStash(pch, pfl[3], "state=TRUE, log3");
    mustStash(pch, pfl[4], "state=TRUE, log4");
    mustStash(pch, pfl[5], "state=TRUE, log5");
    dbStateClear(red);
    mustSwap(pch, pfl[6], "state=TRUE, log6");
    mustStash(pch, pfl[7], "state=FALSE, log7");
    mustStash(pch, pfl[8], "state=FALSE, log8");
    mustStash(pch, pfl[9], "state=FALSE, log9");

    dbChannelDelete(pch);

    testDiag("%d field_logs on free-list", db_available_logs());

    /* mode AFTER */

    testHead("Mode AFTER  (m='after', s='red')");
    testOk(!!(pch = dbChannelCreate("x.VAL{\"sync\":{\"m\":\"after\",\"s\":\"red\"}}")),
           "dbChannel with plugin sync (m='after' s='red') created");

    checkAndOpenChannel(pch, plug);

    for (i = 0; i < 9; i++) {
        pfl[i] = db_create_read_log(pch);
        fl_setup(pch, pfl[i], 120 + i);
    }

    testDiag("Test event stream");

    streamReset();
    dbStateClear(red);
    mustDrop(pch, pfl[0], "state=FALSE, log0");
    mustDrop(pch, pfl[1], "state=FALSE, log1");
    mustDrop(pch, pfl[2], "state=FALSE, log2");
    dbStateSet(red);
    mustDrop(pch, pfl[3], "state=TRUE, log3");
    mustDrop(pch, pfl[4], "state=TRUE, log4");
    mustDrop(pch, pfl[5], "state=TRUE, log5");
    dbStateClear(red);
    mustPass(pch, pfl[6], "state=FALSE, log6");
    mustDrop(pch, pfl[7], "state=FALSE, log7");
    mustDrop(pch, pfl[8], "state=FALSE, log8");

    dbChannelDelete(pch);

    logsFinal = db_available_logs();
    testOk(logsFree == logsFinal, "%d field_logs on free-list", logsFinal);

    db_close_events(evtctx);

    testIocShutdownOk();

    testdbCleanup();

    return testDone();
}
