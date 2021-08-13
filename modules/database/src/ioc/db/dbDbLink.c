/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* dbDbLink.c
 *
 *      Original Authors: Bob Dalesio, Marty Kraimer
 *      Current Author: Andrew Johnson
 */

/* The PUTF and RPRO fields in dbCommon are flags that indicate when a record
 * is being processed as a result of an external put (i.e. some server process
 * calling dbPutField()), ensuring that the record and its successors will
 * eventually get processed even if they happen to be busy at the time of the
 * put. From Base-3.16.2 and 7.0.2 the code ensures that all records downstream
 * from the original are processed even if a busy asynchronous device appears
 * in the processing chain (this breaks the chain in older versions).
 *
 * PUTF - This field is set in dbPutField() prior to it calling dbProcess().
 *        It is normally cleared at the end of processing in recGblFwdLink().
 *        It may also be cleared in dbProcess() if DISA==DISV (scan disabled),
 *        or by the processTarget() function below.
 *
 * If PUTF is TRUE before a call to dbProcess(prec), then after it returns
 * either PACT is TRUE, or PUTF will be FALSE.
 *
 * RPRO - This field is set by dbPutField() or by the processTarget() function
 *        below when a record to be processed is found to be busy (PACT==1).
 *        It is normally cleared in recGblFwdLink() when the record is queued
 *        for re-processing, or in dbProcess() if DISA==DISV (scan disabled).
 */


#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "cantProceed.h"
#include "cvtFast.h"
#include "dbDefs.h"
#include "ellLib.h"
#include "epicsTime.h"
#include "errlog.h"

#include "caeventmask.h"

#include "dbAccessDefs.h"
#include "dbAddr.h"
#include "dbBase.h"
#include "dbBkpt.h"
#include "dbCommonPvt.h"
#include "dbConvertFast.h"
#include "dbConvert.h"
#include "db_field_log.h"
#include "db_access_routines.h"
#include "dbFldTypes.h"
#include "dbLink.h"
#include "dbLockPvt.h"
#include "dbNotify.h"
#include "dbScan.h"
#include "dbStaticLib.h"
#include "dbServer.h"
#include "devSup.h"
#include "link.h"
#include "recGbl.h"
#include "recSup.h"
#include "special.h"
#include "dbDbLink.h"
#include "dbChannel.h"

/***************************** Database Links *****************************/

/* Forward definitions */
static lset dbDb_lset;

static long processTarget(dbCommon *psrc, dbCommon *pdst);

#define linkChannel(plink) ((dbChannel *) (plink)->value.pv_link.pvt)

long dbDbInitLink(struct link *plink, short dbfType)
{
    long status;
    dbChannel *chan;
    dbCommon *precord;

    chan = dbChannelCreate(plink->value.pv_link.pvname);
    if (!chan)
        return S_db_notFound;
    status = dbChannelOpen(chan);
    if (status)
        return status;

    precord = dbChannelRecord(chan);

    plink->lset = &dbDb_lset;
    plink->type = DB_LINK;
    plink->value.pv_link.pvt = chan;
    ellAdd(&precord->bklnk, &plink->value.pv_link.backlinknode);
    /* merging into the same lockset is deferred to the caller.
     * cf. initPVLinks()
     */
    dbLockSetMerge(NULL, plink->precord, precord);
    assert(plink->precord->lset->plockSet == precord->lset->plockSet);
    return 0;
}

void dbDbAddLink(struct dbLocker *locker, struct link *plink, short dbfType,
    dbChannel *chan)
{
    plink->lset = &dbDb_lset;
    plink->type = DB_LINK;
    plink->value.pv_link.pvt = chan;
    ellAdd(&dbChannelRecord(chan)->bklnk, &plink->value.pv_link.backlinknode);

    /* target record is already locked in dbPutFieldLink() */
    dbLockSetMerge(locker, plink->precord, dbChannelRecord(chan));
}

static void dbDbRemoveLink(struct dbLocker *locker, struct link *plink)
{
    dbChannel *chan = linkChannel(plink);
    dbCommon *precord = dbChannelRecord(chan);

    plink->type = PV_LINK;

    /* locker is NULL when an isolated IOC is closing its links */
    if (locker) {
        plink->value.pv_link.pvt = 0;
        plink->value.pv_link.getCvt = 0;
        plink->value.pv_link.pvlMask = 0;
        plink->value.pv_link.lastGetdbrType = 0;
        ellDelete(&precord->bklnk, &plink->value.pv_link.backlinknode);
        dbLockSetSplit(locker, plink->precord, precord);
    }
    dbChannelDelete(chan);
}

static int dbDbIsConnected(const struct link *plink)
{
    return TRUE;
}

static int dbDbGetDBFtype(const struct link *plink)
{
    dbChannel *chan = linkChannel(plink);
    return dbChannelFinalFieldType(chan);
}

static long dbDbGetElements(const struct link *plink, long *nelements)
{
    dbChannel *chan = linkChannel(plink);
    *nelements = dbChannelFinalElements(chan);
    return 0;
}

static long dbDbGetValue(struct link *plink, short dbrType, void *pbuffer,
        long *pnRequest)
{
    struct pv_link *ppv_link = &plink->value.pv_link;
    dbChannel *chan = linkChannel(plink);
    DBADDR *paddr = &chan->addr;
    dbCommon *precord = plink->precord;
    db_field_log *pfl = NULL;
    long status;

    /* scan passive records if link is process passive  */
    if (ppv_link->pvlMask & pvlOptPP) {
        status = dbScanPassive(precord, dbChannelRecord(chan));
        if (status)
            return status;
    }

    if (ppv_link->getCvt && ppv_link->lastGetdbrType == dbrType)
    {
        /* shortcut: scalar with known conversion, no filter */
        status = ppv_link->getCvt(dbChannelField(chan), pbuffer, paddr);
    }
    else if (dbChannelFinalElements(chan) == 1 && (!pnRequest || *pnRequest == 1)
                && dbChannelSpecial(chan) != SPC_DBADDR
                && dbChannelSpecial(chan) != SPC_ATTRIBUTE
                && ellCount(&chan->filters) == 0)
    {
        /* Simple scalar w/o filters, so *Final* type has no additional information.
         * Needed to correctly handle DBF_MENU fields, which become DBF_ENUM during
         * probe of dbChannelOpen().
         */
        unsigned short dbfType = dbChannelFieldType(chan);

        if (dbrType < 0 || dbrType > DBR_ENUM || dbfType > DBF_DEVICE)
            return S_db_badDbrtype;

        ppv_link->getCvt = dbFastGetConvertRoutine[dbfType][dbrType];
        ppv_link->lastGetdbrType = dbrType;
        status = ppv_link->getCvt(dbChannelField(chan), pbuffer, paddr);
    }
    else
    {
        /* filter, array, or special */
        ppv_link->getCvt = NULL;

        if (ellCount(&chan->filters)) {
            /* If filters are involved in a read, create field log and run filters */
            pfl = db_create_read_log(chan);
            if (!pfl)
                return S_db_noMemory;

            pfl = dbChannelRunPreChain(chan, pfl);
            pfl = dbChannelRunPostChain(chan, pfl);
        }

        status = dbChannelGet(chan, dbrType, pbuffer, NULL, pnRequest, pfl);

        if (pfl)
            db_delete_field_log(pfl);

        if (status)
            return status;
    }

    if (!status && precord != dbChannelRecord(chan))
        recGblInheritSevr(plink->value.pv_link.pvlMask & pvlOptMsMode,
            plink->precord,
            dbChannelRecord(chan)->stat, dbChannelRecord(chan)->sevr);
    return status;
}

static long dbDbGetControlLimits(const struct link *plink, double *low,
        double *high)
{
    dbChannel *chan = linkChannel(plink);
    DBADDR *paddr = &chan->addr;
    struct buffer {
        DBRctrlDouble
        double value;
    } buffer;
    long options = DBR_CTRL_DOUBLE;
    long number_elements = 0;
    long status = dbGet(paddr, DBR_DOUBLE, &buffer, &options, &number_elements,
            NULL);

    if (status)
        return status;

    *low = buffer.lower_ctrl_limit;
    *high = buffer.upper_ctrl_limit;
    return 0;
}

static long dbDbGetGraphicLimits(const struct link *plink, double *low,
        double *high)
{
    dbChannel *chan = linkChannel(plink);
    DBADDR *paddr = &chan->addr;
    struct buffer {
        DBRgrDouble
        double value;
    } buffer;
    long options = DBR_GR_DOUBLE;
    long number_elements = 0;
    long status = dbGet(paddr, DBR_DOUBLE, &buffer, &options, &number_elements,
            NULL);

    if (status)
        return status;

    *low = buffer.lower_disp_limit;
    *high = buffer.upper_disp_limit;
    return 0;
}

static long dbDbGetAlarmLimits(const struct link *plink, double *lolo,
        double *low, double *high, double *hihi)
{
    dbChannel *chan = linkChannel(plink);
    DBADDR *paddr = &chan->addr;
    struct buffer {
        DBRalDouble
        double value;
    } buffer;
    long options = DBR_AL_DOUBLE;
    long number_elements = 0;
    long status = dbGet(paddr, DBR_DOUBLE, &buffer, &options, &number_elements,
            0);

    if (status)
        return status;

    *lolo = buffer.lower_alarm_limit;
    *low = buffer.lower_warning_limit;
    *high = buffer.upper_warning_limit;
    *hihi = buffer.upper_alarm_limit;
    return 0;
}

static long dbDbGetPrecision(const struct link *plink, short *precision)
{
    dbChannel *chan = linkChannel(plink);
    DBADDR *paddr = &chan->addr;
    struct buffer {
        DBRprecision
        double value;
    } buffer;
    long options = DBR_PRECISION;
    long number_elements = 0;
    long status = dbGet(paddr, DBR_DOUBLE, &buffer, &options, &number_elements,
            0);

    if (status)
        return status;

    *precision = (short) buffer.precision.dp;
    return 0;
}

static long dbDbGetUnits(const struct link *plink, char *units, int unitsSize)
{
    dbChannel *chan = linkChannel(plink);
    DBADDR *paddr = &chan->addr;
    struct buffer {
        DBRunits
        double value;
    } buffer;
    long options = DBR_UNITS;
    long number_elements = 0;
    long status = dbGet(paddr, DBR_DOUBLE, &buffer, &options, &number_elements,
            0);

    if (status)
        return status;

    strncpy(units, buffer.units, unitsSize);
    return 0;
}

static long dbDbGetAlarmMsg(const struct link *plink, epicsEnum16 *status,
                            epicsEnum16 *severity, char *msgbuf, size_t msgbuflen)
{
    dbChannel *chan = linkChannel(plink);
    dbCommon *precord = dbChannelRecord(chan);
    if (status)
        *status = precord->stat;
    if (severity)
        *severity = precord->sevr;
    if (msgbuf && msgbuflen) {
        strncpy(msgbuf, precord->amsg, msgbuflen-1);
        msgbuf[msgbuflen-1] = '\0';
    }
    return 0;
}

static long dbDbGetTimeStampTag(const struct link *plink, epicsTimeStamp *pstamp, epicsUTag *ptag)
{
    dbChannel *chan = linkChannel(plink);
    dbCommon *precord = dbChannelRecord(chan);
    *pstamp = precord->time;
    if(ptag)
        *ptag = precord->utag;
    return 0;
}

static long dbDbPutValue(struct link *plink, short dbrType,
        const void *pbuffer, long nRequest)
{
    struct pv_link *ppv_link = &plink->value.pv_link;
    dbChannel *chan = linkChannel(plink);
    struct dbCommon *psrce = plink->precord;
    DBADDR *paddr = &chan->addr;
    dbCommon *pdest = dbChannelRecord(chan);
    long status = dbPut(paddr, dbrType, pbuffer, nRequest);

    recGblInheritSevr(ppv_link->pvlMask & pvlOptMsMode, pdest, psrce->nsta,
        psrce->nsev);
    if (status)
        return status;

    if (dbChannelField(chan) == (void *) &pdest->proc ||
        (ppv_link->pvlMask & pvlOptPP && pdest->scan == 0)) {
        status = processTarget(psrce, pdest);
    }

    return status;
}

static void dbDbScanFwdLink(struct link *plink)
{
    dbCommon *precord = plink->precord;
    dbChannel *chan = linkChannel(plink);
    dbScanPassive(precord, dbChannelRecord(chan));
}

static long doLocked(struct link *plink, dbLinkUserCallback rtn, void *priv)
{
    return rtn(plink, priv);
}

static lset dbDb_lset = {
    0, 0, /* not Constant, not Volatile */
    NULL, dbDbRemoveLink,
    NULL, NULL, NULL,
    dbDbIsConnected,
    dbDbGetDBFtype, dbDbGetElements,
    dbDbGetValue,
    dbDbGetControlLimits, dbDbGetGraphicLimits, dbDbGetAlarmLimits,
    dbDbGetPrecision, dbDbGetUnits,
    NULL, NULL,
    dbDbPutValue, NULL,
    dbDbScanFwdLink, doLocked,
    dbDbGetAlarmMsg,
    dbDbGetTimeStampTag,
};


/*
 *  Process a record if its scan field is passive.
 */
long dbScanPassive(dbCommon *pfrom, dbCommon *pto)
{
    /* if not passive we're done */
    if (pto->scan != 0)
        return 0;

    return processTarget(pfrom, pto);
}

static long processTarget(dbCommon *psrc, dbCommon *pdst)
{
    char context[40] = "";
    int trace = dbAccessDebugPUTF && *dbLockSetAddrTrace(psrc);
    int claim_src = dbRec2Pvt(psrc)->procThread==NULL;
    int claim_dst = psrc!=pdst && dbRec2Pvt(pdst)->procThread==NULL;
    long status;
    epicsUInt8 pact = psrc->pact;
    epicsThreadId self = epicsThreadGetIdSelf();

    psrc->pact = TRUE;

    if (psrc->ppn)
        dbNotifyAdd(psrc, pdst);

    if (trace && dbServerClient(context, sizeof(context))) {
        /* No client, use thread name */
        strncpy(context, epicsThreadGetNameSelf(), sizeof(context));
        context[sizeof(context) - 1] = 0;
    }

    if (!pdst->pact) {
        /* Normal propagation of PUTF from src to dst */
        if (trace)
            printf("%s: '%s' -> '%s' with PUTF=%u\n",
                context, psrc->name, pdst->name, psrc->putf);

        pdst->putf = psrc->putf;
    }
    else if (psrc->putf && claim_dst) {
        /* The dst record is busy (awaiting async reprocessing),
         * not being processed recursively by us, and
         * we were originally triggered by a call to dbPutField(),
         * so we mark the dst record for reprocessing once the async
         * completion is over.
         */
        if (trace)
            printf("%s: '%s' -> Active '%s', setting RPRO=1\n",
                context, psrc->name, pdst->name);

        pdst->putf = FALSE;
        pdst->rpro = TRUE;
    }
    else {
        /* The dst record is busy, but either is being processed recursively,
         * or wasn't triggered by a call to dbPutField(). Do nothing.
         */
        if (trace)
            printf("%s: '%s' -> Active '%s', done\n",
                context, psrc->name, pdst->name);
    }

    if(claim_src) {
        dbRec2Pvt(psrc)->procThread = self;
    }
    if(claim_dst) {
        dbRec2Pvt(pdst)->procThread = self;
    }

    if(dbRec2Pvt(psrc)->procThread!=self ||
       dbRec2Pvt(pdst)->procThread!=self) {
        errlogPrintf("Logic Error: processTarget 1 from %p, %s(%p) -> %s(%p)\n",
                     self, psrc->name, dbRec2Pvt(psrc), pdst->name, dbRec2Pvt(pdst));
    }

    status = dbProcess(pdst);

    psrc->pact = pact;

    if(dbRec2Pvt(psrc)->procThread!=self ||
       dbRec2Pvt(pdst)->procThread!=self) {
        errlogPrintf("Logic Error: processTarget 2 from %p, %s(%p) -> %s(%p)\n",
                     self, psrc->name, dbRec2Pvt(psrc), pdst->name, dbRec2Pvt(pdst));
    }

    if(claim_src) {
        dbRec2Pvt(psrc)->procThread = NULL;
    }
    if(claim_dst) {
        dbRec2Pvt(pdst)->procThread = NULL;
    }

    return status;
}
