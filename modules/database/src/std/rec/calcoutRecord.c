/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* calcout.c - Record Support Routines for calc with output records */
/*
 *   Author : Ned Arnold
 *   Based on recCalc.c by Julie Sander and Bob Dalesio
 *      Date:            7-27-87
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "alarm.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "dbLink.h"
#include "dbScan.h"
#include "cantProceed.h"
#include "epicsMath.h"
#include "errMdef.h"
#include "errlog.h"
#include "recSup.h"
#include "devSup.h"
#include "recGbl.h"
#include "special.h"
#include "callback.h"
#include "taskwd.h"
#include "menuIvoa.h"

#define GEN_SIZE_OFFSET
#include "calcoutRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(struct dbCommon *, int);
static long process(struct dbCommon *);
static long special(DBADDR *, int);
#define get_value NULL
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
static long get_units(DBADDR *, char *);
static long get_precision(const DBADDR *, long *);
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double(DBADDR *, struct dbr_grDouble *);
static long get_control_double(DBADDR *, struct dbr_ctrlDouble *);
static long get_alarm_double(DBADDR *, struct dbr_alDouble *);

rset calcoutRSET = {
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
epicsExportAddress(rset, calcoutRSET);

int calcoutODLYprecision = 2;
epicsExportAddress(int, calcoutODLYprecision);
double calcoutODLYlimit = 100000;
epicsExportAddress(double, calcoutODLYlimit);

/* To provide feedback to the user as to the connection status of the
 * links (.INxV and .OUTV), the following algorithm has been implemented ...
 *
 * A new PV_LINK is checked [in both init() and special()] to see if the
 * target is local -- if so it is marked as such. If not, a checkLinkCb
 * callback is scheduled to check the connection status later by calling
 * dbIsLinkConnected(). Anytime there are unconnected CA_LINKs, another
 * callback is scheduled. Once all connections are established, the CA_LINKs
 * are checked whenever the record processes.
 *
 */

#define NO_CA_LINKS     0
#define CA_LINKS_ALL_OK 1
#define CA_LINKS_NOT_OK 2

typedef struct rpvtStruct {
    epicsCallback doOutCb;
    epicsCallback checkLinkCb;
    short    cbScheduled;
    short    caLinkStat; /* NO_CA_LINKS, CA_LINKS_ALL_OK, CA_LINKS_NOT_OK */
} rpvtStruct;

static void checkAlarms(calcoutRecord *prec);
static void monitor(calcoutRecord *prec);
static int fetch_values(calcoutRecord *prec);
static void execOutput(calcoutRecord *prec);
static void checkLinks(calcoutRecord *prec);
static void checkLinksCallback(epicsCallback *arg);
static long writeValue(calcoutRecord *prec);

int    calcoutRecDebug;


static long init_record(struct dbCommon *pcommon, int pass)
{
    struct calcoutRecord *prec = (struct calcoutRecord *)pcommon;
    DBLINK *plink;
    int i;
    double *pvalue;
    epicsEnum16 *plinkValid;
    short error_number;
    calcoutdset *pcalcoutDSET;
    rpvtStruct *prpvt;

    if (pass == 0) {
        prec->rpvt = (rpvtStruct *) callocMustSucceed(1, sizeof(rpvtStruct), "calcoutRecord");
        return 0;
    }

    if (!(pcalcoutDSET = (calcoutdset *)prec->dset)) {
        recGblRecordError(S_dev_noDSET, (void *)prec, "calcout:init_record");
        return S_dev_noDSET;
    }

    /* must have write defined */
    if ((pcalcoutDSET->common.number < 5) || (pcalcoutDSET->write ==NULL)) {
        recGblRecordError(S_dev_missingSup, (void *)prec, "calcout:init_record");
        return S_dev_missingSup;
    }

    prpvt = prec->rpvt;
    plink = &prec->inpa;
    pvalue = &prec->a;
    plinkValid = &prec->inav;

    for (i = 0; i <= CALCPERFORM_NARGS; i++, plink++, pvalue++, plinkValid++) {
        /* Don't InitConstantLink the .OUT link */
        if (i < CALCPERFORM_NARGS) {
            recGblInitConstantLink(plink, DBF_DOUBLE, pvalue);
        }

        if (dbLinkIsConstant(plink)) {
            *plinkValid = calcoutINAV_CON;
        }
        else if (dbLinkIsVolatile(plink)) {
            int conn = dbIsLinkConnected(plink);

            if (conn)
                *plinkValid = calcoutINAV_EXT;
            else {
                /* Monitor for connection */
                *plinkValid = calcoutINAV_EXT_NC;
                 prpvt->caLinkStat = CA_LINKS_NOT_OK;
            }
        }
        else {
            /* PV must reside on this ioc */
            *plinkValid = calcoutINAV_LOC;

            if (!dbIsLinkConnected(plink)) {
                errlogPrintf("calcout: %s.INP%c in no-vo disco state\n",
                    prec->name, i+'A');
            }
        }
    }

    prec->clcv = postfix(prec->calc, prec->rpcl, &error_number);
    if (prec->clcv){
        recGblRecordError(S_db_badField, (void *)prec,
                          "calcout: init_record: Illegal CALC field");
        errlogPrintf("%s.CALC: %s in expression \"%s\"\n",
                     prec->name, calcErrorStr(error_number), prec->calc);
    }

    prec->oclv = postfix(prec->ocal, prec->orpc, &error_number);
    if (prec->dopt == calcoutDOPT_Use_OVAL && prec->oclv){
        recGblRecordError(S_db_badField, (void *)prec,
                          "calcout: init_record: Illegal OCAL field");
        errlogPrintf("%s.OCAL: %s in expression \"%s\"\n",
                     prec->name, calcErrorStr(error_number), prec->ocal);
    }

    prpvt = prec->rpvt;
    callbackSetCallback(checkLinksCallback, &prpvt->checkLinkCb);
    callbackSetPriority(0, &prpvt->checkLinkCb);
    callbackSetUser(prec, &prpvt->checkLinkCb);
    prpvt->cbScheduled = 0;

    prec->epvt = eventNameToHandle(prec->oevt);

    if (pcalcoutDSET->common.init_record) pcalcoutDSET->common.init_record(pcommon);
    prec->pval = prec->val;
    prec->mlst = prec->val;
    prec->alst = prec->val;
    prec->lalm = prec->val;
    prec->povl = prec->oval;
    return 0;
}

static long process(struct dbCommon *pcommon)
{
    struct calcoutRecord *prec = (struct calcoutRecord *)pcommon;
    rpvtStruct *prpvt = prec->rpvt;
    int doOutput;
    unsigned char    pact=prec->pact;

    if (!prec->pact) {
        prec->pact = TRUE;
        /* if some links are CA, check connections */
        if (prpvt->caLinkStat != NO_CA_LINKS) {
            checkLinks(prec);
        }
        if (fetch_values(prec) == 0) {
            if (calcPerform(&prec->a, &prec->val, prec->rpcl)) {
                recGblSetSevrMsg(prec, CALC_ALARM, INVALID_ALARM, "calcPerform");
            } else {
                prec->udf = isnan(prec->val);
            }
        }
        checkAlarms(prec);

        if ( !pact ) {
            /* Update the timestamp before writing output values so it
             * will be up to date if any downstream records fetch it via TSEL */
            recGblGetTimeStamp(prec);
        }
        /* check for output link execution */
        switch (prec->oopt) {
        case calcoutOOPT_Every_Time:
            doOutput = 1;
            break;
        case calcoutOOPT_On_Change:
            doOutput = ! (fabs(prec->pval - prec->val) <= prec->mdel);
            break;
        case calcoutOOPT_Transition_To_Zero:
            doOutput = ((prec->pval != 0.0) && (prec->val == 0.0));
            break;
        case calcoutOOPT_Transition_To_Non_zero:
            doOutput = ((prec->pval == 0.0) && (prec->val != 0.0));
            break;
        case calcoutOOPT_When_Zero:
            doOutput = (prec->val == 0.0);
            break;
        case calcoutOOPT_When_Non_zero:
            doOutput = (prec->val != 0.0);
            break;
        default:
            doOutput = 0;
            break;
        }
        prec->pval = prec->val;
        if (doOutput) {
            if (prec->odly > 0.0) {
                prec->dlya = 1;
                db_post_events(prec, &prec->dlya, DBE_VALUE);
                callbackRequestProcessCallbackDelayed(&prpvt->doOutCb,
                        prec->prio, prec, (double)prec->odly);
                return 0;
            } else {
                prec->pact = FALSE;
                execOutput(prec);
                if (prec->pact) return 0;
                prec->pact = TRUE;
            }
        }
    } else { /* pact == TRUE */
        /* Update timestamp again for asynchronous devices */
        recGblGetTimeStamp(prec);

        if (prec->dlya) {
            prec->dlya = 0;
            db_post_events(prec, &prec->dlya, DBE_VALUE);
            /* Make pact FALSE for asynchronous device support*/
            prec->pact = FALSE;
            execOutput(prec);
            if (prec->pact) return 0;
            prec->pact = TRUE;
        } else {/*Device Support is asynchronous*/
            writeValue(prec);
        }
    }
    monitor(prec);
    recGblFwdLink(prec);
    prec->pact = FALSE;
    return 0;
}

static long special(DBADDR *paddr, int after)
{
    calcoutRecord *prec = (calcoutRecord *)paddr->precord;
    rpvtStruct  *prpvt = prec->rpvt;
    short       error_number;
    int         fieldIndex = dbGetFieldIndex(paddr);
    int         lnkIndex;
    DBLINK      *plink;
    double      *pvalue;
    epicsEnum16 *plinkValid;

    if (!after) return 0;
    switch(fieldIndex) {
      case(calcoutRecordCALC):
        prec->clcv = postfix(prec->calc, prec->rpcl, &error_number);
        if (prec->clcv){
            recGblRecordError(S_db_badField, (void *)prec,
                      "calcout: special(): Illegal CALC field");
            errlogPrintf("%s.CALC: %s in expression \"%s\"\n",
                         prec->name, calcErrorStr(error_number), prec->calc);
        }
        db_post_events(prec, &prec->clcv, DBE_VALUE);
        return 0;

      case(calcoutRecordOCAL):
        prec->oclv = postfix(prec->ocal, prec->orpc, &error_number);
        if (prec->dopt == calcoutDOPT_Use_OVAL && prec->oclv){
            recGblRecordError(S_db_badField, (void *)prec,
                    "calcout: special(): Illegal OCAL field");
            errlogPrintf("%s.OCAL: %s in expression \"%s\"\n",
                         prec->name, calcErrorStr(error_number), prec->ocal);
        }
        db_post_events(prec, &prec->oclv, DBE_VALUE);
        return 0;
      case(calcoutRecordINPA):
      case(calcoutRecordINPB):
      case(calcoutRecordINPC):
      case(calcoutRecordINPD):
      case(calcoutRecordINPE):
      case(calcoutRecordINPF):
      case(calcoutRecordINPG):
      case(calcoutRecordINPH):
      case(calcoutRecordINPI):
      case(calcoutRecordINPJ):
      case(calcoutRecordINPK):
      case(calcoutRecordINPL):
      case(calcoutRecordOUT):
        lnkIndex = fieldIndex - calcoutRecordINPA;
        plink   = &prec->inpa + lnkIndex;
        pvalue  = &prec->a    + lnkIndex;
        plinkValid = &prec->inav + lnkIndex;

        if (fieldIndex != calcoutRecordOUT)
            recGblInitConstantLink(plink, DBF_DOUBLE, pvalue);

        if (dbLinkIsConstant(plink)) {
            db_post_events(prec, pvalue, DBE_VALUE);
            *plinkValid = calcoutINAV_CON;
        } else if (dbLinkIsVolatile(plink)) {
            int conn = dbIsLinkConnected(plink);

            if (conn)
                *plinkValid = calcoutINAV_EXT;
            else {
                /* Monitor for connection */
                *plinkValid = calcoutINAV_EXT_NC;
                /* DO_CALLBACK, if not already scheduled */
                if (!prpvt->cbScheduled) {
                    callbackRequestDelayed(&prpvt->checkLinkCb, .5);
                    prpvt->cbScheduled = 1;
                    prpvt->caLinkStat = CA_LINKS_NOT_OK;
                }
            }
        }
        else {
            /* PV must reside on this ioc */
            *plinkValid = calcoutINAV_LOC;

            if (!dbIsLinkConnected(plink)) {
                errlogPrintf("calcout: %s.INP%c in no-vo diso state\n",
                    prec->name, lnkIndex);
            }
        }
        db_post_events(prec, plinkValid, DBE_VALUE);
        return 0;
      case(calcoutRecordOEVT):
        prec->epvt = eventNameToHandle(prec->oevt);
        return 0;
      default:
        recGblDbaddrError(S_db_badChoice, paddr, "calc: special");
        return(S_db_badChoice);
    }
}

#define indexof(field) calcoutRecord##field

static long get_linkNumber(int fieldIndex) {
    if (fieldIndex >= indexof(A) && fieldIndex <= indexof(L))
        return fieldIndex - indexof(A);
    if (fieldIndex >= indexof(LA) && fieldIndex <= indexof(LL))
        return fieldIndex - indexof(LA);
    return -1;
}

static long get_units(DBADDR *paddr, char *units)
{
    calcoutRecord *prec = (calcoutRecord *)paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);
    int linkNumber;

    if(fieldIndex == indexof(ODLY)) {
        strcpy(units, "s");
        return 0;
    }

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
    calcoutRecord *prec = (calcoutRecord *)paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);
    int linkNumber;

    if (fieldIndex == indexof(ODLY)) {
        *pprecision = calcoutODLYprecision;
        return 0;
    }

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
    calcoutRecord *prec = (calcoutRecord *)paddr->precord;
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
        case indexof(ODLY):
            recGblGetGraphicDouble(paddr,pgd);
            pgd->lower_disp_limit = 0.0;
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
    calcoutRecord *prec = (calcoutRecord *)paddr->precord;

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
        case indexof(ODLY):
            pcd->lower_ctrl_limit = 0.0;
            pcd->upper_ctrl_limit = calcoutODLYlimit;
            break;
        default:
            recGblGetControlDouble(paddr,pcd);
    }
    return 0;
}

static long get_alarm_double(DBADDR *paddr, struct dbr_alDouble *pad)
{
    calcoutRecord *prec = (calcoutRecord *)paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);
    int linkNumber;

    if (fieldIndex == indexof(VAL)) {
        pad->upper_alarm_limit = prec->hhsv ? prec->hihi : epicsNAN;
        pad->upper_warning_limit = prec->hsv ? prec->high : epicsNAN;
        pad->lower_warning_limit = prec->lsv ? prec->low : epicsNAN;
        pad->lower_alarm_limit = prec->llsv ? prec->lolo : epicsNAN;
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

static void checkAlarms(calcoutRecord *prec)
{
    double val, hyst, lalm;
    double alev;
    epicsEnum16 asev;

    if (prec->udf) {
        recGblSetSevr(prec, UDF_ALARM, prec->udfs);
        return;
    }

    val = prec->val;
    hyst = prec->hyst;
    lalm = prec->lalm;

    /* alarm condition hihi */
    asev = prec->hhsv;
    alev = prec->hihi;
    if (asev && (val >= alev || ((lalm == alev) && (val >= alev - hyst)))) {
        if (recGblSetSevr(prec, HIHI_ALARM, asev))
            prec->lalm = alev;
        return;
    }

    /* alarm condition lolo */
    asev = prec->llsv;
    alev = prec->lolo;
    if (asev && (val <= alev || ((lalm == alev) && (val <= alev + hyst)))) {
        if (recGblSetSevr(prec, LOLO_ALARM, asev))
            prec->lalm = alev;
        return;
    }

    /* alarm condition high */
    asev = prec->hsv;
    alev = prec->high;
    if (asev && (val >= alev || ((lalm == alev) && (val >= alev - hyst)))) {
        if (recGblSetSevr(prec, HIGH_ALARM, asev))
            prec->lalm = alev;
        return;
    }

    /* alarm condition low */
    asev = prec->lsv;
    alev = prec->low;
    if (asev && (val <= alev || ((lalm == alev) && (val <= alev + hyst)))) {
        if (recGblSetSevr(prec, LOW_ALARM, asev))
            prec->lalm = alev;
        return;
    }

    /* we get here only if val is out of alarm by at least hyst */
    prec->lalm = val;
    return;
}

static void execOutput(calcoutRecord *prec)
{
    /* Determine output data */
    switch(prec->dopt) {
    case calcoutDOPT_Use_VAL:
        prec->oval = prec->val;
        break;
    case calcoutDOPT_Use_OVAL:
        if (calcPerform(&prec->a, &prec->oval, prec->orpc)) {
            recGblSetSevrMsg(prec, CALC_ALARM, INVALID_ALARM, "OCAL calcPerform");
        } else {
            prec->udf = isnan(prec->oval);
        }
        break;
    }
    if (prec->udf){
        recGblSetSevr(prec, UDF_ALARM, prec->udfs);
    }

    /* Check to see what to do if INVALID */
    if (prec->nsev < INVALID_ALARM ) {
        /* Output the value */
        writeValue(prec);
        /* post output event if set */
        if (prec->epvt) postEvent(prec->epvt);
    } else switch (prec->ivoa) {
        case menuIvoaContinue_normally:
            writeValue(prec);
            /* post output event if set */
            if (prec->epvt) postEvent(prec->epvt);
            break;
        case menuIvoaDon_t_drive_outputs:
            break;
        case menuIvoaSet_output_to_IVOV:
            prec->oval = prec->ivov;
            writeValue(prec);
            /* post output event if set */
            if (prec->epvt) postEvent(prec->epvt);
            break;
        default:
            recGblRecordError(S_db_badField, (void *)prec,
                              "calcout:process Illegal IVOA field");
    }
}

static void monitor(calcoutRecord *prec)
{
    unsigned        monitor_mask;
    double          *pnew;
    double          *pprev;
    int             i;

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
    for (i = 0, pnew = &prec->a, pprev = &prec->la; i<CALCPERFORM_NARGS;
         i++, pnew++, pprev++) {
        if ((*pnew != *pprev) || (monitor_mask&DBE_ALARM)) {
            db_post_events(prec, pnew, monitor_mask|DBE_VALUE|DBE_LOG);
            *pprev = *pnew;
        }
    }
    /* Check OVAL field */
    if (prec->povl != prec->oval) {
        db_post_events(prec, &prec->oval, monitor_mask|DBE_VALUE|DBE_LOG);
        prec->povl = prec->oval;
    }
    return;
}

static int fetch_values(calcoutRecord *prec)
{
        DBLINK  *plink; /* structure of the link field  */
        double          *pvalue;
        long            status = 0;
        int             i;

        for (i = 0, plink = &prec->inpa, pvalue = &prec->a; i<CALCPERFORM_NARGS;
            i++, plink++, pvalue++) {
            int newStatus;

            newStatus = dbGetLink(plink, DBR_DOUBLE, pvalue, 0, 0);
            if (!status) status = newStatus;
        }
        return(status);
}

static void checkLinksCallback(epicsCallback *arg)
{

    calcoutRecord *prec;
    rpvtStruct   *prpvt;

    callbackGetUser(prec, arg);
    prpvt = prec->rpvt;

    dbScanLock((dbCommon *)prec);
    prpvt->cbScheduled = 0;
    checkLinks(prec);
    dbScanUnlock((dbCommon *)prec);

}

static void checkLinks(calcoutRecord *prec)
{

    DBLINK *plink;
    rpvtStruct *prpvt = prec->rpvt;
    int i;
    int stat;
    int caLink   = 0;
    int caLinkNc = 0;
    epicsEnum16 *plinkValid;

    if (calcoutRecDebug) printf("checkLinks() for %p\n", prec);

    plink   = &prec->inpa;
    plinkValid = &prec->inav;

    for (i = 0; i<CALCPERFORM_NARGS+1; i++, plink++, plinkValid++) {
        if (dbLinkIsVolatile(plink)) {
            caLink = 1;
            stat = dbIsLinkConnected(plink);
            if (!stat && (*plinkValid == calcoutINAV_EXT_NC)) {
                caLinkNc = 1;
            }
            else if (!stat && (*plinkValid == calcoutINAV_EXT)) {
                *plinkValid = calcoutINAV_EXT_NC;
                db_post_events(prec, plinkValid, DBE_VALUE);
                caLinkNc = 1;
            }
            else if (stat && (*plinkValid == calcoutINAV_EXT_NC)) {
                *plinkValid = calcoutINAV_EXT;
                db_post_events(prec, plinkValid, DBE_VALUE);
            }
        }
    }
    if (caLinkNc)
        prpvt->caLinkStat = CA_LINKS_NOT_OK;
    else if (caLink)
        prpvt->caLinkStat = CA_LINKS_ALL_OK;
    else
        prpvt->caLinkStat = NO_CA_LINKS;

    if (!prpvt->cbScheduled && caLinkNc) {
        /* Schedule another epicsCallback */
        prpvt->cbScheduled = 1;
        callbackRequestDelayed(&prpvt->checkLinkCb, .5);
    }
}

static long writeValue(calcoutRecord *prec)
{
    calcoutdset *pcalcoutDSET = (calcoutdset *)prec->dset;


    if (!pcalcoutDSET || !pcalcoutDSET->write) {
        errlogPrintf("%s DSET write does not exist\n", prec->name);
        recGblSetSevrMsg(prec, SOFT_ALARM, INVALID_ALARM, "DSET write does not exist");
        prec->pact = TRUE;
        return(-1);
    }
    return pcalcoutDSET->write(prec);
}
