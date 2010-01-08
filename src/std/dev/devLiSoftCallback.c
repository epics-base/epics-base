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
#include "dbNotify.h"
#include "epicsAssert.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "link.h"
#include "longinRecord.h"
#include "epicsExport.h"


#define GET_OPTIONS (DBR_STATUS | DBR_TIME)

typedef struct notifyInfo {
    processNotify *ppn;
    CALLBACK *pcallback;
    long options;
    int status;
    struct {
        DBRstatus
        DBRtime
        epicsInt32 value;
    } buffer;
} notifyInfo;


static void getCallback(processNotify *ppn, notifyGetType type)
{
    longinRecord *prec = (longinRecord *)ppn->usrPvt;
    notifyInfo *pnotifyInfo = (notifyInfo *)prec->dpvt;
    long no_elements = 1;

    if (ppn->status == notifyCanceled) {
        printf("devLiSoftCallback::getCallback notifyCanceled\n");
        return;
    }

    assert(type == getFieldType);
    pnotifyInfo->status = dbGetField(ppn->paddr, DBR_LONG,
        &pnotifyInfo->buffer, &pnotifyInfo->options, &no_elements, 0);
}

static void doneCallback(processNotify *ppn)
{
    longinRecord *prec = (longinRecord *)ppn->usrPvt;
    notifyInfo *pnotifyInfo = (notifyInfo *)prec->dpvt;

    callbackRequestProcessCallback(pnotifyInfo->pcallback, prec->prio, prec);
}

static long add_record(dbCommon *pcommon)
{
    longinRecord *prec = (longinRecord *)pcommon;
    DBLINK *plink = &prec->inp;
    DBADDR *pdbaddr;
    long  status;
    notifyInfo *pnotifyInfo;
    processNotify *ppn;

    if (plink->type == CONSTANT) return 0;

    if (plink->type != PV_LINK) {
        recGblRecordError(S_db_badField, (void *)prec,
            "devLiSoftCallback (add_record) Illegal INP field");
        return S_db_badField;
    }

    pdbaddr = callocMustSucceed(1, sizeof(*pdbaddr),
        "devLiSoftCallback::add_record");
    status = dbNameToAddr(plink->value.pv_link.pvname, pdbaddr);
    if (status) {
        free(pdbaddr);
        recGblRecordError(status, (void *)prec,
            "devLiSoftCallback (init_record) linked record not found");
        return status;
    }

    plink->type = PN_LINK;
    plink->value.pv_link.precord = pcommon;
    plink->value.pv_link.pvt = pdbaddr;
    plink->value.pv_link.pvlMask &= pvlOptMsMode;   /* Severity flags only */

    ppn = callocMustSucceed(1, sizeof(*ppn),
        "devLiSoftCallback::add_record");
    ppn->usrPvt = prec;
    ppn->paddr = pdbaddr;
    ppn->getCallback = getCallback;
    ppn->doneCallback = doneCallback;
    ppn->requestType = processGetRequest;

    pnotifyInfo = callocMustSucceed(1, sizeof(*pnotifyInfo),
        "devLiSoftCallback::add_record");
    pnotifyInfo->pcallback = callocMustSucceed(1, sizeof(CALLBACK),
        "devLiSoftCallback::add_record");
    pnotifyInfo->ppn = ppn;
    pnotifyInfo->options = GET_OPTIONS;

    prec->dpvt = pnotifyInfo;
    return 0;
}

static long del_record(dbCommon *pcommon) {
    longinRecord *prec = (longinRecord *)pcommon;
    DBLINK *plink = &prec->inp;
    notifyInfo *pnotifyInfo = (notifyInfo *)prec->dpvt;

    if (plink->type == CONSTANT) return 0;
    assert(plink->type == PN_LINK);

    dbNotifyCancel(pnotifyInfo->ppn);
    free(pnotifyInfo->ppn);
    free(pnotifyInfo->pcallback);
    free(pnotifyInfo);
    free(plink->value.pv_link.pvt);

    plink->type = PV_LINK;
    plink->value.pv_link.pvt = NULL;
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
    /* INP must be CONSTANT or PN_LINK */
    switch (prec->inp.type) {
    case CONSTANT:
        if (recGblInitConstantLink(&prec->inp, DBR_LONG, &prec->val))
            prec->udf = FALSE;
        break;
    case PN_LINK:
        /* Handled by add_record */
        break;
    default:
        recGblRecordError(S_db_badField, (void *)prec,
            "devLiSoftCallback (init_record) Illegal INP field");
        prec->pact = TRUE;
        return S_db_badField;
    }
    return 0;
}

static long read_li(longinRecord *prec)
{
    notifyInfo *pnotifyInfo = (notifyInfo *)prec->dpvt;

    if (!prec->dpvt)
        return 0;

    if (!prec->pact) {
        dbProcessNotify(pnotifyInfo->ppn);
        prec->pact = TRUE;
        return 0;
    }

    if (pnotifyInfo->status) {
        recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
        return pnotifyInfo->status;
    }

    prec->val = pnotifyInfo->buffer.value;
    prec->udf = FALSE;

    switch (prec->inp.value.pv_link.pvlMask & pvlOptMsMode) {
        case pvlOptNMS:
            break;
        case pvlOptMSI:
            if (pnotifyInfo->buffer.severity < INVALID_ALARM)
                break;
            /* else fall through */
        case pvlOptMS:
            recGblSetSevr(prec, LINK_ALARM, pnotifyInfo->buffer.severity);
            break;
        case pvlOptMSS:
            recGblSetSevr(prec, pnotifyInfo->buffer.status,
                pnotifyInfo->buffer.severity);
            break;
    }

    if (prec->tsel.type == CONSTANT &&
        prec->tse == epicsTimeEventDeviceTime)
        prec->time = pnotifyInfo->buffer.time;
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
