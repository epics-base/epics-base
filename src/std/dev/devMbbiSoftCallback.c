/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devMbbiSoftCallback.c */
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
#include "mbbiRecord.h"
#include "epicsExport.h"


#define GET_OPTIONS (DBR_STATUS | DBR_TIME)

typedef struct devPvt {
    processNotify *ppn;
    CALLBACK *pcallback;
    long options;
    int status;
    struct {
        DBRstatus
        DBRtime
        epicsEnum16 value;
    } buffer;
} devPvt;


static void getCallback(processNotify *ppn, notifyGetType type)
{
    mbbiRecord *prec = (mbbiRecord *)ppn->usrPvt;
    devPvt *pdevPvt = (devPvt *)prec->dpvt;
    long no_elements = 1;

    if (ppn->status == notifyCanceled) {
        printf("devMbbiSoftCallback::getCallback notifyCanceled\n");
        return;
    }

    assert(type == getFieldType);
    pdevPvt->status = dbGetField(ppn->paddr, DBR_ENUM,
        &pdevPvt->buffer, &pdevPvt->options, &no_elements, 0);
}

static void doneCallback(processNotify *ppn)
{
    mbbiRecord *prec = (mbbiRecord *)ppn->usrPvt;
    devPvt *pdevPvt = (devPvt *)prec->dpvt;

    callbackRequestProcessCallback(pdevPvt->pcallback, prec->prio, prec);
}

static long add_record(dbCommon *pcommon)
{
    mbbiRecord *prec = (mbbiRecord *)pcommon;
    DBLINK *plink = &prec->inp;
    DBADDR *pdbaddr;
    long status;
    devPvt *pdevPvt;
    processNotify *ppn;

    if (plink->type == CONSTANT) return 0;

    if (plink->type != PV_LINK) {
        recGblRecordError(S_db_badField, (void *)prec,
            "devMbbiSoftCallback (add_record) Illegal INP field");
        return S_db_badField;
    }

    pdbaddr = callocMustSucceed(1, sizeof(*pdbaddr),
        "devMbbiSoftCallback::add_record");
    status = dbNameToAddr(plink->value.pv_link.pvname, pdbaddr);
    if (status) {
        free(pdbaddr);
        recGblRecordError(status, (void *)prec,
            "devMbbiSoftCallback (add_record) linked record not found");
        return status;
    }

    plink->type = PN_LINK;
    plink->value.pv_link.precord = pcommon;
    plink->value.pv_link.pvt = pdbaddr;
    plink->value.pv_link.pvlMask &= pvlOptMsMode;   /* Severity flags only */

    ppn = callocMustSucceed(1, sizeof(*ppn),
        "devMbbiSoftCallback::add_record");
    ppn->usrPvt = prec;
    ppn->paddr = pdbaddr;
    ppn->getCallback = getCallback;
    ppn->doneCallback = doneCallback;
    ppn->requestType = processGetRequest;

    pdevPvt = callocMustSucceed(1, sizeof(*pdevPvt),
        "devMbbiSoftCallback::add_record");
    pdevPvt->pcallback = callocMustSucceed(1, sizeof(CALLBACK),
        "devMbbiSoftCallback::add_record");
    pdevPvt->ppn = ppn;
    pdevPvt->options = GET_OPTIONS;

    prec->dpvt = pdevPvt;
    return 0;
}

static long del_record(dbCommon *pcommon) {
    mbbiRecord *prec = (mbbiRecord *)pcommon;
    DBLINK *plink = &prec->inp;
    devPvt *pdevPvt = (devPvt *)prec->dpvt;

    if (plink->type == CONSTANT) return 0;
    assert(plink->type == PN_LINK);

    dbNotifyCancel(pdevPvt->ppn);
    free(pdevPvt->ppn);
    free(pdevPvt->pcallback);
    free(pdevPvt);
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

static long init_record(mbbiRecord *prec)
{
    /* INP must be CONSTANT or PN_LINK */
    switch (prec->inp.type) {
    case CONSTANT:
        if (recGblInitConstantLink(&prec->inp, DBR_ENUM, &prec->val))
            prec->udf = FALSE;
        break;
    case PN_LINK:
        /* Handled by add_record */
        break;
    default:
        recGblRecordError(S_db_badField, (void *)prec,
            "devMbbiSoftCallback (init_record) Illegal INP field");
        prec->pact = TRUE;
        return S_db_badField;
    }
    return 0;
}

static long read_mbbi(mbbiRecord *prec)
{
    devPvt *pdevPvt = (devPvt *)prec->dpvt;

    if (!prec->dpvt)
        return 2;

    if (!prec->pact) {
        dbProcessNotify(pdevPvt->ppn);
        prec->pact = TRUE;
        return 0;
    }

    if (pdevPvt->status) {
        recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
        return 2;
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

    if (prec->tsel.type == CONSTANT &&
        prec->tse == epicsTimeEventDeviceTime)
        prec->time = pdevPvt->buffer.time;
    return 2;
}

/* Create the dset for devMbbiSoftCallback */
struct {
    dset common;
    DEVSUPFUN read_mbbi;
} devMbbiSoftCallback = {
    {5, NULL, init, init_record, NULL},
    read_mbbi
};
epicsExportAddress(dset,devMbbiSoftCallback);
