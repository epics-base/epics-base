/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Author:  Janet Anderson
 * Date:    9/23/91
 */
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include "epicsMath.h"
#include "alarm.h"
#include "callback.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "dbFldTypes.h"
#include "devSup.h"
#include "errMdef.h"
#include "recSup.h"
#include "recGbl.h"
#include "special.h"
#include "menuYesNo.h"
#include "menuIvoa.h"
#include "menuOmsl.h"

#define GEN_SIZE_OFFSET
#include "longoutRecord.h"
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
#define get_precision NULL
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double(DBADDR *, struct dbr_grDouble *);
static long get_control_double(DBADDR *, struct dbr_ctrlDouble *);
static long get_alarm_double(DBADDR *, struct dbr_alDouble *);

rset longoutRSET={
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
epicsExportAddress(rset,longoutRSET);

#define DONT_EXEC_OUTPUT    0
#define EXEC_OUTPUT         1

static void checkAlarms(longoutRecord *prec);
static void monitor(longoutRecord *prec);
static long writeValue(longoutRecord *prec);
static void convert(longoutRecord *prec, epicsInt32 value);
static long conditional_write(longoutRecord *prec);

static long init_record(struct dbCommon *pcommon, int pass)
{
    struct longoutRecord *prec = (struct longoutRecord *)pcommon;
    longoutdset *pdset = (longoutdset *) prec->dset;

    if (pass == 0) return 0;

    recGblInitSimm(pcommon, &prec->sscn, &prec->oldsimm, &prec->simm, &prec->siml);

    if (!pdset) {
        recGblRecordError(S_dev_noDSET, prec, "longout: init_record");
        return S_dev_noDSET;
    }

    /* must have  write_longout functions defined */
    if ((pdset->common.number < 5) || (pdset->write_longout == NULL)) {
        recGblRecordError(S_dev_missingSup, prec, "longout: init_record");
        return S_dev_missingSup;
    }

    if (recGblInitConstantLink(&prec->dol, DBF_LONG, &prec->val))
        prec->udf=FALSE;

    if (pdset->common.init_record) {
        long status = pdset->common.init_record(pcommon);

        if (status)
            return status;
    }

    prec->mlst = prec->val;
    prec->alst = prec->val;
    prec->lalm = prec->val;
    prec->pval = prec->val;
    prec->outpvt = EXEC_OUTPUT;
    
    return 0;
}

static long process(struct dbCommon *pcommon)
{
    struct longoutRecord *prec = (struct longoutRecord *)pcommon;
    longoutdset  *pdset = (longoutdset *)(prec->dset);
    long                status=0;
    epicsInt32          value;
    unsigned char       pact=prec->pact;

    if( (pdset==NULL) || (pdset->write_longout==NULL) ) {
        prec->pact=TRUE;
        recGblRecordError(S_dev_missingSup,(void *)prec,"write_longout");
        return(S_dev_missingSup);
    }
    if (!prec->pact) {
        if (!dbLinkIsConstant(&prec->dol) &&
                prec->omsl == menuOmslclosed_loop) {
            status = dbGetLink(&prec->dol, DBR_LONG, &value, 0, 0);
            if (!dbLinkIsConstant(&prec->dol) && !status)
                prec->udf=FALSE;
        }
        else {
            value = prec->val;
        }
        if (!status) convert(prec,value);

        /* Update the timestamp before writing output values so it
         * will be up to date if any downstream records fetch it via TSEL */
        recGblGetTimeStampSimm(prec, prec->simm, NULL);
    }

    /* check for alarms */
    checkAlarms(prec);

    if (prec->nsev < INVALID_ALARM )
            status=writeValue(prec); /* write the new value */
    else {
        switch (prec->ivoa) {
            case (menuIvoaContinue_normally) :
                status=writeValue(prec); /* write the new value */
                break;
            case (menuIvoaDon_t_drive_outputs) :
                break;
            case (menuIvoaSet_output_to_IVOV) :
                if(prec->pact == FALSE){
                        prec->val=prec->ivov;
                }
                status=writeValue(prec); /* write the new value */
                break;
            default :
                status=-1;
                recGblRecordError(S_db_badField,(void *)prec,
                        "longout:process Illegal IVOA field");
        }
    }

    /* check if device support set pact */
    if ( !pact && prec->pact ) return(0);
    prec->pact = TRUE;

    if ( pact ) {
        /* Update timestamp again for asynchronous devices */
        recGblGetTimeStampSimm(prec, prec->simm, NULL);
    }

    /* check event list */
    monitor(prec);

    /* process the forward scan link record */
    recGblFwdLink(prec);

    prec->pact=FALSE;
    return(status);
}

static long special(DBADDR *paddr, int after)
{
    longoutRecord *prec = (longoutRecord *)(paddr->precord);
    int    special_type = paddr->special;

    switch(special_type) {
    case(SPC_MOD):
        if (dbGetFieldIndex(paddr) == longoutRecordSIMM) {
            if (!after)
                recGblSaveSimm(prec->sscn, &prec->oldsimm, prec->simm);
            else
                recGblCheckSimm((dbCommon *)prec, &prec->sscn, prec->oldsimm, prec->simm);
            return(0);
        }

        /* Detect an output link re-direction (change) */
        if (dbGetFieldIndex(paddr) == longoutRecordOUT) {
            if ((after) && (prec->ooch == menuYesNoYES))
                prec->outpvt = EXEC_OUTPUT;
            return(0);
        }

    default:
        recGblDbaddrError(S_db_badChoice, paddr, "longout: special");
        return(S_db_badChoice);
    }
}

#define indexof(field) longoutRecord##field

static long get_units(DBADDR *paddr,char *units)
{
    longoutRecord *prec=(longoutRecord *)paddr->precord;

    if(paddr->pfldDes->field_type == DBF_LONG) {
        strncpy(units,prec->egu,DB_UNITS_SIZE);
    }
    return(0);
}

static long get_graphic_double(DBADDR *paddr,struct dbr_grDouble *pgd)
{
    longoutRecord *prec=(longoutRecord *)paddr->precord;

    switch (dbGetFieldIndex(paddr)) {
        case indexof(VAL):
        case indexof(HIHI):
        case indexof(HIGH):
        case indexof(LOW):
        case indexof(LOLO):
        case indexof(LALM):
        case indexof(ALST):
        case indexof(MLST):
            pgd->upper_disp_limit = prec->hopr;
            pgd->lower_disp_limit = prec->lopr;
            break;
        default:
            recGblGetGraphicDouble(paddr,pgd);
    }
    return(0);
}

static long get_control_double(DBADDR *paddr,struct dbr_ctrlDouble *pcd)
{
    longoutRecord *prec=(longoutRecord *)paddr->precord;

    switch (dbGetFieldIndex(paddr)) {
        case indexof(VAL):
        case indexof(HIHI):
        case indexof(HIGH):
        case indexof(LOW):
        case indexof(LOLO):
        case indexof(LALM):
        case indexof(ALST):
        case indexof(MLST):
            /* do not change pre drvh/drvl behavior */
            if(prec->drvh > prec->drvl) {
                pcd->upper_ctrl_limit = prec->drvh;
                pcd->lower_ctrl_limit = prec->drvl;
            } else {
                pcd->upper_ctrl_limit = prec->hopr;
                pcd->lower_ctrl_limit = prec->lopr;
            }
            break;
        default:
            recGblGetControlDouble(paddr,pcd);
    }
    return(0);
}

static long get_alarm_double(DBADDR *paddr,struct dbr_alDouble *pad)
{
    longoutRecord    *prec=(longoutRecord *)paddr->precord;

    if (dbGetFieldIndex(paddr) == indexof(VAL)) {
         pad->upper_alarm_limit = prec->hhsv ? prec->hihi : epicsNAN;
         pad->upper_warning_limit = prec->hsv ? prec->high : epicsNAN;
         pad->lower_warning_limit = prec->lsv ? prec->low : epicsNAN;
         pad->lower_alarm_limit = prec->llsv ? prec->lolo : epicsNAN;
    }
    else
        recGblGetAlarmDouble(paddr,pad);
    return 0;
}

static void checkAlarms(longoutRecord *prec)
{
    epicsInt32 val, hyst, lalm;
    epicsInt32 alev;
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

/* DELTA calculates the absolute difference between its arguments
 * expressed as an unsigned 32-bit integer */
#define DELTA(last, val) \
    ((epicsUInt32) ((last) > (val) ? (last) - (val) : (val) - (last)))

static void monitor(longoutRecord *prec)
{
    unsigned short monitor_mask = recGblResetAlarms(prec);

    if (prec->mdel < 0 ||
        DELTA(prec->mlst, prec->val) > (epicsUInt32) prec->mdel) {
        /* post events for value change */
        monitor_mask |= DBE_VALUE;
        /* update last value monitored */
        prec->mlst = prec->val;
    }

    if (prec->adel < 0 ||
        DELTA(prec->alst, prec->val) > (epicsUInt32) prec->adel) {
        /* post events for archive value change */
        monitor_mask |= DBE_LOG;
        /* update last archive value monitored */
        prec->alst = prec->val;
    }

    /* send out monitors connected to the value field */
    if (monitor_mask)
        db_post_events(prec, &prec->val, monitor_mask);
}

static long writeValue(longoutRecord *prec)
{
    long status = 0;

    if (!prec->pact) {
        status = recGblGetSimm((dbCommon *)prec, &prec->sscn, &prec->oldsimm, &prec->simm, &prec->siml);
        if (status) return status;
    }

    switch (prec->simm) {
    case menuYesNoNO:
        status = conditional_write(prec);
        break;

    case menuYesNoYES: {
        recGblSetSevr(prec, SIMM_ALARM, prec->sims);
        if (prec->pact || (prec->sdly < 0.)) {
            status = dbPutLink(&prec->siol, DBR_LONG, &prec->val, 1);
            prec->pact = FALSE;
        } else { /* !prec->pact && delay >= 0. */
            epicsCallback *pvt = prec->simpvt;
            if (!pvt) {
                pvt = calloc(1, sizeof(epicsCallback)); /* very lazy allocation of callback structure */
                prec->simpvt = pvt;
            }
            if (pvt) callbackRequestProcessCallbackDelayed(pvt, prec->prio, prec, prec->sdly);
            prec->pact = TRUE;
        }
        break;
    }

    default:
        recGblSetSevr(prec, SOFT_ALARM, INVALID_ALARM);
        status = -1;
    }

    return status;
}

static void convert(longoutRecord *prec, epicsInt32 value)
{
    /* check drive limits */
    if(prec->drvh > prec->drvl) {
        if (value > prec->drvh) value = prec->drvh;
        else if (value < prec->drvl) value = prec->drvl;
    }
    prec->val = value;
}

/* Evaluate OOPT field to perform the write operation */
static long conditional_write(longoutRecord *prec)
{
    struct longoutdset *pdset = (struct longoutdset *) prec->dset;
    long status = 0;
    int doDevSupWrite = 0;

    switch (prec->oopt) 
    {
    case longoutOOPT_On_Change:
        /* Forces a write op if a change in the OUT field is detected OR is first process */
        if (prec->outpvt == EXEC_OUTPUT) {
            doDevSupWrite = 1;
        } else {
            /* Only write if value is different from the previous one */ 
            doDevSupWrite = (prec->val != prec->pval);
        }
        break;

    case longoutOOPT_Every_Time:
        doDevSupWrite = 1;
        break;

    case longoutOOPT_When_Zero:
        doDevSupWrite = (prec->val == 0);
        break;

    case longoutOOPT_When_Non_zero:
        doDevSupWrite = (prec->val != 0);
        break;

    case longoutOOPT_Transition_To_Zero:
        doDevSupWrite = ((prec->val == 0)&&(prec->pval != 0));
        break;

    case longoutOOPT_Transition_To_Non_zero:
        doDevSupWrite = ((prec->val != 0)&&(prec->pval == 0));      
        break;

    default:
        break;
    }

    if (doDevSupWrite)
        status = pdset->write_longout(prec);

    prec->pval = prec->val;
    prec->outpvt = DONT_EXEC_OUTPUT; /* reset status */
    return status;
}
