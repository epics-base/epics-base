/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* recGbl.c */
/*
 *      Author:          Marty Kraimer
 *                       Andrew Johnson <anj@aps.anl.gov>
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "alarm.h"
#include "dbDefs.h"
#include "alarm.h"
#include "epicsMath.h"
#include "epicsPrint.h"
#include "epicsStdlib.h"
#include "epicsStdio.h"
#include "epicsTime.h"
#include "errlog.h"

#include "caeventmask.h"

#include "dbAccessDefs.h"
#include "dbStaticLib.h"
#include "dbAddr.h"
#include "dbBase.h"
#include "dbCommon.h"
#include "menuSimm.h"
#include "dbEvent.h"
#include "db_field_log.h"
#include "dbFldTypes.h"
#include "dbLink.h"
#include "dbNotify.h"
#include "dbScan.h"
#include "devSup.h"
#include "link.h"
#include "recGbl.h"


/* Hook Routines */

RECGBL_ALARM_HOOK_ROUTINE recGblAlarmHook = NULL;

/* local routines */
static void getMaxRangeValues(short field_type, double *pupper_limit,
    double *plower_limit);


void recGblRecordError(long status, void *pdbc,
    const char *pmessage)
{
    dbCommon *precord = pdbc;
    char errMsg[256] = "";

    if (status)
        errSymLookup(status, errMsg, sizeof(errMsg));

    errlogPrintf("recGblRecordError: %s %s PV: %s\n",
        pmessage ? pmessage : "", errMsg,
        precord ? precord->name : "Unknown");
}

void recGblDbaddrError(long status, const struct dbAddr *paddr,
    const char *pmessage)
{
    dbCommon *precord = 0;
    dbFldDes *pdbFldDes = 0;
    char errMsg[256] = "";

    if (paddr) {
        pdbFldDes = paddr->pfldDes;
        precord = paddr->precord;
    }
    if (status)
        errSymLookup(status, errMsg, sizeof(errMsg));

    errlogPrintf("recGblDbaddrError: %s %s PV: %s.%s\n",
        pmessage ? pmessage : "",errMsg,
        precord ? precord->name : "Unknown",
        pdbFldDes ? pdbFldDes->name : "");
}

void recGblRecSupError(long status, const struct dbAddr *paddr,
    const char *pmessage, const char *psupport_name)
{
    dbCommon *precord = 0;
    dbFldDes *pdbFldDes = 0;
    dbRecordType *pdbRecordType = 0;
    char errMsg[256] = "";

    if (paddr) {
        precord = paddr->precord;
        pdbFldDes = paddr->pfldDes;
        if (pdbFldDes)
            pdbRecordType = pdbFldDes->pdbRecordType;
    }

    if (status)
        errSymLookup(status, errMsg, sizeof(errMsg));

    errlogPrintf("recGblRecSupError: %s %s %s::%s PV: %s.%s\n",
        pmessage ? pmessage : "", errMsg,
        pdbRecordType ? pdbRecordType->name : "Unknown",
        psupport_name ? psupport_name : "Unknown",
        precord ? precord->name : "Unknown",
        pdbFldDes ? pdbFldDes->name : "");
}

void recGblGetPrec(const struct dbAddr *paddr, long *precision)
{
    dbFldDes *pdbFldDes = paddr->pfldDes;

    switch (pdbFldDes->field_type) {
    case DBF_CHAR:
    case DBF_UCHAR:
    case DBF_SHORT:
    case DBF_USHORT:
    case DBF_LONG:
    case DBF_ULONG:
    case DBF_INT64:
    case DBF_UINT64:
        *precision = 0;
        break;

    case DBF_FLOAT:
    case DBF_DOUBLE:
        if (*precision < 0 || *precision > 15)
            *precision = 15;
        break;

    default:
         break;
    }
}

void recGblGetGraphicDouble(const struct dbAddr *paddr,
    struct dbr_grDouble *pgd)
{
    dbFldDes *pdbFldDes = paddr->pfldDes;

    getMaxRangeValues(pdbFldDes->field_type,
        &pgd->upper_disp_limit, &pgd->lower_disp_limit);
}

void recGblGetAlarmDouble(const struct dbAddr *paddr,
    struct dbr_alDouble *pad)
{
    pad->upper_alarm_limit   = epicsNAN;
    pad->upper_warning_limit = epicsNAN;
    pad->lower_warning_limit = epicsNAN;
    pad->lower_alarm_limit   = epicsNAN;
}

void recGblGetControlDouble(const struct dbAddr *paddr,
    struct dbr_ctrlDouble *pcd)
{
    dbFldDes *pdbFldDes = paddr->pfldDes;

    getMaxRangeValues(pdbFldDes->field_type,
        &pcd->upper_ctrl_limit, &pcd->lower_ctrl_limit);
}

int recGblInitConstantLink(struct link *plink, short dbftype, void *pdest)
{
    return !dbLoadLink(plink, dbftype, pdest);
}

unsigned short recGblResetAlarms(void *precord)
{
    dbCommon *pdbc = precord;
    epicsEnum16 prev_stat = pdbc->stat;
    epicsEnum16 prev_sevr = pdbc->sevr;
    epicsEnum16 new_stat = pdbc->nsta;
    epicsEnum16 new_sevr = pdbc->nsev;
    epicsEnum16 val_mask = 0;
    epicsEnum16 stat_mask = 0;

    if (new_sevr > INVALID_ALARM)
        new_sevr = INVALID_ALARM;

    if(strcmp(pdbc->namsg, pdbc->amsg)!=0) {
        strcpy(pdbc->amsg, pdbc->namsg);
        stat_mask = DBE_ALARM;
    }

    pdbc->stat = new_stat;
    pdbc->sevr = new_sevr;
    pdbc->nsta = 0;
    pdbc->nsev = 0;

    if (prev_sevr != new_sevr) {
        stat_mask = DBE_ALARM;
        db_post_events(pdbc, &pdbc->sevr, DBE_VALUE);
    }
    if (prev_stat != new_stat) {
        stat_mask |= DBE_VALUE;
    }
    if (stat_mask) {
        db_post_events(pdbc, &pdbc->stat, stat_mask);
        db_post_events(pdbc, &pdbc->amsg, stat_mask);
        val_mask = DBE_ALARM;

        if (!pdbc->ackt || new_sevr >= pdbc->acks) {
            pdbc->acks = new_sevr;
            db_post_events(pdbc, &pdbc->acks, DBE_VALUE);
        }

        if (recGblAlarmHook) {
            (*recGblAlarmHook)(pdbc, prev_sevr, prev_stat);
        }
    }
    return val_mask;
}
int recGblSetSevrMsg(void *precord, epicsEnum16 new_stat,
                     epicsEnum16 new_sevr,
                     const char *msg, ...)
{
    int ret;
    va_list args;
    va_start(args, msg);
    ret = recGblSetSevrVMsg(precord, new_stat, new_sevr, msg, args);
    va_end(args);
    return ret;
}

int recGblSetSevrVMsg(void *precord, epicsEnum16 new_stat,
                     epicsEnum16 new_sevr,
                     const char *msg, va_list args)
{
    struct dbCommon *prec = precord;
    if (prec->nsev < new_sevr) {
        prec->nsta = new_stat;
        prec->nsev = new_sevr;
        if(msg) {
            epicsVsnprintf(prec->namsg, sizeof(prec->namsg)-1, msg, args);
            prec->namsg[sizeof(prec->namsg)-1] = '\0';

        } else {
            prec->namsg[0] = '\0';
        }
        prec->namsg[sizeof(prec->namsg)-1] = '\0';
        return TRUE;
    }
    return FALSE;
}

int recGblSetSevr(void *precord, epicsEnum16 new_stat, epicsEnum16 new_sevr)
{
    return recGblSetSevrMsg(precord, new_stat, new_sevr, NULL);
}

void recGblInheritSevr(int msMode, void *precord, epicsEnum16 stat,
    epicsEnum16 sevr)
{
    switch (msMode) {
    case pvlOptNMS:
        break;
    case pvlOptMSI:
        if (sevr < INVALID_ALARM)
            break;
        /* Fall through */
    case pvlOptMS:
        recGblSetSevr(precord, LINK_ALARM, sevr);
        break;
    case pvlOptMSS:
        recGblSetSevr(precord, stat, sevr);
        break;
    }
}


void recGblFwdLink(void *precord)
{
    dbCommon *pdbc = precord;

    dbScanFwdLink(&pdbc->flnk);
    /*Handle dbPutFieldNotify record completions*/
    if(pdbc->ppn) dbNotifyCompletion(pdbc);
    if(pdbc->rpro) {
        /*If anyone requested reprocessing do it*/
        pdbc->rpro = FALSE;
        scanOnce(pdbc);
    }
    /*In case putField caused put we are all done */
    pdbc->putf = FALSE;
}

void recGblGetTimeStamp(void *pvoid)
{
    recGblGetTimeStampSimm(pvoid, menuSimmNO, 0);
}

void recGblGetTimeStampSimm(void *pvoid, const epicsEnum16 simm, struct link *siol)
{
    dbCommon *prec = (dbCommon *)pvoid;
    struct link *plink = &prec->tsel;

    if (!dbLinkIsConstant(plink)) {
        if (plink->flags & DBLINK_FLAG_TSELisTIME) {
            if (dbGetTimeStampTag(plink, &prec->time, &prec->utag))
                errlogPrintf("recGblGetTimeStamp: dbGetTimeStamp failed for %s.TSEL\n",
                    prec->name);
            return;
        }
        dbGetLink(plink, DBR_SHORT, &prec->tse, 0, 0);
    }
    if (prec->tse != epicsTimeEventDeviceTime) {
        if (epicsTimeGetEvent(&prec->time, prec->tse))
            errlogPrintf("recGblGetTimeStampSimm: epicsTimeGetEvent failed, %s.TSE = %d\n",
                         prec->name, prec->tse);
    } else {
        if (simm != menuSimmNO) {
            if (siol && !dbLinkIsConstant(siol)) {
                if (dbGetTimeStampTag(siol, &prec->time, &prec->utag))
                    errlogPrintf("recGblGetTimeStampSimm: dbGetTimeStamp (sim mode) failed, %s.SIOL = %s\n",
                        prec->name, siol->value.pv_link.pvname);
                return;
            } else {
                if (epicsTimeGetCurrent(&prec->time))
                    errlogPrintf("recGblGetTimeStampSimm: epicsTimeGetCurrent (sim mode) failed for %s.\n",
                        prec->name);
                return;
            }
        }
    }
}

void recGblCheckDeadband(epicsFloat64 *poldval, const epicsFloat64 newval,
    const epicsFloat64 deadband, unsigned *monitor_mask, const unsigned add_mask)
{
    double delta = 0;

    if (finite(newval) && finite(*poldval)) {
        /* both are finite -> compare delta with deadband */
        delta = *poldval - newval;
        if (delta < 0.0) delta = -delta;
    }
    else if (!isnan(newval) != !isnan(*poldval) ||
             !isinf(newval) != !isinf(*poldval)) {
        /* one is NaN or +-inf, the other not -> send update */
        delta = epicsINF;
    }
    else if (isinf(newval) && newval != *poldval) {
        /* one is +inf, the other -inf -> send update */
        delta = epicsINF;
    }
    if (delta > deadband) {
        /* add bits to monitor mask */
        *monitor_mask |= add_mask;
        /* update last value monitored */
        *poldval = newval;
    }
}

static void getMaxRangeValues(short field_type, double *pupper_limit,
    double *plower_limit)
{
    switch(field_type){
    case DBF_CHAR:
        *pupper_limit = (double) CHAR_MAX;
        *plower_limit = (double) CHAR_MIN;
        break;
    case DBF_UCHAR:
        *pupper_limit = (double) UCHAR_MAX;
        *plower_limit = 0.0;
        break;
    case DBF_SHORT:
        *pupper_limit = (double) SHRT_MAX;
        *plower_limit = (double) SHRT_MIN;
        break;
    case DBF_ENUM:
    case DBF_USHORT:
        *pupper_limit = (double) USHRT_MAX;
        *plower_limit = 0.0;
        break;
    case DBF_LONG:
        *pupper_limit = 2147483647.0;
        *plower_limit = -2147483648.0;
        break;
    case DBF_ULONG:
        *pupper_limit = 4294967295.0;
        *plower_limit = 0.0;
        break;
    case DBF_INT64:
        *pupper_limit = 9223372036854775808.0;
        *plower_limit = -9223372036854775808.0;
        break;
    case DBF_UINT64:
        *pupper_limit = 18446744073709551615.0;
        *plower_limit = 0.0;
        break;
    case DBF_FLOAT:
        *pupper_limit = 1e30;
        *plower_limit = -1e30;
        break;
    case DBF_DOUBLE:
        *pupper_limit = 1e300;
        *plower_limit = -1e300;
        break;
    }
    return;
}

void recGblSaveSimm(const epicsEnum16 sscn,
    epicsEnum16 *poldsimm, const epicsEnum16 simm) {
    if (sscn == USHRT_MAX) return;
    *poldsimm = simm;
}

void recGblCheckSimm(struct dbCommon *pcommon, epicsEnum16 *psscn,
    const epicsEnum16 oldsimm, const epicsEnum16 simm) {
    if (*psscn == USHRT_MAX) return;
    if (simm != oldsimm) {
        epicsUInt16 scan = pcommon->scan;
        scanDelete(pcommon);
        pcommon->scan = *psscn;
        scanAdd(pcommon);
        *psscn = scan;
    }
}

void recGblInitSimm(struct dbCommon *pcommon, epicsEnum16 *psscn,
    epicsEnum16 *poldsimm, epicsEnum16 *psimm, struct link *psiml) {
    if (dbLinkIsConstant(psiml)) {
        recGblSaveSimm(*psscn, poldsimm, *psimm);
        dbLoadLink(psiml, DBF_USHORT, psimm);
        recGblCheckSimm(pcommon, psscn, *poldsimm, *psimm);
    }
}

long recGblGetSimm(struct dbCommon *pcommon, epicsEnum16 *psscn,
    epicsEnum16 *poldsimm, epicsEnum16 *psimm, struct link *psiml) {
    long status;

    recGblSaveSimm(*psscn, poldsimm, *psimm);
    status = dbTryGetLink(psiml, DBR_USHORT, psimm, 0);
    if (status && !pcommon->nsev) pcommon->nsta = LINK_ALARM;
    recGblCheckSimm(pcommon, psscn, *poldsimm, *psimm);
    return 0;
}
