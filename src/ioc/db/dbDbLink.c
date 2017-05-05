/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* dbDbLink.c
 *
 *      Original Authors: Bob Dalesio, Marty Kraimer
 *      Current Author: Andrew Johnson
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

#define epicsExportSharedSymbols
#include "dbAccessDefs.h"
#include "dbAddr.h"
#include "dbBase.h"
#include "dbBkpt.h"
#include "dbCommon.h"
#include "dbConvertFast.h"
#include "dbConvert.h"
#include "db_field_log.h"
#include "dbFldTypes.h"
#include "dbLink.h"
#include "dbLockPvt.h"
#include "dbNotify.h"
#include "dbScan.h"
#include "dbStaticLib.h"
#include "devSup.h"
#include "link.h"
#include "recGbl.h"
#include "recSup.h"
#include "special.h"

/***************************** Database Links *****************************/

/* Forward definition */
static lset dbDb_lset;

long dbDbInitLink(struct link *plink, short dbfType)
{
    DBADDR dbaddr;
    long status;
    DBADDR *pdbAddr;

    status = dbNameToAddr(plink->value.pv_link.pvname, &dbaddr);
    if (status)
        return status;

    plink->lset = &dbDb_lset;
    plink->type = DB_LINK;
    pdbAddr = dbCalloc(1, sizeof(struct dbAddr));
    *pdbAddr = dbaddr; /* structure copy */
    plink->value.pv_link.pvt = pdbAddr;
    ellAdd(&dbaddr.precord->bklnk, &plink->value.pv_link.backlinknode);
    /* merging into the same lockset is deferred to the caller.
     * cf. initPVLinks()
     */
    dbLockSetMerge(NULL, plink->precord, dbaddr.precord);
    assert(plink->precord->lset->plockSet == dbaddr.precord->lset->plockSet);
    return 0;
}

void dbDbAddLink(struct dbLocker *locker, struct link *plink, short dbfType,
    DBADDR *ptarget)
{
    plink->lset = &dbDb_lset;
    plink->type = DB_LINK;
    plink->value.pv_link.pvt = ptarget;
    ellAdd(&ptarget->precord->bklnk, &plink->value.pv_link.backlinknode);

    /* target record is already locked in dbPutFieldLink() */
    dbLockSetMerge(locker, plink->precord, ptarget->precord);
}

static void dbDbRemoveLink(struct dbLocker *locker, struct link *plink)
{
    DBADDR *pdbAddr = (DBADDR *) plink->value.pv_link.pvt;

    plink->type = PV_LINK;

    /* locker is NULL when an isolated IOC is closing its links */
    if (locker) {
        plink->value.pv_link.pvt = 0;
        plink->value.pv_link.getCvt = 0;
        plink->value.pv_link.pvlMask = 0;
        plink->value.pv_link.lastGetdbrType = 0;
        ellDelete(&pdbAddr->precord->bklnk, &plink->value.pv_link.backlinknode);
        dbLockSetSplit(locker, plink->precord, pdbAddr->precord);
    }
    free(pdbAddr);
}

static int dbDbIsConnected(const struct link *plink)
{
    return TRUE;
}

static int dbDbGetDBFtype(const struct link *plink)
{
    DBADDR *paddr = (DBADDR *) plink->value.pv_link.pvt;

    return paddr->field_type;
}

static long dbDbGetElements(const struct link *plink, long *nelements)
{
    DBADDR *paddr = (DBADDR *) plink->value.pv_link.pvt;

    *nelements = paddr->no_elements;
    return 0;
}

static long dbDbGetValue(struct link *plink, short dbrType, void *pbuffer,
        long *pnRequest)
{
    struct pv_link *ppv_link = &plink->value.pv_link;
    DBADDR *paddr = ppv_link->pvt;
    dbCommon *precord = plink->precord;
    long status;

    /* scan passive records if link is process passive  */
    if (ppv_link->pvlMask & pvlOptPP) {
        unsigned char pact = precord->pact;

        precord->pact = TRUE;
        status = dbScanPassive(precord, paddr->precord);
        precord->pact = pact;
        if (status)
            return status;
    }

    if (ppv_link->getCvt && ppv_link->lastGetdbrType == dbrType) {
        status = ppv_link->getCvt(paddr->pfield, pbuffer, paddr);
    } else {
        unsigned short dbfType = paddr->field_type;

        if (dbrType < 0 || dbrType > DBR_ENUM || dbfType > DBF_DEVICE)
            return S_db_badDbrtype;

        if (paddr->no_elements == 1 && (!pnRequest || *pnRequest == 1)
                && paddr->special != SPC_DBADDR
                && paddr->special != SPC_ATTRIBUTE) {
            ppv_link->getCvt = dbFastGetConvertRoutine[dbfType][dbrType];
            status = ppv_link->getCvt(paddr->pfield, pbuffer, paddr);
        } else {
            ppv_link->getCvt = NULL;
            status = dbGet(paddr, dbrType, pbuffer, NULL, pnRequest, NULL);
        }
        ppv_link->lastGetdbrType = dbrType;
    }

    if (!status)
        recGblInheritSevr(plink->value.pv_link.pvlMask & pvlOptMsMode,
            plink->precord, paddr->precord->stat, paddr->precord->sevr);
    return status;
}

static long dbDbGetControlLimits(const struct link *plink, double *low,
        double *high)
{
    DBADDR *paddr = (DBADDR *) plink->value.pv_link.pvt;
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
    DBADDR *paddr = (DBADDR *) plink->value.pv_link.pvt;
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
    DBADDR *paddr = (DBADDR *) plink->value.pv_link.pvt;
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
    DBADDR *paddr = (DBADDR *) plink->value.pv_link.pvt;
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
    DBADDR *paddr = (DBADDR *) plink->value.pv_link.pvt;
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

static long dbDbGetAlarm(const struct link *plink, epicsEnum16 *status,
        epicsEnum16 *severity)
{
    DBADDR *paddr = (DBADDR *) plink->value.pv_link.pvt;

    if (status)
        *status = paddr->precord->stat;
    if (severity)
        *severity = paddr->precord->sevr;
    return 0;
}

static long dbDbGetTimeStamp(const struct link *plink, epicsTimeStamp *pstamp)
{
    DBADDR *paddr = (DBADDR *) plink->value.pv_link.pvt;

    *pstamp = paddr->precord->time;
    return 0;
}

static long dbDbPutValue(struct link *plink, short dbrType,
        const void *pbuffer, long nRequest)
{
    struct pv_link *ppv_link = &plink->value.pv_link;
    struct dbCommon *psrce = plink->precord;
    DBADDR *paddr = (DBADDR *) ppv_link->pvt;
    dbCommon *pdest = paddr->precord;
    long status = dbPut(paddr, dbrType, pbuffer, nRequest);

    recGblInheritSevr(ppv_link->pvlMask & pvlOptMsMode, pdest, psrce->nsta,
        psrce->nsev);
    if (status)
        return status;

    if (paddr->pfield == (void *) &pdest->proc ||
            (ppv_link->pvlMask & pvlOptPP && pdest->scan == 0)) {
        /* if dbPutField caused asyn record to process */
        /* ask for reprocessing*/
        if (pdest->putf) {
            pdest->rpro = TRUE;
        } else { /* process dest record with source's PACT true */
            unsigned char pact;

            if (psrce && psrce->ppn)
                dbNotifyAdd(psrce, pdest);
            pact = psrce->pact;
            psrce->pact = TRUE;
            status = dbProcess(pdest);
            psrce->pact = pact;
        }
    }
    return status;
}

static void dbDbScanFwdLink(struct link *plink)
{
    dbCommon *precord = plink->precord;
    dbAddr *paddr = (dbAddr *) plink->value.pv_link.pvt;

    dbScanPassive(precord, paddr->precord);
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
    dbDbGetAlarm, dbDbGetTimeStamp,
    dbDbPutValue, NULL,
    dbDbScanFwdLink, doLocked
};
