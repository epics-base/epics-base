/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* recCalc.c */
/* base/src/rec  $Id$ */

/* recCalc.c - Record Support Routines for Calculation records */
/*
 *      Original Author: Julie Sander and Bob Dalesio
 *      Current  Author: Marty Kraimer
 *      Date:            7-27-87
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include "postfix.h"
#include "alarm.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "dbFldTypes.h"
#include "errMdef.h"
#include "recSup.h"
#include "recGbl.h"
#include "special.h"
#define GEN_SIZE_OFFSET
#include "calcRecord.h"
#undef  GEN_SIZE_OFFSET

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
static long special();
#define get_value NULL
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
static long get_units();
static long get_precision();
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double();
static long get_control_double();
static long get_alarm_double();

struct rset calcRSET={
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
	get_alarm_double };

static void checkAlarms();
static void monitor();
static int fetch_values();


#define ARG_MAX 12

static long init_record(pcalc,pass)
    struct calcRecord	*pcalc;
    int pass;
{
    long status;
    struct link *plink;
    int i;
    double *pvalue;
    short error_number;

    if (pass==0) return(0);

    plink = &pcalc->inpa;
    pvalue = &pcalc->a;
    for(i=0; i<ARG_MAX; i++, plink++, pvalue++) {
        if (plink->type == CONSTANT) {
	    recGblInitConstantLink(plink,DBF_DOUBLE,pvalue);
        }
    }
    status=postfix(pcalc->calc,pcalc->rpcl,&error_number);
    if(status) recGblRecordError(S_db_badField,(void *)pcalc,
	           "calc: init_record: Illegal CALC field");
    return(0);
}

static long process(pcalc)
	struct calcRecord     *pcalc;
{

	pcalc->pact = TRUE;
	if(fetch_values(pcalc)==0) {
		if(calcPerform(&pcalc->a,&pcalc->val,pcalc->rpcl)) {
			recGblSetSevr(pcalc,CALC_ALARM,INVALID_ALARM);
		} else pcalc->udf = FALSE;
	}
	recGblGetTimeStamp(pcalc);
	/* check for alarms */
	checkAlarms(pcalc);
	/* check event list */
	monitor(pcalc);
	/* process the forward scan link record */
	recGblFwdLink(pcalc);
	pcalc->pact = FALSE;
	return(0);
}

static long special(paddr,after)
    struct dbAddr *paddr;
    int	   	  after;
{
    long	status;
    calcRecord  *pcalc = (struct calcRecord *)(paddr->precord);
    int         special_type = paddr->special;
    short       error_number;

    if(!after) return(0);
    switch(special_type) {
    case(SPC_CALC):
	status=postfix(pcalc->calc,pcalc->rpcl,&error_number);
        if(status) recGblRecordError(S_db_badField,(void *)pcalc,
			"calc: special: Illegal CALC field");
	return(0);
    default:
	recGblDbaddrError(S_db_badChoice,paddr,"calc: special");
	return(S_db_badChoice);
    }
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct calcRecord	*pcalc=(struct calcRecord *)paddr->precord;

    strncpy(units,pcalc->egu,DB_UNITS_SIZE);
    return(0);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct calcRecord	*pcalc=(struct calcRecord *)paddr->precord;

    *precision = pcalc->prec;
    if(paddr->pfield == (void *)&pcalc->val) return(0);
    recGblGetPrec(paddr,precision);
    return(0);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble	*pgd;
{
    struct calcRecord	*pcalc=(struct calcRecord *)paddr->precord;

    if(paddr->pfield==(void *)&pcalc->val
    || paddr->pfield==(void *)&pcalc->hihi
    || paddr->pfield==(void *)&pcalc->high
    || paddr->pfield==(void *)&pcalc->low
    || paddr->pfield==(void *)&pcalc->lolo){
        pgd->upper_disp_limit = pcalc->hopr;
        pgd->lower_disp_limit = pcalc->lopr;
       return(0);
    } 

    if(paddr->pfield>=(void *)&pcalc->a && paddr->pfield<=(void *)&pcalc->l){
        pgd->upper_disp_limit = pcalc->hopr;
        pgd->lower_disp_limit = pcalc->lopr;
        return(0);
    }
    if(paddr->pfield>=(void *)&pcalc->la && paddr->pfield<=(void *)&pcalc->ll){
        pgd->upper_disp_limit = pcalc->hopr;
        pgd->lower_disp_limit = pcalc->lopr;
        return(0);
    }
    recGblGetGraphicDouble(paddr,pgd);
    return(0);
}

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct calcRecord	*pcalc=(struct calcRecord *)paddr->precord;

    if(paddr->pfield==(void *)&pcalc->val
    || paddr->pfield==(void *)&pcalc->hihi
    || paddr->pfield==(void *)&pcalc->high
    || paddr->pfield==(void *)&pcalc->low
    || paddr->pfield==(void *)&pcalc->lolo){
        pcd->upper_ctrl_limit = pcalc->hopr;
        pcd->lower_ctrl_limit = pcalc->lopr;
       return(0);
    } 

    if(paddr->pfield>=(void *)&pcalc->a && paddr->pfield<=(void *)&pcalc->l){
        pcd->upper_ctrl_limit = pcalc->hopr;
        pcd->lower_ctrl_limit = pcalc->lopr;
        return(0);
    }
    if(paddr->pfield>=(void *)&pcalc->la && paddr->pfield<=(void *)&pcalc->ll){
        pcd->upper_ctrl_limit = pcalc->hopr;
        pcd->lower_ctrl_limit = pcalc->lopr;
        return(0);
    }
    recGblGetControlDouble(paddr,pcd);
    return(0);
}
static long get_alarm_double(paddr,pad)
    struct dbAddr *paddr;
    struct dbr_alDouble	*pad;
{
    struct calcRecord	*pcalc=(struct calcRecord *)paddr->precord;

    if(paddr->pfield==(void *)&pcalc->val){
         pad->upper_alarm_limit = pcalc->hihi;
         pad->upper_warning_limit = pcalc->high;
         pad->lower_warning_limit = pcalc->low;
         pad->lower_alarm_limit = pcalc->lolo;
    } else recGblGetAlarmDouble(paddr,pad);
    return(0);
}


static void checkAlarms(pcalc)
    struct calcRecord	*pcalc;
{
	double		val;
	double		hyst, lalm, hihi, high, low, lolo;
	unsigned short	hhsv, llsv, hsv, lsv;

	if(pcalc->udf == TRUE ){
 		recGblSetSevr(pcalc,UDF_ALARM,INVALID_ALARM);
		return;
	}
	hihi = pcalc->hihi; lolo = pcalc->lolo; high = pcalc->high; low = pcalc->low;
	hhsv = pcalc->hhsv; llsv = pcalc->llsv; hsv = pcalc->hsv; lsv = pcalc->lsv;
	val = pcalc->val; hyst = pcalc->hyst; lalm = pcalc->lalm;

	/* alarm condition hihi */
	if (hhsv && (val >= hihi || ((lalm==hihi) && (val >= hihi-hyst)))){
	        if (recGblSetSevr(pcalc,HIHI_ALARM,pcalc->hhsv)) pcalc->lalm = hihi;
		return;
	}

	/* alarm condition lolo */
	if (llsv && (val <= lolo || ((lalm==lolo) && (val <= lolo+hyst)))){
	        if (recGblSetSevr(pcalc,LOLO_ALARM,pcalc->llsv)) pcalc->lalm = lolo;
		return;
	}

	/* alarm condition high */
	if (hsv && (val >= high || ((lalm==high) && (val >= high-hyst)))){
	        if (recGblSetSevr(pcalc,HIGH_ALARM,pcalc->hsv)) pcalc->lalm = high;
		return;
	}

	/* alarm condition low */
	if (lsv && (val <= low || ((lalm==low) && (val <= low+hyst)))){
	        if (recGblSetSevr(pcalc,LOW_ALARM,pcalc->lsv)) pcalc->lalm = low;
		return;
	}

	/* we get here only if val is out of alarm by at least hyst */
	pcalc->lalm = val;
	return;
}

static void monitor(pcalc)
    struct calcRecord	*pcalc;
{
	unsigned short	monitor_mask;
	double		delta;
	double		*pnew;
	double		*pprev;
	int		i;

        monitor_mask = recGblResetAlarms(pcalc);
        /* check for value change */
        delta = pcalc->mlst - pcalc->val;
        if(delta<0.0) delta = -delta;
        if (delta > pcalc->mdel) {
                /* post events for value change */
                monitor_mask |= DBE_VALUE;
                /* update last value monitored */
                pcalc->mlst = pcalc->val;
        }
        /* check for archive change */
        delta = pcalc->alst - pcalc->val;
        if(delta<0.0) delta = -delta;
        if (delta > pcalc->adel) {
                /* post events on value field for archive change */
                monitor_mask |= DBE_LOG;
                /* update last archive value monitored */
                pcalc->alst = pcalc->val;
        }

        /* send out monitors connected to the value field */
        if (monitor_mask){
                db_post_events(pcalc,&pcalc->val,monitor_mask);
        }
        /* check all input fields for changes*/
        for(i=0, pnew=&pcalc->a, pprev=&pcalc->la; i<ARG_MAX; i++, pnew++, pprev++) {
             if((*pnew != *pprev) || (monitor_mask&DBE_ALARM)) {
                  db_post_events(pcalc,pnew,monitor_mask|DBE_VALUE|DBE_LOG);
                  *pprev = *pnew;
		     }
		}
        return;
}

static int fetch_values(pcalc)
struct calcRecord *pcalc;
{
	struct link	*plink;	/* structure of the link field  */
	double		*pvalue;
	long		status = 0;
	int		i;

	for(i=0, plink=&pcalc->inpa, pvalue=&pcalc->a; i<ARG_MAX; i++, plink++, pvalue++) {

            status = dbGetLink(plink,DBR_DOUBLE, pvalue,0,0);

	    if (!RTN_SUCCESS(status)) return(status);
	}
	return(0);
}
