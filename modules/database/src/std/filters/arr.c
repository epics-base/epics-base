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

#include <stdio.h>

#include <freeList.h>
#include <dbAccess.h>
#include <dbExtractArray.h>
#include <db_field_log.h>
#include <dbLock.h>
#include <recSup.h>
#include <epicsExit.h>
#include <special.h>
#include <chfPlugin.h>
#include <epicsExport.h>

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
    long len = 0;

    if (*start < 0) *start = no_elements + *start;
    if (*start < 0) *start = 0;
    if (*start > no_elements) *start = no_elements;

    if (*end < 0) *end = no_elements + *end;
    if (*end < 0) *end = 0;
    if (*end >= no_elements) *end = no_elements - 1;

    if (*end - *start >= 0) len = 1 + (*end - *start) / increment;
    return len;
}

static db_field_log* filter(void* pvt, dbChannel *chan, db_field_log *pfl)
{
    myStruct *my = (myStruct*) pvt;
    struct dbCommon *prec;
    rset *prset;
    long start = my->start;
    long end = my->end;
    long nTarget = 0;
    long offset = 0;
    long nSource = chan->addr.no_elements;
    long capacity = nSource;
    void *pdst;

    switch (pfl->type) {
    case dbfl_type_val:
        /* Only filter arrays */
        break;

    case dbfl_type_rec:
        /* Extract from record */
        if (chan->addr.special == SPC_DBADDR &&
            nSource > 1 &&
            (prset = dbGetRset(&chan->addr)) &&
            prset->get_array_info)
        {
            void *pfieldsave = chan->addr.pfield;
            prec = dbChannelRecord(chan);
            dbScanLock(prec);
            prset->get_array_info(&chan->addr, &nSource, &offset);
            nTarget = wrapArrayIndices(&start, my->incr, &end, nSource);
            pfl->type = dbfl_type_ref;
            pfl->stat = prec->stat;
            pfl->sevr = prec->sevr;
            pfl->time = prec->time;
            pfl->field_type = chan->addr.field_type;
            pfl->field_size = chan->addr.field_size;
            pfl->no_elements = nTarget;
            if (nTarget) {
                pdst = freeListCalloc(my->arrayFreeList);
                if (pdst) {
                    pfl->u.r.dtor = freeArray;
                    pfl->u.r.pvt = my->arrayFreeList;
                    offset = (offset + start) % chan->addr.no_elements;
                    dbExtractArrayFromRec(&chan->addr, pdst, nTarget, capacity,
                        offset, my->incr);
                    pfl->u.r.field = pdst;
                }
            }
            dbScanUnlock(prec);
            chan->addr.pfield = pfieldsave;
        }
        break;

    /* Extract from buffer */
    case dbfl_type_ref:
        pdst = NULL;
        nSource = pfl->no_elements;
        nTarget = wrapArrayIndices(&start, my->incr, &end, nSource);
        pfl->no_elements = nTarget;
        if (nTarget) {
            /* Copy the data out */
            void *psrc = pfl->u.r.field;

            pdst = freeListCalloc(my->arrayFreeList);
            if (!pdst) break;
            offset = start;
            dbExtractArrayFromBuf(psrc, pdst, pfl->field_size, pfl->field_type,
                nTarget, nSource, offset, my->incr);
        }
        if (pfl->u.r.dtor) pfl->u.r.dtor(pfl);
        if (nTarget) {
            pfl->u.r.dtor = freeArray;
            pfl->u.r.pvt = my->arrayFreeList;
            pfl->u.r.field = pdst;
        }
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
