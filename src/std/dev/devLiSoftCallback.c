/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* devLiSoftCallback.c */
/*
 *      Authors:  Marty Kraimer & Andrew Johnson
 */

#include <stdlib.h>
#include <stdio.h>

#include "alarm.h"
#include "callback.h"
#include "cantProceed.h"
#include "dbCommon.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "dbChannel.h"
#include "dbNotify.h"
#include "epicsAssert.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "link.h"
#include "longinRecord.h"
#include "epicsExport.h"


#define GET_OPTIONS (DBR_STATUS | DBR_TIME)

typedef struct devPvt {
    processNotify pn;
    CALLBACK callback;
    long options;
    int status;
    struct {
        DBRstatus
        DBRtime
        epicsInt32 value;
    } buffer;
} devPvt;


static void getCallback(processNotify *ppn, notifyGetType type)
{
    longinRecord *prec = (longinRecord *)ppn->usrPvt;
    devPvt *pdevPvt = (devPvt *)prec->dpvt;
    long no_elements = 1;

    if (ppn->status == notifyCanceled) {
        printf("devLiSoftCallback::getCallback notifyCanceled\n");
        return;
    }

    assert(type == getFieldType);
    pdevPvt->status = dbChannelGetField(ppn->chan, DBR_LONG,
        &pdevPvt->buffer, &pdevPvt->options, &no_elements, 0);
}

static void doneCallback(processNotify *ppn)
{
    longinRecord *prec = (longinRecord *)ppn->usrPvt;
    devPvt *pdevPvt = (devPvt *)prec->dpvt;

    callbackRequestProcessCallback(&pdevPvt->callback, prec->prio, prec);
}

static long add_record(dbCommon *pcommon)
{
    longinRecord *prec = (longinRecord *)pcommon;
    DBLINK *plink = &prec->inp;
    dbChannel *chan;
    devPvt *pdevPvt;
    processNotify *ppn;

    if (dbLinkIsDefined(plink) && dbLinkIsConstant(plink))
        return 0;

    if (plink->type != PV_LINK) {
        long status = S_db_badField;

        recGblRecordError(status, (void *)prec,
            "devLiSoftCallback (add_record) Illegal INP field");
        return status;
    }

    chan = dbChannelCreate(plink->value.pv_link.pvname);
    if (!chan) {
        long status = S_db_notFound;

        recGblRecordError(status, (void *)prec,
            "devLiSoftCallback (init_record) linked record not found");
        return status;
    }

    pdevPvt = calloc(1, sizeof(*pdevPvt));
    if (!pdevPvt) {
        long status = S_db_noMemory;

        recGblRecordError(status, (void *)prec,
            "devLiSoftCallback (add_record) out of memory, calloc() failed");
        return status;
    }
    ppn = &pdevPvt->pn;

    plink->type = PN_LINK;
    plink->value.pv_link.pvlMask &= pvlOptMsMode;   /* Severity flags only */

    ppn->usrPvt = prec;
    ppn->chan = chan;
    ppn->getCallback = getCallback;
    ppn->doneCallback = doneCallback;
    ppn->requestType = processGetRequest;

    pdevPvt->options = GET_OPTIONS;

    prec->dpvt = pdevPvt;
    return 0;
}

static long del_record(dbCommon *pcommon) {
    longinRecord *prec = (longinRecord *)pcommon;
    DBLINK *plink = &prec->inp;
    devPvt *pdevPvt = (devPvt *)prec->dpvt;

    if (dbLinkIsDefined(plink) && dbLinkIsConstant(plink))
        return 0;

    assert(plink->type == PN_LINK);

    dbNotifyCancel(&pdevPvt->pn);
    dbChannelDelete(pdevPvt->pn.chan);
    free(pdevPvt);

    plink->type = PV_LINK;
    return 0;
}

static struct dsxt dsxtSoftCallback = {
    add_record, del_record
};

static long init(int pass)
{
    if (pass == 0) devExtend(&dsxtSoftCallback);
    return 0;
}

static long init_record(longinRecord *prec)
{
    if (recGblInitConstantLink(&prec->inp, DBR_LONG, &prec->val))
        prec->udf = FALSE;

    return 0;
}

static long read_li(longinRecord *prec)
{
    devPvt *pdevPvt = (devPvt *)prec->dpvt;

    if (!prec->dpvt)
        return 0;

    if (!prec->pact) {
        dbProcessNotify(&pdevPvt->pn);
        prec->pact = TRUE;
        return 0;
    }

    if (pdevPvt->status) {
        recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
        return pdevPvt->status;
    }

    prec->val = pdevPvt->buffer.value;
    prec->udf = FALSE;

    switch (prec->inp.value.pv_link.pvlMask & pvlOptMsMode) {
        case pvlOptNMS:
            break;
        case pvlOptMSI:
            if (pdevPvt->buffer.severity < INVALID_ALARM)
                break;
            /* else fall through */
        case pvlOptMS:
            recGblSetSevr(prec, LINK_ALARM, pdevPvt->buffer.severity);
            break;
        case pvlOptMSS:
            recGblSetSevr(prec, pdevPvt->buffer.status,
                pdevPvt->buffer.severity);
            break;
    }

    if (dbLinkIsConstant(&prec->tsel) &&
        prec->tse == epicsTimeEventDeviceTime)
        prec->time = pdevPvt->buffer.time;

    return 0;
}

/* Create the dset for devLiSoftCallback */
struct {
    dset common;
    DEVSUPFUN read_li;
} devLiSoftCallback = {
    {5, NULL, init, init_record, NULL},
    read_li
};
epicsExportAddress(dset, devLiSoftCallback);
