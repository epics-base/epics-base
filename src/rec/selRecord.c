/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* $Id$ */

/* selRecord.c - Record Support Routines for Select records */
/*
 *      Original Author: Bob Dalesio
 *      Date:            6-2-89
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
#include "dbAccess.h"
#include "dbEvent.h"
#include "dbFldTypes.h"
#include "errMdef.h"
#include "recSup.h"
#include "recGbl.h"
#define GEN_SIZE_OFFSET
#include "selRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(selRecord *, int);
static long process(selRecord *);
#define special NULL
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
static long get_control_double(DBADDR *, struct dbr_ctrlDouble *);
static long get_alarm_double(DBADDR *, struct dbr_alDouble *);

rset selRSET={
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
epicsExportAddress(rset,selRSET);

#define	SEL_MAX	12

static void checkAlarms(selRecord *);
static void do_sel(selRecord *);
static int fetch_values(selRecord *);
static void monitor(selRecord *);

/* This needed to prevent the MS optimizer from barfing */
static double divide(double num, double den) { return num / den; }

static double NaN, Inf;


static long init_record(selRecord *psel, int pass)
{
    struct link *plink;
    int i;
    double *pvalue;

    if (pass==0) return(0);

    NaN = divide(0.0, 0.0);
    Inf = divide(1.0, 0.0);

    /* get seln initial value if nvl is a constant*/
    if (psel->nvl.type == CONSTANT ) {
	recGblInitConstantLink(&psel->nvl,DBF_USHORT,&psel->seln);
    }

    plink = &psel->inpa;
    pvalue = &psel->a;
    for(i=0; i<SEL_MAX; i++, plink++, pvalue++) {
	*pvalue = NaN;
	if (plink->type==CONSTANT) {
	    recGblInitConstantLink(plink,DBF_DOUBLE,pvalue);
	}
    }
    return(0);
}

static long process(selRecord *psel)
{
    psel->pact = TRUE;
    if ( RTN_SUCCESS(fetch_values(psel)) ) {
	do_sel(psel);
    }

    recGblGetTimeStamp(psel);
    /* check for alarms */
    checkAlarms(psel);


    /* check event list */
    monitor(psel);

    /* process the forward scan link record */
    recGblFwdLink(psel);

    psel->pact=FALSE;
    return(0);
}


static long get_units(DBADDR *paddr, char *units)
{
    selRecord	*psel=(selRecord *)paddr->precord;

    strncpy(units,psel->egu,DB_UNITS_SIZE);
    return(0);
}

static long get_precision(DBADDR *paddr, long *precision)
{
    selRecord	*psel=(selRecord *)paddr->precord;
    double *pvalue,*plvalue;
    int i;

    *precision = psel->prec;
    if(paddr->pfield==(void *)&psel->val){
        return(0);
    }
    pvalue = &psel->a;
    plvalue = &psel->la;
    for(i=0; i<SEL_MAX; i++, pvalue++, plvalue++) {
	if(paddr->pfield==(void *)&pvalue
	|| paddr->pfield==(void *)&plvalue){
	    return(0);
	}
    }
    recGblGetPrec(paddr,precision);
    return(0);
}


static long get_graphic_double(DBADDR *paddr, struct dbr_grDouble *pgd)
{
    selRecord	*psel=(selRecord *)paddr->precord;

    if(paddr->pfield==(void *)&psel->val
    || paddr->pfield==(void *)&psel->hihi
    || paddr->pfield==(void *)&psel->high
    || paddr->pfield==(void *)&psel->low
    || paddr->pfield==(void *)&psel->lolo){
	pgd->upper_disp_limit = psel->hopr;
	pgd->lower_disp_limit = psel->lopr;
	return(0);
    }

    if(paddr->pfield>=(void *)&psel->a && paddr->pfield<=(void *)&psel->l){
	pgd->upper_disp_limit = psel->hopr;
	pgd->lower_disp_limit = psel->lopr;
	return(0);
    }
    if(paddr->pfield>=(void *)&psel->la && paddr->pfield<=(void *)&psel->ll){
	pgd->upper_disp_limit = psel->hopr;
	pgd->lower_disp_limit = psel->lopr;
	return(0);
    }
    recGblGetGraphicDouble(paddr,pgd);
    return(0);
}

static long get_control_double(struct dbAddr *paddr, struct dbr_ctrlDouble *pcd)
{
    selRecord	*psel=(selRecord *)paddr->precord;

    if(paddr->pfield==(void *)&psel->val
    || paddr->pfield==(void *)&psel->hihi
    || paddr->pfield==(void *)&psel->high
    || paddr->pfield==(void *)&psel->low
    || paddr->pfield==(void *)&psel->lolo){
	pcd->upper_ctrl_limit = psel->hopr;
	pcd->lower_ctrl_limit = psel->lopr;
	return(0);
    }

    if(paddr->pfield>=(void *)&psel->a && paddr->pfield<=(void *)&psel->l){
	pcd->upper_ctrl_limit = psel->hopr;
	pcd->lower_ctrl_limit = psel->lopr;
	return(0);
    }
    if(paddr->pfield>=(void *)&psel->la && paddr->pfield<=(void *)&psel->ll){
	pcd->upper_ctrl_limit = psel->hopr;
	pcd->lower_ctrl_limit = psel->lopr;
	return(0);
    }
    recGblGetControlDouble(paddr,pcd);
    return(0);
}

static long get_alarm_double(DBADDR *paddr, struct dbr_alDouble *pad)
{
    selRecord	*psel=(selRecord *)paddr->precord;

    if(paddr->pfield==(void *)&psel->val ){
	pad->upper_alarm_limit = psel->hihi;
	pad->upper_warning_limit = psel->high;
	pad->lower_warning_limit = psel->low;
	pad->lower_alarm_limit = psel->lolo;
    } else recGblGetAlarmDouble(paddr,pad);
    return(0);
}

static void checkAlarms(selRecord *prec)
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

static void monitor(selRecord *psel)
{
    unsigned short	monitor_mask;
    double		delta;
    double		*pnew;
    double		*pprev;
    int			i;

    monitor_mask = recGblResetAlarms(psel);
    /* check for value change */
    delta = psel->mlst - psel->val;
    if(delta<0.0) delta = -delta;
    if (delta > psel->mdel) {
        /* post events for value change */
        monitor_mask |= DBE_VALUE;
        /* update last value monitored */
        psel->mlst = psel->val;
    }
    /* check for archive change */
    delta = psel->alst - psel->val;
    if(delta<0.0) delta = -delta;
    if (delta > psel->adel) {
        /* post events on value field for archive change */
        monitor_mask |= DBE_LOG;
        /* update last archive value monitored */
        psel->alst = psel->val;
    }

    /* send out monitors connected to the value field */
    if (monitor_mask)
        db_post_events(psel, &psel->val, monitor_mask);

    monitor_mask |= DBE_VALUE|DBE_LOG;

    /* trigger monitors of the SELN field */
    if (psel->nlst != psel->seln) {
	psel->nlst = psel->seln;
	db_post_events(psel, &psel->seln, monitor_mask);
    }

    /* check all input fields for changes, even if VAL hasn't changed */
    for(i=0, pnew=&psel->a, pprev=&psel->la; i<SEL_MAX; i++, pnew++, pprev++) {
	if(*pnew != *pprev) {
	    db_post_events(psel, pnew, monitor_mask);
	    *pprev = *pnew;
	}
    }
    return;
}

static void do_sel(selRecord *psel)
{
    double		*pvalue;
    double		order[SEL_MAX];
    unsigned short	count;
    unsigned short	i,j;
    double		val;

    /* selection mechanism */
    pvalue = &psel->a;
    switch (psel->selm){
    case (selSELM_Specified):
	if (psel->seln >= SEL_MAX) {
	    recGblSetSevr(psel,SOFT_ALARM,INVALID_ALARM);
	    return;
	}
	val = *(pvalue+psel->seln);
	break;
    case (selSELM_High_Signal):
	val = -Inf;
	for (i = 0; i < SEL_MAX; i++,pvalue++){
	    if (!isnan(*pvalue) && val < *pvalue) {
		val = *pvalue;
		psel->seln = i;
	    }
	}
	break;
    case (selSELM_Low_Signal):
	val = Inf;
	for (i = 0; i < SEL_MAX; i++,pvalue++){
	    if (!isnan(*pvalue) && val > *pvalue) {
		val = *pvalue;
		psel->seln = i;
	    }
	}
	break;
    case (selSELM_Median_Signal):
	count = 0;
	order[0] = NaN;
	for (i = 0; i < SEL_MAX; i++,pvalue++){
	    if (!isnan(*pvalue)){
		/* Insertion sort */
		j = count;
		while ((j > 0) && (order[j-1] > *pvalue)){
		    order[j] = order[j-1];
		    j--;
		}
		order[j] = *pvalue;
		count++;
	    }
	}
	psel->seln = count;
	val = order[count / 2];
	break;
    default:
	recGblSetSevr(psel,CALC_ALARM,INVALID_ALARM);
	return;
    }
    if (!isinf(val)){
	psel->val=val;
	psel->udf=isnan(psel->val);
    }  else {
	recGblSetSevr(psel,UDF_ALARM,MAJOR_ALARM);
	/* If UDF is TRUE this alarm will be overwritten by checkAlarms*/
    }
    return;
}

/*
 * FETCH_VALUES
 *
 * fetch the values for the variables from which to select
 */
static int fetch_values(selRecord *psel)
{
    struct link	*plink;
    double	*pvalue;
    int		i;
    long	status;

    plink = &psel->inpa;
    pvalue = &psel->a;
    /* If mechanism is selSELM_Specified, only get the selected input*/
    if(psel->selm == selSELM_Specified) {
	/* fetch the select index */
	status=dbGetLink(&(psel->nvl),DBR_USHORT,&(psel->seln),0,0);
	if (!RTN_SUCCESS(status) || (psel->seln >= SEL_MAX))
	    return(status);

	plink += psel->seln;
	pvalue += psel->seln;

	status=dbGetLink(plink,DBR_DOUBLE, pvalue,0,0);
	return(status);
    }
    /* fetch all inputs*/
    for(i=0; i<SEL_MAX; i++, plink++, pvalue++) {
	status=dbGetLink(plink,DBR_DOUBLE, pvalue,0,0);
    }
    return(status);
}
