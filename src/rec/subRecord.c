/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* recSub.c */
/* base/src/rec  $Id$ */

/* recSub.c - Record Support Routines for Subroutine records */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            01-25-90
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include "epicsMath.h"
#include "registryFunction.h"
#include "alarm.h"
#include "callback.h"
#include "cantProceed.h"
#include "dbAccess.h"
#include "epicsPrint.h"
#include "dbEvent.h"
#include "dbFldTypes.h"
#include "errMdef.h"
#include "recSup.h"
#include "recGbl.h"
#define GEN_SIZE_OFFSET
#include "subRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
#define special NULL
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

rset subRSET={
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
epicsExportAddress(rset,subRSET);

static void checkAlarms();
static long do_sub();
static long fetch_values();
static void monitor();

#define INP_ARG_MAX 12
typedef long (*SUBFUNCPTR)();

static long init_record(psub,pass)
    struct subRecord	*psub;
    int pass;
{
    SUBFUNCPTR	psubroutine;
    long	status = 0;
    struct link *plink;
    int i;
    double *pvalue;

    if (pass==0) return(0);

    plink = &psub->inpa;
    pvalue = &psub->a;
    for(i=0; i<INP_ARG_MAX; i++, plink++, pvalue++) {
        if (plink->type==CONSTANT) {
	    recGblInitConstantLink(plink,DBF_DOUBLE,pvalue);
        }
    }

    if(strlen(psub->inam)!=0) {
        /* convert the initialization subroutine name  */
        psubroutine = (SUBFUNCPTR)registryFunctionFind(psub->inam);
        if(psubroutine==0) {
	    recGblRecordError(S_db_BadSub,(void *)psub,"recSub(init_record)");
	    return(S_db_BadSub);
        }
        /* invoke the initialization subroutine */
        status = (*psubroutine)(psub,process);
    }

    if(strlen(psub->snam)==0) {
	epicsPrintf("%s snam not specified\n",psub->name);
	psub->pact = TRUE;
	return(0);
    }
    psub->sadr = (void *)registryFunctionFind(psub->snam);
    if(psub->sadr==0) {
	recGblRecordError(S_db_BadSub,(void *)psub,"recSub(init_record)");
	return(S_db_BadSub);
    }
    return(0);
}

static long process(psub)
	struct subRecord *psub;
{
	long		 status=0;
	unsigned char	pact=psub->pact;

        if(!psub->pact || !psub->sadr){
		psub->pact = TRUE;
		status = fetch_values(psub);
		psub->pact = FALSE;
	}
        if(status==0) status = do_sub(psub);
	/* check if device support set pact */
        if ( !pact && psub->pact ) return(0);
	/*previously we had different rules. Lets support old subs*/
        psub->pact = TRUE;
	if(status==1) return(0);
	recGblGetTimeStamp(psub);
        /* check for alarms */
        checkAlarms(psub);
        /* check event list */
        monitor(psub);
        /* process the forward scan link record */
        recGblFwdLink(psub);
        psub->pact = FALSE;
        return(0);
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct subRecord	*psub=(struct subRecord *)paddr->precord;

    strncpy(units,psub->egu,DB_UNITS_SIZE);
    return(0);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct subRecord	*psub=(struct subRecord *)paddr->precord;

    *precision = psub->prec;
    if(paddr->pfield==(void *)&psub->val) return(0);
    recGblGetPrec(paddr,precision);
    return(0);
}


static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble	*pgd;
{
    struct subRecord	*psub=(struct subRecord *)paddr->precord;

    if(paddr->pfield==(void *)&psub->val
    || paddr->pfield==(void *)&psub->hihi
    || paddr->pfield==(void *)&psub->high
    || paddr->pfield==(void *)&psub->low
    || paddr->pfield==(void *)&psub->lolo){
        pgd->upper_disp_limit = psub->hopr;
        pgd->lower_disp_limit = psub->lopr;
        return(0);
    }

    if(paddr->pfield>=(void *)&psub->a && paddr->pfield<=(void *)&psub->l){
        pgd->upper_disp_limit = psub->hopr;
        pgd->lower_disp_limit = psub->lopr;
        return(0);
    }
    if(paddr->pfield>=(void *)&psub->la && paddr->pfield<=(void *)&psub->ll){
        pgd->upper_disp_limit = psub->hopr;
        pgd->lower_disp_limit = psub->lopr;
        return(0);
    }
    recGblGetGraphicDouble(paddr,pgd);
    return(0);
}

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct subRecord	*psub=(struct subRecord *)paddr->precord;

    if(paddr->pfield==(void *)&psub->val
    || paddr->pfield==(void *)&psub->hihi
    || paddr->pfield==(void *)&psub->high
    || paddr->pfield==(void *)&psub->low
    || paddr->pfield==(void *)&psub->lolo){
        pcd->upper_ctrl_limit = psub->hopr;
        pcd->lower_ctrl_limit = psub->lopr;
       return(0);
    } 

    if(paddr->pfield>=(void *)&psub->a && paddr->pfield<=(void *)&psub->l){
        pcd->upper_ctrl_limit = psub->hopr;
        pcd->lower_ctrl_limit = psub->lopr;
        return(0);
    }
    if(paddr->pfield>=(void *)&psub->la && paddr->pfield<=(void *)&psub->ll){
        pcd->upper_ctrl_limit = psub->hopr;
        pcd->lower_ctrl_limit = psub->lopr;
        return(0);
    }
    recGblGetControlDouble(paddr,pcd);
    return(0);
}

static long get_alarm_double(paddr,pad)
    struct dbAddr *paddr;
    struct dbr_alDouble	*pad;
{
    struct subRecord	*psub=(struct subRecord *)paddr->precord;

    if(paddr->pfield==(void *)&psub->val){
         pad->upper_alarm_limit = psub->hihi;
         pad->upper_warning_limit = psub->high;
         pad->lower_warning_limit = psub->low;
         pad->lower_alarm_limit = psub->lolo;
    } else recGblGetAlarmDouble(paddr,pad);
    return(0);
}

static void checkAlarms(psub)
    struct subRecord	*psub;
{
	double		val;
	double		hyst, lalm, hihi, high, low, lolo;
	unsigned short	hhsv, llsv, hsv, lsv;

	if (psub->udf) {
 		recGblSetSevr(psub,UDF_ALARM,INVALID_ALARM);
		return;
	}
	hihi = psub->hihi; lolo = psub->lolo; high = psub->high; low = psub->low;
	hhsv = psub->hhsv; llsv = psub->llsv; hsv = psub->hsv; lsv = psub->lsv;
	val = psub->val; hyst = psub->hyst; lalm = psub->lalm;

	/* alarm condition hihi */
	if (hhsv && (val >= hihi || ((lalm==hihi) && (val >= hihi-hyst)))){
	        if (recGblSetSevr(psub,HIHI_ALARM,psub->hhsv)) psub->lalm = hihi;
		return;
	}

	/* alarm condition lolo */
	if (llsv && (val <= lolo || ((lalm==lolo) && (val <= lolo+hyst)))){
	        if (recGblSetSevr(psub,LOLO_ALARM,psub->llsv)) psub->lalm = lolo;
		return;
	}

	/* alarm condition high */
	if (hsv && (val >= high || ((lalm==high) && (val >= high-hyst)))){
	        if (recGblSetSevr(psub,HIGH_ALARM,psub->hsv)) psub->lalm = high;
		return;
	}

	/* alarm condition low */
	if (lsv && (val <= low || ((lalm==low) && (val <= low+hyst)))){
	        if (recGblSetSevr(psub,LOW_ALARM,psub->lsv)) psub->lalm = low;
		return;
	}

	/* we get here only if val is out of alarm by at least hyst */
	psub->lalm = val;
	return;
}

static void monitor(psub)
    struct subRecord	*psub;
{
	unsigned short	monitor_mask;
	double		delta;
	double           *pnew;
	double           *pprev;
	int             i;

        /* get previous stat and sevr  and new stat and sevr*/
        monitor_mask = recGblResetAlarms(psub);
        /* check for value change */
        delta = psub->mlst - psub->val;
        if(delta<0.0) delta = -delta;
        if (delta > psub->mdel) {
                /* post events for value change */
                monitor_mask |= DBE_VALUE;
                /* update last value monitored */
                psub->mlst = psub->val;
        }
        /* check for archive change */
        delta = psub->alst - psub->val;
        if(delta<0.0) delta = -delta;
        if (delta > psub->adel) {
                /* post events on value field for archive change */
                monitor_mask |= DBE_LOG;
                /* update last archive value monitored */
                psub->alst = psub->val;
        }
        /* send out monitors connected to the value field */
        if (monitor_mask){
                db_post_events(psub,&psub->val,monitor_mask);
        }
	/* check all input fields for changes*/
	for(i=0, pnew=&psub->a, pprev=&psub->la; i<INP_ARG_MAX; i++, pnew++, pprev++) {
		if(*pnew != *pprev) {
			db_post_events(psub,pnew,monitor_mask|DBE_VALUE|DBE_LOG);
			*pprev = *pnew;
		}
	}
        return;
}

static long fetch_values(psub)
struct subRecord *psub;
{
        struct link     *plink; /* structure of the link field  */
        double           *pvalue;
        int             i;
	long		status;

        for(i=0, plink=&psub->inpa, pvalue=&psub->a; i<INP_ARG_MAX; i++, plink++, pvalue++) {
		status=dbGetLink(plink,DBR_DOUBLE, pvalue,0,0);
		if (!RTN_SUCCESS(status)) return(-1);
        }
        return(0);
}

static long do_sub(psub)
struct subRecord *psub;  /* pointer to subroutine record  */
{
	long	status;
	SUBFUNCPTR	psubroutine;


	/* call the subroutine */
	psubroutine = (SUBFUNCPTR)(psub->sadr);
	if(psubroutine==NULL) {
               	recGblSetSevr(psub,BAD_SUB_ALARM,INVALID_ALARM);
		return(0);
	}
	status = (*psubroutine)(psub);
	if(status < 0){
               	recGblSetSevr(psub,SOFT_ALARM,psub->brsv);
	} else psub->udf = isnan(psub->val);
	return(status);
}
