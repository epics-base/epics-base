/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* $Id$ */

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

/* Create RSET - Record Support Entry Table */

#define report NULL
#define initialize NULL
static long init_record(calcRecord *pcalc, int pass);
static long process(calcRecord *pcalc);
static long special(DBADDR *paddr, int after);
#define get_value NULL
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
static long get_units(DBADDR *paddr, char *units);
static long get_precision(DBADDR *paddr, long *precision);
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double(DBADDR *paddr, struct dbr_grDouble *pgd);
static long get_ctrl_double(DBADDR *paddr, struct dbr_ctrlDouble *pcd);
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
	get_ctrl_double,
	get_alarm_double
};
epicsExportAddress(rset, calcRSET);

static void checkAlarms(calcRecord *pcalc);
static void monitor(calcRecord *pcalc);
static int fetch_values(calcRecord *pcalc);


static long init_record(calcRecord *pcalc, int pass)
{
    struct link *plink;
    double *pvalue;
    int i;
    short error_number;

    if (pass==0) return(0);

    plink = &pcalc->inpa;
    pvalue = &pcalc->a;
    for (i = 0; i < CALCPERFORM_NARGS; i++, plink++, pvalue++) {
	if (plink->type == CONSTANT) {
	    recGblInitConstantLink(plink, DBF_DOUBLE, pvalue);
	}
    }
    if (postfix(pcalc->calc, pcalc->rpcl, &error_number)) {
	recGblRecordError(S_db_badField, (void *)pcalc,
			  "calc: init_record: Illegal CALC field");
	errlogPrintf("%s.CALC: %s in expression \"%s\"\n",
		     pcalc->name, calcErrorStr(error_number), pcalc->calc);
    }
    return 0;
}

static long process(calcRecord *pcalc)
{
    pcalc->pact = TRUE;
    if (fetch_values(pcalc)==0) {
	if (calcPerform(&pcalc->a, &pcalc->val, pcalc->rpcl)) {
	    recGblSetSevr(pcalc, CALC_ALARM, INVALID_ALARM);
	} else pcalc->udf = isnan(pcalc->val);
    }
    recGblGetTimeStamp(pcalc);
    /* check for alarms */
    checkAlarms(pcalc);
    /* check event list */
    monitor(pcalc);
    /* process the forward scan link record */
    recGblFwdLink(pcalc);
    pcalc->pact = FALSE;
    return 0;
}

static long special(DBADDR *paddr, int after)
{
    calcRecord *pcalc = (calcRecord *)paddr->precord;
    short error_number;

    if (!after) return 0;
    if (paddr->special == SPC_CALC) {
	if (postfix(pcalc->calc, pcalc->rpcl, &error_number)) {
	    recGblRecordError(S_db_badField, (void *)pcalc,
			      "calc: Illegal CALC field");
	    errlogPrintf("%s.CALC: %s in expression \"%s\"\n",
			 pcalc->name, calcErrorStr(error_number), pcalc->calc);
	    return S_db_badField;
	}
	return 0;
    }
    recGblDbaddrError(S_db_badChoice, paddr, "calc::special - bad special value!");
    return S_db_badChoice;
}

static long get_units(DBADDR *paddr, char *units)
{
    calcRecord *pcalc = (calcRecord *)paddr->precord;

    strncpy(units, pcalc->egu, DB_UNITS_SIZE);
    return 0;
}

static long get_precision(DBADDR *paddr, long *pprecision)
{
    calcRecord *pcalc = (calcRecord *)paddr->precord;

    if (paddr->pfield == (void *)&pcalc->val) {
	*pprecision = pcalc->prec;
    } else {
	recGblGetPrec(paddr, pprecision);
    }
    return 0;
}

static long get_graphic_double(DBADDR *paddr, struct dbr_grDouble *pgd)
{
    calcRecord *pcalc = (calcRecord *)paddr->precord;

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
    calcRecord *pcalc = (calcRecord *)paddr->precord;

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
    calcRecord *pcalc = (calcRecord *)paddr->precord;

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

static void checkAlarms(calcRecord *prec)
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

static void monitor(calcRecord *pcalc)
{
    unsigned short monitor_mask;
    double delta, *pnew, *pprev;
    int i;

    monitor_mask = recGblResetAlarms(pcalc);
    /* check for value change */
    delta = pcalc->mlst - pcalc->val;
    if (delta < 0.0) delta = -delta;
    if (delta > pcalc->mdel) {
	/* post events for value change */
	monitor_mask |= DBE_VALUE;
	/* update last value monitored */
	pcalc->mlst = pcalc->val;
    }
    /* check for archive change */
    delta = pcalc->alst - pcalc->val;
    if (delta < 0.0) delta = -delta;
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
    pnew = &pcalc->a;
    pprev = &pcalc->la;
    for (i = 0; i < CALCPERFORM_NARGS; i++, pnew++, pprev++) {
	if (*pnew != *pprev ||
	    monitor_mask & DBE_ALARM) {
	    db_post_events(pcalc, pnew, monitor_mask | DBE_VALUE | DBE_LOG);
	    *pprev = *pnew;
	}
    }
    return;
}

static int fetch_values(calcRecord *pcalc)
{
    struct link *plink;
    double *pvalue;
    long status = 0;
    int i;

    plink = &pcalc->inpa;
    pvalue = &pcalc->a;
    for(i = 0; i < CALCPERFORM_NARGS; i++, plink++, pvalue++) {
	int newStatus;

	newStatus = dbGetLink(plink, DBR_DOUBLE, pvalue, 0, 0);
	if (status == 0) status = newStatus;
    }
    return status;
}
