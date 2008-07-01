/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
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
#include "dbScan.h"
#include "cantProceed.h"
#include "epicsMath.h"
#include "errMdef.h"
#include "recSup.h"
#include "devSup.h"
#include "recGbl.h"
#include "special.h"
#include "callback.h"
#include "taskwd.h"

#define GEN_SIZE_OFFSET
#include "calcoutRecord.h"
#undef  GEN_SIZE_OFFSET
#include "menuIvoa.h"
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(calcoutRecord *, int);
static long process(calcoutRecord *);
static long special(DBADDR *, int);
#define get_value NULL
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
static long get_units(DBADDR *, char *);
static long get_precision(DBADDR *, long *);
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double(DBADDR *, struct dbr_grDouble *);
static long get_ctrl_double(DBADDR *, struct dbr_ctrlDouble *);
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
    get_ctrl_double,
    get_alarm_double
};
epicsExportAddress(rset, calcoutRSET);

typedef struct calcoutDSET {
    long       number;
    DEVSUPFUN  dev_report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  write;
}calcoutDSET;


/* To provide feedback to the user as to the connection status of the 
 * links (.INxV and .OUTV), the following algorithm has been implemented ...
 *
 * A new PV_LINK [either in init() or special()] is searched for using
 * dbNameToAddr. If local, it is so indicated. If not, a checkLinkCb
 * callback is scheduled to check the connectivity later using 
 * dbCaIsLinkConnected(). Anytime there are unconnected CA_LINKs, another
 * callback is scheduled. Once all connections are established, the CA_LINKs
 * are checked whenever the record processes. 
 *
 */

#define NO_CA_LINKS     0
#define CA_LINKS_ALL_OK 1
#define CA_LINKS_NOT_OK 2

typedef struct rpvtStruct {
    CALLBACK doOutCb;
    CALLBACK checkLinkCb;
    short    cbScheduled;
    short    caLinkStat; /* NO_CA_LINKS, CA_LINKS_ALL_OK, CA_LINKS_NOT_OK */
} rpvtStruct;

static void checkAlarms(calcoutRecord *pcalc);
static void monitor(calcoutRecord *pcalc);
static int fetch_values(calcoutRecord *pcalc);
static void execOutput(calcoutRecord *pcalc);
static void checkLinks(calcoutRecord *pcalc);
static void checkLinksCallback(CALLBACK *arg);
static long writeValue(calcoutRecord *pcalc);

int    calcoutRecDebug;


static long init_record(calcoutRecord *pcalc, int pass)
{
    DBLINK *plink;
    int i;
    double *pvalue;
    epicsEnum16 *plinkValid;
    short error_number;
    calcoutDSET *pcalcoutDSET;

    DBADDR     dbaddr;
    DBADDR     *pAddr = &dbaddr;
    rpvtStruct *prpvt;

    if (pass == 0) {
        pcalc->rpvt = (rpvtStruct *) callocMustSucceed(1, sizeof(rpvtStruct), "calcoutRecord");
        return 0;
    }
    if (!(pcalcoutDSET = (calcoutDSET *)pcalc->dset)) {
        recGblRecordError(S_dev_noDSET, (void *)pcalc, "calcout:init_record");
        return S_dev_noDSET;
    }
    /* must have write defined */
    if ((pcalcoutDSET->number < 5) || (pcalcoutDSET->write ==NULL)) {
        recGblRecordError(S_dev_missingSup, (void *)pcalc, "calcout:init_record");
        return S_dev_missingSup;
    }
    prpvt = pcalc->rpvt;
    plink = &pcalc->inpa;
    pvalue = &pcalc->a;
    plinkValid = &pcalc->inav;
    for (i = 0; i <= CALCPERFORM_NARGS; i++, plink++, pvalue++, plinkValid++) {
        if (plink->type == CONSTANT) {
            /* Don't InitConstantLink the .OUT link */
            if (i < CALCPERFORM_NARGS) {
                recGblInitConstantLink(plink, DBF_DOUBLE, pvalue);
            }
            *plinkValid = calcoutINAV_CON;
        } else if (!dbNameToAddr(plink->value.pv_link.pvname, pAddr)) {
            /* PV resides on this ioc */
            *plinkValid = calcoutINAV_LOC;
        } else {
            /* pv is not on this ioc. Callback later for connection stat */
            *plinkValid = calcoutINAV_EXT_NC;
             prpvt->caLinkStat = CA_LINKS_NOT_OK;
        }
    }

    pcalc->clcv = postfix(pcalc->calc, pcalc->rpcl, &error_number);
    if (pcalc->clcv){
        recGblRecordError(S_db_badField, (void *)pcalc,
                          "calcout: init_record: Illegal CALC field");
        errlogPrintf("%s.CALC: %s in expression \"%s\"\n",
                     pcalc->name, calcErrorStr(error_number), pcalc->calc);
    }

    pcalc->oclv = postfix(pcalc->ocal, pcalc->orpc, &error_number);
    if (pcalc->dopt == calcoutDOPT_Use_OVAL && pcalc->oclv){
        recGblRecordError(S_db_badField, (void *)pcalc,
                          "calcout: init_record: Illegal OCAL field");
        errlogPrintf("%s.OCAL: %s in expression \"%s\"\n",
                     pcalc->name, calcErrorStr(error_number), pcalc->ocal);
    }

    prpvt = pcalc->rpvt;
    callbackSetCallback(checkLinksCallback, &prpvt->checkLinkCb);
    callbackSetPriority(0, &prpvt->checkLinkCb);
    callbackSetUser(pcalc, &prpvt->checkLinkCb);
    prpvt->cbScheduled = 0;

    if (pcalcoutDSET->init_record) pcalcoutDSET->init_record(pcalc);
    return 0;
}

static long process(calcoutRecord *pcalc)
{
    rpvtStruct *prpvt = pcalc->rpvt;
    int doOutput = 0;

    if (!pcalc->pact) {
        pcalc->pact = TRUE;
        /* if some links are CA, check connections */
        if (prpvt->caLinkStat != NO_CA_LINKS) {
            checkLinks(pcalc);
        }
        if (fetch_values(pcalc) == 0) {
            if (calcPerform(&pcalc->a, &pcalc->val, pcalc->rpcl)) {
                recGblSetSevr(pcalc, CALC_ALARM, INVALID_ALARM);
            } else {
                pcalc->udf = isnan(pcalc->val);
            }
        }
        checkAlarms(pcalc);
        /* check for output link execution */
        switch (pcalc->oopt) {
        case calcoutOOPT_Every_Time:
            doOutput = 1;
            break;
        case calcoutOOPT_On_Change:
            if (fabs(pcalc->pval - pcalc->val) > pcalc->mdel) doOutput = 1;
            break;
        case calcoutOOPT_Transition_To_Zero:
            if ((pcalc->pval != 0.0) && (pcalc->val == 0.0)) doOutput = 1;
            break;
        case calcoutOOPT_Transition_To_Non_zero:
            if ((pcalc->pval == 0.0) && (pcalc->val != 0.0)) doOutput = 1;
            break;
        case calcoutOOPT_When_Zero:
            if (pcalc->val == 0.0) doOutput = 1;
            break;
        case calcoutOOPT_When_Non_zero:
            if (pcalc->val != 0.0) doOutput = 1;
            break;
        default:
            break;
        }
        pcalc->pval = pcalc->val;
        if (doOutput) {
            if (pcalc->odly > 0.0) {
                pcalc->dlya = 1;
                db_post_events(pcalc, &pcalc->dlya, DBE_VALUE);
                callbackRequestProcessCallbackDelayed(&prpvt->doOutCb,
                        pcalc->prio, pcalc, (double)pcalc->odly);
                return 0;
            } else {
                pcalc->pact = FALSE;
                execOutput(pcalc);
                if (pcalc->pact) return 0;
                pcalc->pact = TRUE;
            }
        }
    } else { /* pact == TRUE */
        if (pcalc->dlya) {
            pcalc->dlya = 0;
            db_post_events(pcalc, &pcalc->dlya, DBE_VALUE);
            /* Make pact FALSE for asynchronous device support*/
            pcalc->pact = FALSE;
            execOutput(pcalc);
            if (pcalc->pact) return 0;
            pcalc->pact = TRUE;
        } else {/*Device Support is asynchronous*/
            writeValue(pcalc);
        }
    }
    recGblGetTimeStamp(pcalc);
    monitor(pcalc);
    recGblFwdLink(pcalc);
    pcalc->pact = FALSE;
    return 0;
}

static long special(DBADDR *paddr, int after)
{
    calcoutRecord *pcalc = (calcoutRecord *)paddr->precord;
    rpvtStruct  *prpvt = pcalc->rpvt;
    DBADDR      dbaddr;
    DBADDR      *pAddr = &dbaddr;
    short       error_number;
    int         fieldIndex = dbGetFieldIndex(paddr);
    int         lnkIndex;
    DBLINK      *plink;
    double      *pvalue;
    epicsEnum16 *plinkValid;

    if (!after) return 0;
    switch(fieldIndex) {
      case(calcoutRecordCALC):
        pcalc->clcv = postfix(pcalc->calc, pcalc->rpcl, &error_number);
        if (pcalc->clcv){
            recGblRecordError(S_db_badField, (void *)pcalc,
                      "calcout: special(): Illegal CALC field");
            errlogPrintf("%s.CALC: %s in expression \"%s\"\n",
                         pcalc->name, calcErrorStr(error_number), pcalc->calc);
        }
        db_post_events(pcalc, &pcalc->clcv, DBE_VALUE);
        return 0;

      case(calcoutRecordOCAL):
        pcalc->oclv = postfix(pcalc->ocal, pcalc->orpc, &error_number);
        if (pcalc->dopt == calcoutDOPT_Use_OVAL && pcalc->oclv){
            recGblRecordError(S_db_badField, (void *)pcalc,
                    "calcout: special(): Illegal OCAL field");
            errlogPrintf("%s.OCAL: %s in expression \"%s\"\n",
                         pcalc->name, calcErrorStr(error_number), pcalc->ocal);
        }
        db_post_events(pcalc, &pcalc->oclv, DBE_VALUE);
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
        plink   = &pcalc->inpa + lnkIndex;
        pvalue  = &pcalc->a    + lnkIndex;
        plinkValid = &pcalc->inav + lnkIndex;
        if (plink->type == CONSTANT) {
            if (fieldIndex != calcoutRecordOUT) {
                recGblInitConstantLink(plink, DBF_DOUBLE, pvalue);
                db_post_events(pcalc, pvalue, DBE_VALUE);
            }
            *plinkValid = calcoutINAV_CON;
        } else if (!dbNameToAddr(plink->value.pv_link.pvname, pAddr)) {
            /* if the PV resides on this ioc */
            *plinkValid = calcoutINAV_LOC;
        } else {
            /* pv is not on this ioc. Callback later for connection stat */
            *plinkValid = calcoutINAV_EXT_NC;
            /* DO_CALLBACK, if not already scheduled */
            if (!prpvt->cbScheduled) {
                callbackRequestDelayed(&prpvt->checkLinkCb, .5);
                prpvt->cbScheduled = 1;
                prpvt->caLinkStat = CA_LINKS_NOT_OK;
            }
        }
        db_post_events(pcalc, plinkValid, DBE_VALUE);
        return 0;
      default:
        recGblDbaddrError(S_db_badChoice, paddr, "calc: special");
        return(S_db_badChoice);
    }
}

static long get_units(DBADDR *paddr, char *units)
{
    calcoutRecord *pcalc = (calcoutRecord *)paddr->precord;

    strncpy(units, pcalc->egu, DB_UNITS_SIZE);
    return 0;
}

static long get_precision(DBADDR *paddr, long *pprecision)
{
    calcoutRecord *pcalc = (calcoutRecord *)paddr->precord;

    if (paddr->pfield == (void *)&pcalc->val) {
        *pprecision = pcalc->prec;
    } else {
        recGblGetPrec(paddr, pprecision);
    }
    return 0;
}

static long get_graphic_double(DBADDR *paddr, struct dbr_grDouble *pgd)
{
    calcoutRecord *pcalc = (calcoutRecord *)paddr->precord;

    if (paddr->pfield == (void *)&pcalc->val ||
        paddr->pfield == (void *)&pcalc->hihi ||
        paddr->pfield == (void *)&pcalc->high ||
        paddr->pfield == (void *)&pcalc->low ||
        paddr->pfield == (void *)&pcalc->lolo) {
        pgd->upper_disp_limit = pcalc->hopr;
        pgd->lower_disp_limit = pcalc->lopr;
        return 0;
    }

    if (paddr->pfield >= (void *)&pcalc->a &&
        paddr->pfield <= (void *)&pcalc->l) {
        pgd->upper_disp_limit = pcalc->hopr;
        pgd->lower_disp_limit = pcalc->lopr;
        return 0;
    }
    if (paddr->pfield >= (void *)&pcalc->la &&
        paddr->pfield <= (void *)&pcalc->ll) {
        pgd->upper_disp_limit = pcalc->hopr;
        pgd->lower_disp_limit = pcalc->lopr;
        return 0;
    }
    recGblGetGraphicDouble(paddr, pgd);
    return 0;
}

static long get_ctrl_double(DBADDR *paddr, struct dbr_ctrlDouble *pcd)
{
    calcoutRecord *pcalc = (calcoutRecord *)paddr->precord;

    if (paddr->pfield == (void *)&pcalc->val ||
        paddr->pfield == (void *)&pcalc->hihi ||
        paddr->pfield == (void *)&pcalc->high ||
        paddr->pfield == (void *)&pcalc->low ||
        paddr->pfield == (void *)&pcalc->lolo) {
        pcd->upper_ctrl_limit = pcalc->hopr;
        pcd->lower_ctrl_limit = pcalc->lopr;
        return 0;
    }

    if (paddr->pfield >= (void *)&pcalc->a &&
        paddr->pfield <= (void *)&pcalc->l) {
        pcd->upper_ctrl_limit = pcalc->hopr;
        pcd->lower_ctrl_limit = pcalc->lopr;
        return 0;
    }
    if (paddr->pfield >= (void *)&pcalc->la &&
        paddr->pfield <= (void *)&pcalc->ll) {
        pcd->upper_ctrl_limit = pcalc->hopr;
        pcd->lower_ctrl_limit = pcalc->lopr;
        return 0;
    }
    recGblGetControlDouble(paddr, pcd);
    return 0;
}

static long get_alarm_double(DBADDR *paddr, struct dbr_alDouble *pad)
{
    calcoutRecord *pcalc = (calcoutRecord *)paddr->precord;

    if (paddr->pfield == (void *)&pcalc->val) {
        pad->upper_alarm_limit = pcalc->hihi;
        pad->upper_warning_limit = pcalc->high;
        pad->lower_warning_limit = pcalc->low;
        pad->lower_alarm_limit = pcalc->lolo;
    } else {
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
        recGblSetSevr(prec, UDF_ALARM, INVALID_ALARM);
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

static void execOutput(calcoutRecord *pcalc)
{
    long status;

    /* Determine output data */
    switch(pcalc->dopt) {
    case calcoutDOPT_Use_VAL:
        pcalc->oval = pcalc->val;
        break;
    case calcoutDOPT_Use_OVAL:
        if (calcPerform(&pcalc->a, &pcalc->oval, pcalc->orpc)) {
            recGblSetSevr(pcalc, CALC_ALARM, INVALID_ALARM);
        } else {
            pcalc->udf = isnan(pcalc->oval);
        }
        break;
    }
    if (pcalc->udf){
        recGblSetSevr(pcalc, UDF_ALARM, INVALID_ALARM);
    }

    /* Check to see what to do if INVALID */
    if (pcalc->nsev < INVALID_ALARM ) {
        /* Output the value */
        status = writeValue(pcalc);
        /* post event if output event != 0 */
        if (pcalc->oevt > 0) {
            post_event((int)pcalc->oevt);
        }
    } else switch (pcalc->ivoa) {
        case menuIvoaContinue_normally:
            status = writeValue(pcalc);
            /* post event if output event != 0 */
            if (pcalc->oevt > 0) {
                post_event((int)pcalc->oevt);
            }
            break;
        case menuIvoaDon_t_drive_outputs:
            break;
        case menuIvoaSet_output_to_IVOV:
            pcalc->oval = pcalc->ivov;
            status = writeValue(pcalc);
            /* post event if output event != 0 */
            if (pcalc->oevt > 0) {
                post_event((int)pcalc->oevt);
            }
            break;
        default:
            status = -1;
            recGblRecordError(S_db_badField, (void *)pcalc,
                              "calcout:process Illegal IVOA field");
    }
}

static void monitor(calcoutRecord *pcalc)
{
        unsigned        monitor_mask;
        double          delta;
        double          *pnew;
        double          *pprev;
        int             i;

        monitor_mask = recGblResetAlarms(pcalc);
        /* check for value change */
        delta = pcalc->mlst - pcalc->val;
        if (delta<0.0) delta = -delta;
        if (delta > pcalc->mdel) {
                /* post events for value change */
                monitor_mask |= DBE_VALUE;
                /* update last value monitored */
                pcalc->mlst = pcalc->val;
        }
        /* check for archive change */
        delta = pcalc->alst - pcalc->val;
        if (delta<0.0) delta = -delta;
        if (delta > pcalc->adel) {
                /* post events on value field for archive change */
                monitor_mask |= DBE_LOG;
                /* update last archive value monitored */
                pcalc->alst = pcalc->val;
        }

        /* send out monitors connected to the value field */
        if (monitor_mask){
                db_post_events(pcalc, &pcalc->val, monitor_mask);
        }
        /* check all input fields for changes*/
        for (i = 0, pnew = &pcalc->a, pprev = &pcalc->la; i<CALCPERFORM_NARGS;
            i++, pnew++, pprev++) {
             if ((*pnew != *pprev) || (monitor_mask&DBE_ALARM)) {
                  db_post_events(pcalc, pnew, monitor_mask|DBE_VALUE|DBE_LOG);
                  *pprev = *pnew;
             }
        }
        /* Check OVAL field */
        if (pcalc->povl != pcalc->oval) {
            db_post_events(pcalc, &pcalc->oval, monitor_mask|DBE_VALUE|DBE_LOG);
            pcalc->povl = pcalc->oval;
        }
        return;
}

static int fetch_values(calcoutRecord *pcalc)
{
        DBLINK  *plink; /* structure of the link field  */
        double          *pvalue;
        long            status = 0;
        int             i;

        for (i = 0, plink = &pcalc->inpa, pvalue = &pcalc->a; i<CALCPERFORM_NARGS;
            i++, plink++, pvalue++) {
            int newStatus;

            newStatus = dbGetLink(plink, DBR_DOUBLE, pvalue, 0, 0);
            if (!status) status = newStatus;
        }
        return(status);
}

static void checkLinksCallback(CALLBACK *arg)
{

    calcoutRecord *pcalc;
    rpvtStruct   *prpvt;

    callbackGetUser(pcalc, arg);
    prpvt = pcalc->rpvt;

    dbScanLock((dbCommon *)pcalc);
    prpvt->cbScheduled = 0;
    checkLinks(pcalc);
    dbScanUnlock((dbCommon *)pcalc);

}

static void checkLinks(calcoutRecord *pcalc)
{

    DBLINK *plink;
    rpvtStruct *prpvt = pcalc->rpvt;
    int i;
    int stat;
    int caLink   = 0;
    int caLinkNc = 0;
    epicsEnum16 *plinkValid;

    if (calcoutRecDebug) printf("checkLinks() for %p\n", pcalc);

    plink   = &pcalc->inpa;
    plinkValid = &pcalc->inav;

    for (i = 0; i<CALCPERFORM_NARGS+1; i++, plink++, plinkValid++) {
        if (plink->type == CA_LINK) {
            caLink = 1;
            stat = dbCaIsLinkConnected(plink);
            if (!stat && (*plinkValid == calcoutINAV_EXT_NC)) {
                caLinkNc = 1;
            }
            else if (!stat && (*plinkValid == calcoutINAV_EXT)) {
                *plinkValid = calcoutINAV_EXT_NC;
                db_post_events(pcalc, plinkValid, DBE_VALUE);
                caLinkNc = 1;
            }
            else if (stat && (*plinkValid == calcoutINAV_EXT_NC)) {
                *plinkValid = calcoutINAV_EXT;
                db_post_events(pcalc, plinkValid, DBE_VALUE);
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
        /* Schedule another CALLBACK */
        prpvt->cbScheduled = 1;
        callbackRequestDelayed(&prpvt->checkLinkCb, .5);
    }
}

static long writeValue(calcoutRecord *pcalc)
{
    calcoutDSET *pcalcoutDSET = (calcoutDSET *)pcalc->dset;


    if (!pcalcoutDSET || !pcalcoutDSET->write) {
        errlogPrintf("%s DSET write does not exist\n", pcalc->name);
        recGblSetSevr(pcalc, SOFT_ALARM, INVALID_ALARM);
        pcalc->pact = TRUE;
        return(-1);
    }
    return pcalcoutDSET->write(pcalc);
}
