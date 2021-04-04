/*************************************************************************\
* Copyright (c) 2021 Cosylab d.d
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <Ralph.Lange@bessy.de>
 *  Author: Jure Varlec <jure.varlec@cosylab.com>
 */

#include <string.h>
#include <math.h>

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

/* A fill pattern for setting a field log to something "random". */
#define PATTERN 0x55

/* Use a "normal" timestamp for testing. What results from filling the field log
   with the above pattern is a timestamp that causes problems with
   epicsTimeToStrftime() on some platforms. */
static epicsTimeStamp const test_ts = { 616600420, 998425354 };

typedef int (*type_check)(const db_field_log *pfl);
typedef int (*value_check)(const db_field_log *pfl, const epicsTimeStamp *ts);

void filterTest_registerRecordDeviceDriver(struct dbBase *);

static int fl_equal(const db_field_log *pfl1, const db_field_log *pfl2) {
    return !(memcmp(pfl1, pfl2, sizeof(db_field_log)));
}

static int fl_equal_ex_ts(const db_field_log *pfl1, const db_field_log *pfl2) {
    db_field_log fl1 = *pfl1;

    fl1.time = pfl2->time;
    return fl_equal(&fl1, pfl2);
}

static void fl_reset(db_field_log *pfl) {
    memset(pfl, PATTERN, sizeof(*pfl));
    pfl->time = test_ts;
}

static void test_generate_filter(const chFilterPlugin *plug) {
    dbChannel *pch;
    chFilter *filter;
    db_field_log fl;
    db_field_log fl1;
    db_field_log *pfl2;
    epicsTimeStamp stamp, now;
    ELLNODE *node;
    chPostEventFunc *cb_out = NULL;
    void *arg_out = NULL;
    char const *chan_name = "x.VAL{ts:{}}";

    testDiag("Channel %s", chan_name);

    testOk(!!(pch = dbChannelCreate(chan_name)),
           "dbChannel with plugin ts created");
    testOk((ellCount(&pch->filters) == 1), "channel has one plugin");

    fl_reset(&fl);
    fl1 = fl;
    node = ellFirst(&pch->filters);
    filter = CONTAINER(node, chFilter, list_node);
    plug->fif->channel_register_post(filter, &cb_out, &arg_out, &fl1);
    plug->fif->channel_register_pre(filter, &cb_out, &arg_out, &fl1);
    testOk(!!(cb_out) && !(arg_out),
           "register_pre registers one filter w/o argument");
    testOk(fl_equal(&fl1, &fl),
           "register_pre does not change field_log data type");

    testOk(!(dbChannelOpen(pch)), "dbChannel with plugin ts opened");
    node = ellFirst(&pch->pre_chain);
    filter = CONTAINER(node, chFilter, pre_node);
    testOk((ellCount(&pch->pre_chain) == 1 && filter->pre_arg == NULL),
           "ts has one filter w/o argument in pre chain");
    testOk((ellCount(&pch->post_chain) == 0), "ts has no filter in post chain");

    fl_reset(&fl);
    fl1 = fl;
    pfl2 = dbChannelRunPreChain(pch, &fl1);
    testOk(pfl2 == &fl1, "ts filter does not drop or replace field_log");
    testOk(fl_equal_ex_ts(&fl1, pfl2),
           "ts filter does not change field_log data");

    testOk(!!(pfl2 = db_create_read_log(pch)), "create field log from channel");
    stamp = pfl2->time;
    db_delete_field_log(pfl2);

    pfl2 = dbChannelRunPreChain(pch, &fl1);
    epicsTimeGetCurrent(&now);
    testOk(epicsTimeDiffInSeconds(&pfl2->time, &stamp) >= 0 &&
           epicsTimeDiffInSeconds(&now, &pfl2->time) >= 0,
           "ts filter sets time stamp to \"now\"");

    dbChannelDelete(pch);
}

static void test_value_filter(const chFilterPlugin *plug, const char *chan_name,
                              type_check tc_func, value_check vc_func) {
    dbChannel *pch;
    chFilter *filter;
    db_field_log fl;
    db_field_log fl2;
    db_field_log *pfl;
    epicsTimeStamp ts;
    ELLNODE *node;
    chPostEventFunc *cb_out = NULL;
    void *arg_out = NULL;

    testDiag("Channel %s", chan_name);

    testOk(!!(pch = dbChannelCreate(chan_name)),
           "dbChannel with plugin ts created");
    testOk((ellCount(&pch->filters) == 1), "channel has one plugin");

    fl_reset(&fl);
    fl.type = dbfl_type_val;
    node = ellFirst(&pch->filters);
    filter = CONTAINER(node, chFilter, list_node);
    plug->fif->channel_register_pre(filter, &cb_out, &arg_out, &fl);
    plug->fif->channel_register_post(filter, &cb_out, &arg_out, &fl);
    testOk(!!(cb_out) && arg_out,
           "register_post registers one filter with argument");
    testOk(tc_func(&fl), "register_post gives correct field type");

    testOk(!(dbChannelOpen(pch)), "dbChannel with plugin ts opened");
    node = ellFirst(&pch->post_chain);
    filter = CONTAINER(node, chFilter, post_node);
    testOk((ellCount(&pch->post_chain) == 1 && filter->post_arg != NULL),
           "ts has one filter with argument in post chain");
    testOk((ellCount(&pch->pre_chain) == 0), "ts has no filter in pre chain");

    fl_reset(&fl);
    fl.type = dbfl_type_val;
    ts = fl.time;
    fl2 = fl;
    pfl = dbChannelRunPostChain(pch, &fl);
    testOk(pfl == &fl, "ts filter does not drop or replace field_log");
    testOk(tc_func(pfl), "ts filter gives correct field type");
    testOk((pfl->time.secPastEpoch == fl2.time.secPastEpoch &&
            pfl->time.nsec == fl2.time.nsec && pfl->stat == fl2.stat &&
            pfl->sevr == fl2.sevr),
           "ts filter does not touch non-value fields of field_log");
    testOk(vc_func(pfl, &ts), "ts filter gives correct field value");

    dbChannelDelete(pch);
}

static int type_check_double(const db_field_log *pfl) {
    return pfl->type == dbfl_type_val
        && pfl->field_type == DBR_DOUBLE
        && pfl->field_size == sizeof(epicsFloat64)
        && pfl->no_elements == 1;
}

static int value_check_double(const db_field_log *pfl, const epicsTimeStamp *ts) {
    epicsFloat64 flt = pfl->u.v.field.dbf_double;
    epicsFloat64 nsec = (flt - (epicsUInt32)(flt)) * 1e9;
    return ts->secPastEpoch == (epicsUInt32)(flt)
        && fabs(ts->nsec - nsec) < 1000.; /* allow loss of precision */
}

static int type_check_sec_nsec(const db_field_log *pfl) {
    return pfl->type == dbfl_type_val
        && pfl->field_type == DBR_ULONG
        && pfl->field_size == sizeof(epicsUInt32)
        && pfl->no_elements == 1;
}

static int value_check_sec(const db_field_log *pfl, const epicsTimeStamp *ts) {
    return ts->secPastEpoch == pfl->u.v.field.dbf_ulong;
}

static int value_check_nsec(const db_field_log *pfl, const epicsTimeStamp *ts) {
    return ts->nsec == pfl->u.v.field.dbf_ulong;
}

static int type_check_array(const db_field_log *pfl) {
    return pfl->field_type == DBR_ULONG
        && pfl->field_size == sizeof(epicsUInt32)
        && pfl->no_elements == 2;
}

static int value_check_array(const db_field_log *pfl, const epicsTimeStamp *ts) {
    epicsUInt32 *arr = (epicsUInt32*)pfl->u.r.field;
    return pfl->type == dbfl_type_ref
        && pfl->u.r.field != NULL
        && pfl->u.r.dtor != NULL
        && pfl->u.r.pvt == NULL
        && ts->secPastEpoch == arr[0]
        && ts->nsec == arr[1];
}

static int value_check_unix(const db_field_log *pfl, const epicsTimeStamp *ts) {
    epicsUInt32 *arr = (epicsUInt32 *)pfl->u.r.field;
    return pfl->type == dbfl_type_ref
        && pfl->u.r.field != NULL
        && pfl->u.r.dtor != NULL
        && pfl->u.r.pvt == NULL
        && ts->secPastEpoch == arr[0] - POSIX_TIME_AT_EPICS_EPOCH
        && ts->nsec == arr[1];
}

static int type_check_string(const db_field_log *pfl) {
    return pfl->field_type == DBR_STRING
        && pfl->field_size == MAX_STRING_SIZE
        && pfl->no_elements == 1;
}

static int value_check_string(const db_field_log *pfl, const epicsTimeStamp *ts) {
    /* We can only verify the type, not the value, because using strptime()
       might be problematic. */
    return pfl->type == dbfl_type_ref
        && pfl->u.r.field != NULL
        && pfl->u.r.dtor != NULL
        && pfl->u.r.pvt == NULL;
}

MAIN(tsTest) {
    int i;
    char ts[] = "ts";
    dbEventCtx evtctx;
    /* const chFilterPlugin *plug */
    const chFilterPlugin *plug;

    char const *test_channels[] = {
        "x.TIME",
        "x.VAL{ts:{\"num\": \"dbl\"}}",
        "x.VAL{ts:{\"num\": \"sec\"}}",
        "x.VAL{ts:{\"num\": \"nsec\"}}",
        "x.VAL{ts:{\"num\": \"ts\"}}",
        "x.VAL{ts:{\"num\": \"ts\", \"epoch\": \"epics\"}}",
        "x.VAL{ts:{\"num\": \"ts\", \"epoch\": \"unix\"}}",
        "x.VAL{ts:{\"str\": \"epics\"}}",
        "x.VAL{ts:{\"str\": \"iso\"}}",
    };

    type_check type_checks[] = {
        type_check_double,
        type_check_double,
        type_check_sec_nsec,
        type_check_sec_nsec,
        type_check_array,
        type_check_array,
        type_check_array,
        type_check_string,
        type_check_string,
    };

    value_check value_checks[] = {
        value_check_double,
        value_check_double,
        value_check_sec,
        value_check_nsec,
        value_check_array,
        value_check_array,
        value_check_unix,
        value_check_string,
        value_check_string,
    };

    int const num_value_tests = sizeof(test_channels) / sizeof(char *);

    testPlan(12 /* test_generate_filter() */
             + num_value_tests * 11 /* test_value_filter() */);

    testdbPrepare();

    testdbReadDatabase("filterTest.dbd", NULL, NULL);

    filterTest_registerRecordDeviceDriver(pdbbase);

    testdbReadDatabase("xRecord.db", NULL, NULL);

    eltc(0);
    testIocInitOk();
    eltc(1);

    evtctx = db_init_events();

    testOk(!!(plug = dbFindFilter(ts, strlen(ts))),
           "plugin ts registered correctly");

    test_generate_filter(plug);

    for (i = 0; i < num_value_tests; ++i) {
        test_value_filter(plug, test_channels[i], type_checks[i], value_checks[i]);
    }

    db_close_events(evtctx);

    testIocShutdownOk();

    testdbCleanup();

    return testDone();
}
