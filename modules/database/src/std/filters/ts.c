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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chfPlugin.h"
#include "dbExtractArray.h"
#include "dbLock.h"
#include "db_field_log.h"
#include "epicsExport.h"
#include "freeList.h"
#include "errlog.h"

/* Allocation size for freelists */
#define ALLOC_NUM_ELEMENTS 32

#define logicErrorMessage() \
    errMessage(-1, "Logic error: invalid state encountered in ts filter")

/* Filter settings */

enum tsMode {
    tsModeInvalid = 0,
    tsModeGenerate = 1,
    tsModeDouble = 2,
    tsModeSec = 3,
    tsModeNsec = 4,
    tsModeArray = 5,
    tsModeString = 6,
};

static const chfPluginEnumType ts_numeric_enum[] = {
    {"dbl", 2}, {"sec", 3}, {"nsec", 4}, {"ts", 5}};

enum tsEpoch {
    tsEpochEpics = 0,
    tsEpochUnix = 1,
};

static const chfPluginEnumType ts_epoch_enum[] = {{"epics", 0}, {"unix", 1}};

enum tsString {
    tsStringInvalid = 0,
    tsStringEpics = 1,
    tsStringIso = 2,
};

static const chfPluginEnumType ts_string_enum[] = {{"epics", 1}, {"iso", 2}};

typedef struct tsPrivate {
    enum tsMode mode;
    enum tsEpoch epoch;
    enum tsString str;
} tsPrivate;

static const chfPluginArgDef ts_args[] = {
    chfEnum(tsPrivate, mode, "num", 0, 0, ts_numeric_enum),
    chfEnum(tsPrivate, epoch, "epoch", 0, 0, ts_epoch_enum),
    chfEnum(tsPrivate, str, "str", 0, 0, ts_string_enum),
    chfPluginArgEnd
};

static int parse_finished(void *pvt) {
    tsPrivate *settings = (tsPrivate *)pvt;
    if (settings->str != tsStringInvalid) {
        settings->mode = tsModeString;
#if defined _MSC_VER && _MSC_VER <= 1700
        // VS 2012 crashes in ISO mode, doesn't support timezones
        if (settings->str == tsStringIso) {
            return -1;
        }
#endif
    } else if (settings->mode == tsModeInvalid) {
        settings->mode = tsModeGenerate;
    }
    return 0;
}


/* Allocation of filter settings */
static void *private_free_list;

static void * allocPvt() {
    return freeListCalloc(private_free_list);
}

static void freePvt(void *pvt) {
    freeListFree(private_free_list, pvt);
}


/* Allocation of two-element arrays for second+nanosecond pairs */
static void *ts_array_free_list;

static void *allocTsArray() {
    return freeListCalloc(ts_array_free_list);
}

static void freeTsArray(db_field_log *pfl) {
    freeListFree(ts_array_free_list, pfl->u.r.field);
}

/* Allocation of strings */
static void *string_free_list;

static void *allocString() {
    return freeListCalloc(string_free_list);
}

static void freeString(db_field_log *pfl) {
    freeListFree(string_free_list, pfl->u.r.field);
}


/* The dtor for waveform data for the case when we have to copy it. */
static void freeArray(db_field_log *pfl) {
    /*
     * The size of the data is different for each channel, and can even
     * change at runtime, so a freeList doesn't make much sense here.
     */
    free(pfl->u.r.field);
}

static db_field_log* generate(void* pvt, dbChannel *chan, db_field_log *pfl) {
    epicsTimeStamp now;
    epicsTimeGetCurrent(&now);

    /* If reference and not already copied,
       must make a copy (to ensure coherence between time and data) */
    if (pfl->type == dbfl_type_ref && !pfl->dtor) {
        void *pTarget = calloc(pfl->no_elements, pfl->field_size);
        void *pSource = pfl->u.r.field;
        if (pTarget) {
            long offset = 0;
            long nSource = pfl->no_elements;
            dbScanLock(dbChannelRecord(chan));
            dbChannelGetArrayInfo(chan, &pSource, &nSource, &offset);
            dbExtractArray(pSource, pTarget, pfl->field_size,
                           nSource, pfl->no_elements, offset, 1);
            pfl->u.r.field = pTarget;
            pfl->dtor = freeArray;
            pfl->u.r.pvt = pvt;
            dbScanUnlock(dbChannelRecord(chan));
        }
    }

    pfl->time = now;
    return pfl;
}

static db_field_log *replace_fl_value(tsPrivate const *pvt,
                                      db_field_log *pfl,
                                      int (*func)(tsPrivate const *,
                                                  db_field_log *)) {
    /* Get rid of the old value */
    if (pfl->type == dbfl_type_ref && pfl->dtor) {
        pfl->dtor(pfl);
        pfl->dtor = NULL;
    }
    pfl->no_elements = 1;
    pfl->type = dbfl_type_val;

    if (func(pvt, pfl)) {
        db_delete_field_log(pfl);
        pfl = NULL;
    }

    return pfl;
}

static void ts_to_array(tsPrivate const *settings,
                        epicsTimeStamp const *ts,
                        epicsUInt32 arr[2]) {
    arr[0] = ts->secPastEpoch;
    arr[1] = ts->nsec;
    if (settings->epoch == tsEpochUnix) {
        /* Cannot use epicsTimeToWhatever because Whatever uses signed ints */
        arr[0] += POSIX_TIME_AT_EPICS_EPOCH;
    }
}

static int ts_seconds(tsPrivate const *settings, db_field_log *pfl) {
    epicsUInt32 arr[2];
    ts_to_array(settings, &pfl->time, arr);
    pfl->field_type = DBF_ULONG;
    pfl->field_size = sizeof(epicsUInt32);
    pfl->u.v.field.dbf_ulong = arr[0];
    return 0;
}

static int ts_nanos(tsPrivate const *settings, db_field_log *pfl) {
    epicsUInt32 arr[2];
    ts_to_array(settings, &pfl->time, arr);
    pfl->field_type = DBF_ULONG;
    pfl->field_size = sizeof(epicsUInt32);
    pfl->u.v.field.dbf_ulong = arr[1];
    return 0;
}

static int ts_double(tsPrivate const *settings, db_field_log *pfl) {
    epicsUInt32 arr[2];
    ts_to_array(settings, &pfl->time, arr);
    pfl->field_type = DBF_DOUBLE;
    pfl->field_size = sizeof(epicsFloat64);
    pfl->u.v.field.dbf_double = arr[0] + arr[1] * 1e-9;
    return 0;
}

static int ts_array(tsPrivate const *settings, db_field_log *pfl) {
    pfl->field_type = DBF_ULONG;
    pfl->field_size = sizeof(epicsUInt32);
    pfl->type = dbfl_type_ref;
    pfl->u.r.pvt = NULL;
    pfl->u.r.field = allocTsArray();
    if (pfl->u.r.field) {
        pfl->no_elements = 2;
        pfl->dtor = freeTsArray;
        ts_to_array(settings, &pfl->time, (epicsUInt32*)pfl->u.r.field);
    } else {
        pfl->no_elements = 0;
        pfl->dtor = NULL;
    }
    return 0;
}

static int ts_string(tsPrivate const *settings, db_field_log *pfl) {
    char const *fmt;
    char *field;
    size_t n;

    switch (settings->str) {
    case tsStringEpics:
        fmt = "%Y-%m-%d %H:%M:%S.%06f";
        break;
    case tsStringIso:
        fmt = "%Y-%m-%dT%H:%M:%S.%06f%z";
        break;
    case tsStringInvalid:
    default:
        logicErrorMessage();
        return 1;
    }

    pfl->field_type = DBF_STRING;
    pfl->field_size = MAX_STRING_SIZE;
    pfl->type = dbfl_type_ref;
    pfl->u.r.pvt = NULL;
    pfl->u.r.field = allocString();

    if (!pfl->u.r.field) {
        pfl->no_elements = 0;
        pfl->dtor = NULL;
        return 0;
    }

    pfl->dtor = freeString;

    field = (char *)pfl->u.r.field;
    n = epicsTimeToStrftime(field, MAX_STRING_SIZE, fmt, &pfl->time);
    if (!n) {
        field[0] = 0;
    }

    return 0;
}

static db_field_log *filter(void *pvt, dbChannel *chan, db_field_log *pfl) {
    tsPrivate *settings = (tsPrivate *)pvt;
    (void)chan;

    switch (settings->mode) {
    case tsModeDouble:
        return replace_fl_value(pvt, pfl, ts_double);
    case tsModeSec:
        return replace_fl_value(pvt, pfl, ts_seconds);
    case tsModeNsec:
        return replace_fl_value(pvt, pfl, ts_nanos);
    case tsModeArray:
        return replace_fl_value(pvt, pfl, ts_array);
    case tsModeString:
        return replace_fl_value(pvt, pfl, ts_string);
    case tsModeGenerate:
    case tsModeInvalid:
    default:
        logicErrorMessage();
        db_delete_field_log(pfl);
        pfl = NULL;
    }

    return pfl;
}


/* Only the "generate" mode is registered for the pre-queue chain as it creates
   it's own timestamp which should be as close to the event as possible */
static void channelRegisterPre(dbChannel * chan, void *pvt,
                               chPostEventFunc **cb_out, void **arg_out,
                               db_field_log *probe) {
    tsPrivate *settings = (tsPrivate *)pvt;
    (void)chan;
    (void)arg_out;
    (void)probe;

    *cb_out = settings->mode == tsModeGenerate ? generate : NULL;
}

/* For other modes, the post-chain is fine as they only manipulate existing
   timestamps */
static void channelRegisterPost(dbChannel *chan, void *pvt,
                                chPostEventFunc **cb_out, void **arg_out,
                                db_field_log *probe) {
    tsPrivate *settings = (tsPrivate *)pvt;
    (void)chan;

    if (settings->mode == tsModeGenerate || settings->mode == tsModeInvalid) {
        *cb_out = NULL;
        return;
    }

    *cb_out = filter;
    *arg_out = pvt;

    /* Get rid of the value of the probe because we will be changing the
       datatype */
    if (probe->type == dbfl_type_ref && probe->dtor) {
        probe->dtor(probe);
        probe->dtor = NULL;
    }
    probe->no_elements = 1;
    probe->type = dbfl_type_val;

    switch (settings->mode) {
    case tsModeArray:
        probe->no_elements = 2;
        /* fallthrough */
    case tsModeSec:
    case tsModeNsec:
        probe->field_type = DBF_ULONG;
        probe->field_size = sizeof(epicsUInt32);
        break;
    case tsModeDouble:
        probe->field_type = DBF_DOUBLE;
        probe->field_size = sizeof(epicsFloat64);
        break;
    case tsModeString:
        probe->field_type = DBF_STRING;
        probe->field_size = MAX_STRING_SIZE;
        break;
    case tsModeGenerate:
    case tsModeInvalid:
        // Already handled above, added here for completeness.
    default:
        logicErrorMessage();
        *cb_out = NULL;
    }
}

static void channel_report(dbChannel *chan, void *pvt, int level, const unsigned short indent)
{
    tsPrivate *settings = (tsPrivate *)pvt;
    (void)chan;
    (void)level;

    printf("%*sTimestamp (ts): mode: %d, epoch: %d, str: %d\n",
           indent, "", settings->mode, settings->epoch, settings->str);
}

static chfPluginIf pif = {
    allocPvt,
    freePvt,

    NULL,           /* parse_error, */
    parse_finished,

    NULL, /* channel_open, */
    channelRegisterPre,
    channelRegisterPost,
    channel_report,
    NULL /* channel_close */
};

static void tsInitialize(void)
{
    freeListInitPvt(&private_free_list, sizeof(tsPrivate),
                    ALLOC_NUM_ELEMENTS);
    freeListInitPvt(&ts_array_free_list, 2 * sizeof(epicsUInt32),
                    ALLOC_NUM_ELEMENTS);
    freeListInitPvt(&string_free_list, MAX_STRING_SIZE,
                    ALLOC_NUM_ELEMENTS);
    chfPluginRegister("ts", &pif, ts_args);
}

epicsExportRegistrar(tsInitialize);
