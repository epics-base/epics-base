/*************************************************************************\
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <Ralph.Lange@bessy.de>
 */

#include <stdio.h>

#include "chfPlugin.h"
#include "dbAccessDefs.h"
#include "dbExtractArray.h"
#include "db_field_log.h"
#include "dbLock.h"
#include "epicsExit.h"
#include "freeList.h"
#include "epicsExport.h"

typedef struct myStruct {
    epicsInt32 start;
    epicsInt32 incr;
    epicsInt32 end;
    void *arrayFreeList;
    long no_elements;
} myStruct;

static void *myStructFreeList;

static const chfPluginArgDef opts[] = {
    chfInt32 (myStruct, start, "s", 0, 1),
    chfInt32 (myStruct, incr, "i", 0, 1),
    chfInt32 (myStruct, end, "e", 0, 1),
    chfPluginArgEnd
};

static void * allocPvt(void)
{
    myStruct *my = (myStruct*) freeListCalloc(myStructFreeList);
    if (!my) return NULL;

    /* defaults */
    my->start = 0;
    my->incr = 1;
    my->end = -1;
    return (void *) my;
}

static void freePvt(void *pvt)
{
    myStruct *my = (myStruct*) pvt;

    if (my->arrayFreeList) freeListCleanup(my->arrayFreeList);
    freeListFree(myStructFreeList, pvt);
}

static int parse_ok(void *pvt)
{
    myStruct *my = (myStruct*) pvt;

    if (my->incr <= 0) my->incr = 1;
    return 0;
}

static void freeArray(db_field_log *pfl)
{
    if (pfl->type == dbfl_type_ref) {
        freeListFree(pfl->u.r.pvt, pfl->u.r.field);
    }
}

static long wrapArrayIndices(long *start, const long increment, long *end,
    const long no_elements)
{
    if (*start < 0) *start = no_elements + *start;
    if (*start < 0) *start = 0;
    if (*start > no_elements) *start = no_elements;

    if (*end < 0) *end = no_elements + *end;
    if (*end < 0) *end = 0;
    if (*end >= no_elements) *end = no_elements - 1;

    if (*end - *start >= 0)
        return 1 + (*end - *start) / increment;
    else
        return 0;
}

static db_field_log* filter(void* pvt, dbChannel *chan, db_field_log *pfl)
{
    myStruct *my = (myStruct*) pvt;
    int must_lock;
    long start = my->start;
    long end = my->end;
    long nTarget;
    void *pTarget;
    long offset = 0;
    long nSource = pfl->no_elements;
    void *pSource = pfl->u.r.field;

    switch (pfl->type) {
    case dbfl_type_val:
        /* TODO Treat scalars as arrays with 1 element */
        break;

    case dbfl_type_ref:
        must_lock = !pfl->dtor;
        if (must_lock) {
            dbScanLock(dbChannelRecord(chan));
            dbChannelGetArrayInfo(chan, &pSource, &nSource, &offset);
        }
        nTarget = wrapArrayIndices(&start, my->incr, &end, nSource);
        if (nTarget > 0) {
            /* copy the data */
            pTarget = freeListCalloc(my->arrayFreeList);
            if (!pTarget) break;
            /* must do the wrap-around with the original no_elements */
            offset = (offset + start) % pfl->no_elements;
            dbExtractArray(pSource, pTarget, pfl->field_size,
                nTarget, pfl->no_elements, offset, my->incr);
            if (pfl->dtor) pfl->dtor(pfl);
            pfl->u.r.field = pTarget;
            pfl->dtor = freeArray;
            pfl->u.r.pvt = my->arrayFreeList;
        }
        /* adjust no_elements (even if zero elements remain) */
        pfl->no_elements = nTarget;
        if (must_lock)
            dbScanUnlock(dbChannelRecord(chan));
        break;
    }
    return pfl;
}

static void channelRegisterPost(dbChannel *chan, void *pvt,
    chPostEventFunc **cb_out, void **arg_out, db_field_log *probe)
{
    myStruct *my = (myStruct*) pvt;
    long start = my->start;
    long end = my->end;
    long max = 0;

    if (probe->no_elements <= 1) return;    /* array data only */

    max = wrapArrayIndices(&start, my->incr, &end, probe->no_elements);
    if (max) {
        if (!my->arrayFreeList)
            freeListInitPvt(&my->arrayFreeList, max * probe->field_size, 2);
        if (!my->arrayFreeList) return;
    }
    probe->no_elements = my->no_elements = max;
    *cb_out = filter;
    *arg_out = pvt;
}

static void channel_report(dbChannel *chan, void *pvt, int level,
    const unsigned short indent)
{
    myStruct *my = (myStruct*) pvt;
    printf("%*sArray (arr): start=%d, incr=%d, end=%d\n", indent, "",
           my->start, my->incr, my->end);
}

static chfPluginIf pif = {
    allocPvt,
    freePvt,

    NULL, /* parse_error, */
    parse_ok,

    NULL, /* channel_open, */
    NULL, /* channelRegisterPre, */
    channelRegisterPost,
    channel_report,
    NULL /* channel_close */
};

static void arrShutdown(void* ignore)
{
    if(myStructFreeList)
        freeListCleanup(myStructFreeList);
    myStructFreeList = NULL;
}

static void arrInitialize(void)
{
    if (!myStructFreeList)
        freeListInitPvt(&myStructFreeList, sizeof(myStruct), 64);

    chfPluginRegister("arr", &pif, opts);
    epicsAtExit(arrShutdown, NULL);
}

epicsExportRegistrar(arrInitialize);
