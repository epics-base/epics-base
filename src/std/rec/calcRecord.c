/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* Record Support Routines for Calculation records */
/*
 *      Original Author: Julie Sander and Bob Dalesio
 *      Date:            7-27-87
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "errlog.h"
#include "alarm.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "dbFldTypes.h"
#include "epicsMath.h"
#include "errMdef.h"
#include "recSup.h"
#include "recGbl.h"
#include "special.h"

#define GEN_SIZE_OFFSET
#include "calcRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

/* Hysterisis for alarm filtering: 1-1/e */
#define THRESHOLD 0.6321

/* Create RSET - Record Support Entry Table */

#define report NULL
#define initialize NULL
static long init_record(struct dbCommon *prec, int pass);
static long process(struct dbCommon *prec);
static long special(DBADDR *paddr, int after);
#define get_value NULL
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
static long get_units(DBADDR *paddr, char *units);
static long get_precision(const DBADDR *paddr, long *precision);
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double(DBADDR *paddr, struct dbr_grDouble *pgd);
static long get_control_double(DBADDR *paddr, struct dbr_ctrlDouble *pcd);
static long get_alarm_double(DBADDR *paddr, struct dbr_alDouble *pad);

rset calcRSET={
    RSETNUMBER,
    report,
    initialize,
    init_record,
    process,
    special,
    get_value,
    cvt_dbaddr,
    get_array_info,
    put_array_info,
    get_units,
    get_precision,
    get_enum_str,
    get_enum_strs,
    put_enum_str,
    get_graphic_double,
    get_control_double,
    get_alarm_double
};
epicsExportAddress(rset, calcRSET);

static void checkAlarms(calcRecord *prec, epicsTimeStamp *timeLast);
static void monitor(calcRecord *prec);
static int fetch_values(calcRecord *prec);


static long init_record(struct dbCommon *pcommon, int pass)
{
    struct calcRecord *prec = (struct calcRecord *)pcommon;
    struct link *plink;
    double *pvalue;
    int i;
    short error_number;

    if (pass==0) return(0);

    plink = &prec->inpa;
    pvalue = &prec->a;
    for (i = 0; i < CALCPERFORM_NARGS; i++, plink++, pvalue++) {
        recGblInitConstantLink(plink, DBF_DOUBLE, pvalue);
    }
    if (postfix(prec->calc, prec->rpcl, &error_number)) {
        recGblRecordError(S_db_badField, (void *)prec,
                          "calc: init_record: Illegal CALC field");
        errlogPrintf("%s.CALC: %s in expression \"%s\"\n",
                     prec->name, calcErrorStr(error_number), prec->calc);
    }
    return 0;
}

static long process(struct dbCommon *pcommon)
{
    struct calcRecord *prec = (struct calcRecord *)pcommon;
    epicsTimeStamp timeLast;

    prec->pact = TRUE;
    if (fetch_values(prec) == 0) {
        if (calcPerform(&prec->a, &prec->val, prec->rpcl)) {
            recGblSetSevr(prec, CALC_ALARM, INVALID_ALARM);
        } else
            prec->udf = isnan(prec->val);
    }

    timeLast = prec->time;
    recGblGetTimeStamp(prec);
    /* check for alarms */
    checkAlarms(prec, &timeLast);
    /* check event list */
    monitor(prec);
    /* process the forward scan link record */
    recGblFwdLink(prec);
    prec->pact = FALSE;
    return 0;
}

static long special(DBADDR *paddr, int after)
{
    calcRecord *prec = (calcRecord *)paddr->precord;
    short error_number;

    if (!after) return 0;
    if (paddr->special == SPC_CALC) {
        if (postfix(prec->calc, prec->rpcl, &error_number)) {
            recGblRecordError(S_db_badField, (void *)prec,
                              "calc: Illegal CALC field");
            errlogPrintf("%s.CALC: %s in expression \"%s\"\n",
                         prec->name, calcErrorStr(error_number), prec->calc);
            return S_db_badField;
        }
        return 0;
    }
    recGblDbaddrError(S_db_badChoice, paddr, "calc::special - bad special value!");
    return S_db_badChoice;
}

#define indexof(field) calcRecord##field

static long get_linkNumber(int fieldIndex) {
    if (fieldIndex >= indexof(A) && fieldIndex <= indexof(L))
        return fieldIndex - indexof(A);
    if (fieldIndex >= indexof(LA) && fieldIndex <= indexof(LL))
        return fieldIndex - indexof(LA);
    return -1;
}

static long get_units(DBADDR *paddr, char *units)
{
    calcRecord *prec = (calcRecord *)paddr->precord;
    int linkNumber;

    if(paddr->pfldDes->field_type == DBF_DOUBLE) {
        linkNumber = get_linkNumber(dbGetFieldIndex(paddr));
        if (linkNumber >= 0)
            dbGetUnits(&prec->inpa + linkNumber, units, DB_UNITS_SIZE);
        else
            strncpy(units,prec->egu,DB_UNITS_SIZE);
    }
    return 0;
}

static long get_precision(const DBADDR *paddr, long *pprecision)
{
    calcRecord *prec = (calcRecord *)paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);
    int linkNumber;

    *pprecision = prec->prec;
    if (fieldIndex == indexof(VAL))
        return 0;

    linkNumber = get_linkNumber(fieldIndex);
    if (linkNumber >= 0) {
        short precision;

        if (dbGetPrecision(&prec->inpa + linkNumber, &precision) == 0)
            *pprecision = precision;
    } else
        recGblGetPrec(paddr, pprecision);
    return 0;
}

static long get_graphic_double(DBADDR *paddr, struct dbr_grDouble *pgd)
{
    calcRecord *prec = (calcRecord *)paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);
    int linkNumber;
    
    switch (fieldIndex) {
        case indexof(VAL):
        case indexof(HIHI):
        case indexof(HIGH):
        case indexof(LOW):
        case indexof(LOLO):
        case indexof(LALM):
        case indexof(ALST):
        case indexof(MLST):
            pgd->lower_disp_limit = prec->lopr;
            pgd->upper_disp_limit = prec->hopr;
            break;
        default:
            linkNumber = get_linkNumber(fieldIndex);
            if (linkNumber >= 0) {
                dbGetGraphicLimits(&prec->inpa + linkNumber,
                    &pgd->lower_disp_limit,
                    &pgd->upper_disp_limit);
            } else
                recGblGetGraphicDouble(paddr,pgd);
    }
    return 0;
}

static long get_control_double(DBADDR *paddr, struct dbr_ctrlDouble *pcd)
{
    calcRecord *prec = (calcRecord *)paddr->precord;
    
    switch (dbGetFieldIndex(paddr)) {
        case indexof(VAL):
        case indexof(HIHI):
        case indexof(HIGH):
        case indexof(LOW):
        case indexof(LOLO):
        case indexof(LALM):
        case indexof(ALST):
        case indexof(MLST):
            pcd->lower_ctrl_limit = prec->lopr;
            pcd->upper_ctrl_limit = prec->hopr;
            break;
        default:
            recGblGetControlDouble(paddr,pcd);
    }
    return 0;
}

static long get_alarm_double(DBADDR *paddr, struct dbr_alDouble *pad)
{
    calcRecord *prec = (calcRecord *)paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);
    int linkNumber;

    if (fieldIndex == indexof(VAL)) {
        pad->lower_alarm_limit = prec->llsv ? prec->lolo : epicsNAN;
        pad->lower_warning_limit = prec->lsv ? prec->low : epicsNAN;
        pad->upper_warning_limit = prec->hsv ? prec->high : epicsNAN;
        pad->upper_alarm_limit = prec->hhsv ? prec->hihi : epicsNAN;
    } else {
        linkNumber = get_linkNumber(fieldIndex);
        if (linkNumber >= 0) {
            dbGetAlarmLimits(&prec->inpa + linkNumber,
                &pad->lower_alarm_limit,
                &pad->lower_warning_limit,
                &pad->upper_warning_limit,
                &pad->upper_alarm_limit);
        } else
            recGblGetAlarmDouble(paddr, pad);
    }
    return 0;
}

static void checkAlarms(calcRecord *prec, epicsTimeStamp *timeLast)
{

	enum {
		range_Lolo = 1,
		range_Low,
		range_Normal,
		range_High,
		range_Hihi
	} alarmRange;
	static const epicsEnum16 range_stat[] = {
		SOFT_ALARM, LOLO_ALARM, LOW_ALARM, 
		NO_ALARM, HIGH_ALARM, HIHI_ALARM
	};

    double val, hyst, lalm, alev, aftc, afvl;
    epicsEnum16 asev;

    if (prec->udf) {
        recGblSetSevr(prec, UDF_ALARM, prec->udfs);
	prec->afvl = 0;
        return;
    }

    val = prec->val;
    hyst = prec->hyst;
    lalm = prec->lalm;

        /* check VAL against alarm limits */
    if ((asev = prec->hhsv) &&
        (val >= (alev = prec->hihi) ||
         ((lalm == alev) && (val >= alev - hyst))))
        alarmRange = range_Hihi;
    else
    if ((asev = prec->llsv) &&
        (val <= (alev = prec->lolo) ||
         ((lalm == alev) && (val <= alev + hyst))))
        alarmRange = range_Lolo;
    else
    if ((asev = prec->hsv) &&
        (val >= (alev = prec->high) ||
         ((lalm == alev) && (val >= alev - hyst))))
        alarmRange = range_High;
    else
    if ((asev = prec->lsv) &&
        (val <= (alev = prec->low) ||
         ((lalm == alev) && (val <= alev + hyst))))
        alarmRange = range_Low;
    else {
        alev = val;
        asev = NO_ALARM;
        alarmRange = range_Normal;
    }

    aftc = prec->aftc;
    afvl = 0;

    if (aftc > 0) {
        /* Apply level filtering */
        afvl = prec->afvl;
        if (afvl == 0) {
            afvl = (double)alarmRange;
        } else {
            double t = epicsTimeDiffInSeconds(&prec->time, timeLast);
            double alpha = aftc / (t + aftc);

            /* The sign of afvl indicates whether the result should be
             * rounded up or down.  This gives the filter hysteresis.
             * If afvl > 0 the floor() function rounds to a lower alarm
             * level, otherwise to a higher.
             */
            afvl = alpha * afvl +
                   ((afvl > 0) ? (1 - alpha) : (alpha - 1)) * alarmRange;
            if (afvl - floor(afvl) > THRESHOLD)
                afvl = -afvl; /* reverse rounding */

            alarmRange = abs((int)floor(afvl));
            switch (alarmRange) {
            case range_Hihi:
                asev = prec->hhsv;
                alev = prec->hihi;
                break;
            case range_High:
                asev = prec->hsv;
                alev = prec->high;
                break;
            case range_Normal:
                asev = NO_ALARM;
                break;
            case range_Low:
                asev = prec->lsv;
                alev = prec->low;
                break;
            case range_Lolo:
                asev = prec->llsv;
                alev = prec->lolo;
                break;
            }
        }
    }
    prec->afvl = afvl;

    if (asev) {
        /* Report alarm condition, store LALM for future HYST calculations */
        if (recGblSetSevr(prec, range_stat[alarmRange], asev))
            prec->lalm = alev;
    } else {
        /* No alarm condition, reset LALM */
        prec->lalm = val;
    }

}

static void monitor(calcRecord *prec)
{
    unsigned monitor_mask;
    double *pnew, *pprev;
    int i;

    monitor_mask = recGblResetAlarms(prec);

    /* check for value change */
    recGblCheckDeadband(&prec->mlst, prec->val, prec->mdel, &monitor_mask, DBE_VALUE);

    /* check for archive change */
    recGblCheckDeadband(&prec->alst, prec->val, prec->adel, &monitor_mask, DBE_ARCHIVE);

    /* send out monitors connected to the value field */
    if (monitor_mask){
        db_post_events(prec, &prec->val, monitor_mask);
    }

    /* check all input fields for changes*/
    pnew = &prec->a;
    pprev = &prec->la;
    for (i = 0; i < CALCPERFORM_NARGS; i++, pnew++, pprev++) {
        if (*pnew != *pprev ||
            monitor_mask & DBE_ALARM) {
            db_post_events(prec, pnew, monitor_mask | DBE_VALUE | DBE_LOG);
            *pprev = *pnew;
        }
    }
    return;
}

static int fetch_values(calcRecord *prec)
{
    struct link *plink;
    double *pvalue;
    long status = 0;
    int i;

    plink = &prec->inpa;
    pvalue = &prec->a;
    for(i = 0; i < CALCPERFORM_NARGS; i++, plink++, pvalue++) {
        int newStatus;

        newStatus = dbGetLink(plink, DBR_DOUBLE, pvalue, 0, 0);
        if (status == 0) status = newStatus;
    }
    return status;
}
