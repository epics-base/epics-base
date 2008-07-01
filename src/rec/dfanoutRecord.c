/*************************************************************************\
* Copyright (c) 2002 Southeastern Universities Research Association, as
*     Operator of Thomas Jefferson National Accelerator Facility.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* $Id$ */

/* recDfanout.c - Record Support Routines for Dfanout records */
/*
 * Original Author: 	Matt Bickley   (Sometime in 1994)
 *
 * Modification Log:
 * -----------------
 * .01  1994        mhb     Started with longout record to make the data fanout
 * .02  May 10, 96  jt	    Bug Fix
 * .03  11SEP2000   mrk     LONG=>DOUBLE, add SELL,SELN,SELM
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
#include "devSup.h"
#include "errMdef.h"
#include "recSup.h"
#include "recGbl.h"
#include "special.h"
#include "menuOmsl.h"
#define GEN_SIZE_OFFSET
#include "dfanoutRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(dfanoutRecord *, int);
static long process(dfanoutRecord *);
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
static long get_graphic_double(DBADDR *,struct dbr_grDouble *);
static long get_control_double(DBADDR *,struct dbr_ctrlDouble *);
static long get_alarm_double(DBADDR *,struct dbr_alDouble *);

rset dfanoutRSET={
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
epicsExportAddress(rset,dfanoutRSET);


static void checkAlarms(dfanoutRecord *);
static void monitor(dfanoutRecord *);
static void push_values(dfanoutRecord *);

#define OUT_ARG_MAX 8


static long init_record(dfanoutRecord *pdfanout, int pass)
{
    if (pass==0) return(0);

    recGblInitConstantLink(&pdfanout->sell,DBF_USHORT,&pdfanout->seln);
    /* get the initial value dol is a constant*/
    if(recGblInitConstantLink(&pdfanout->dol,DBF_DOUBLE,&pdfanout->val))
	    pdfanout->udf = isnan(pdfanout->val);
    return(0);
}

static long process(dfanoutRecord *pdfanout)
{
    long status=0;

    if (!pdfanout->pact
    && (pdfanout->dol.type != CONSTANT)
    && (pdfanout->omsl == menuOmslclosed_loop)){
	status = dbGetLink(&(pdfanout->dol),DBR_DOUBLE,&(pdfanout->val),0,0);
	if(pdfanout->dol.type!=CONSTANT && RTN_SUCCESS(status))
            pdfanout->udf = isnan(pdfanout->val);
    }
    pdfanout->pact = TRUE;
    recGblGetTimeStamp(pdfanout);
    /* Push out the data to all the forward links */
    dbGetLink(&(pdfanout->sell),DBR_USHORT,&(pdfanout->seln),0,0);
    checkAlarms(pdfanout);
    push_values(pdfanout);
    monitor(pdfanout);
    recGblFwdLink(pdfanout);
    pdfanout->pact=FALSE;
    return(status);
}

static long get_units(DBADDR *paddr,char *units)
{
    dfanoutRecord *pdfanout=(dfanoutRecord *)paddr->precord;

    strncpy(units,pdfanout->egu,DB_UNITS_SIZE);
    return(0);
}

static long get_precision(DBADDR *paddr,long *precision)
{
    dfanoutRecord *pdfanout=(dfanoutRecord *)paddr->precord;
    int   fieldIndex = dbGetFieldIndex(paddr);

    if(fieldIndex == dfanoutRecordVAL
    || fieldIndex == dfanoutRecordHIHI
    || fieldIndex == dfanoutRecordHIGH
    || fieldIndex == dfanoutRecordLOW
    || fieldIndex == dfanoutRecordLOLO
    || fieldIndex == dfanoutRecordHOPR
    || fieldIndex == dfanoutRecordLOPR) {
        *precision = pdfanout->prec;
    } else {
        recGblGetPrec(paddr,precision);
    }
    return(0);
}

static long get_graphic_double(DBADDR *paddr,struct dbr_grDouble	*pgd)
{
    dfanoutRecord *pdfanout=(dfanoutRecord *)paddr->precord;
    int   fieldIndex = dbGetFieldIndex(paddr);

    if(fieldIndex == dfanoutRecordVAL
    || fieldIndex == dfanoutRecordHIHI
    || fieldIndex == dfanoutRecordHIGH
    || fieldIndex == dfanoutRecordLOW
    || fieldIndex == dfanoutRecordLOLO
    || fieldIndex == dfanoutRecordHOPR
    || fieldIndex == dfanoutRecordLOPR) {
        pgd->upper_disp_limit = pdfanout->hopr;
        pgd->lower_disp_limit = pdfanout->lopr;
    } else recGblGetGraphicDouble(paddr,pgd);
    return(0);
}

static long get_control_double(DBADDR *paddr,struct dbr_ctrlDouble *pcd)
{
    dfanoutRecord *pdfanout=(dfanoutRecord *)paddr->precord;
    int   fieldIndex = dbGetFieldIndex(paddr);

    if(fieldIndex == dfanoutRecordVAL
    || fieldIndex == dfanoutRecordHIHI
    || fieldIndex == dfanoutRecordHIGH
    || fieldIndex == dfanoutRecordLOW
    || fieldIndex == dfanoutRecordLOLO) {
        pcd->upper_ctrl_limit = pdfanout->hopr;
        pcd->lower_ctrl_limit = pdfanout->lopr;
    } else recGblGetControlDouble(paddr,pcd);
    return(0);
}
static long get_alarm_double(DBADDR *paddr,struct dbr_alDouble *pad)
{
    dfanoutRecord *pdfanout=(dfanoutRecord *)paddr->precord;
    int   fieldIndex = dbGetFieldIndex(paddr);

    
    if(fieldIndex == dfanoutRecordVAL) {
         pad->upper_alarm_limit = pdfanout->hihi;
         pad->upper_warning_limit = pdfanout->high;
         pad->lower_warning_limit = pdfanout->low;
         pad->lower_alarm_limit = pdfanout->lolo;
    } else recGblGetAlarmDouble(paddr,pad);
    return(0);
}

static void checkAlarms(dfanoutRecord *prec)
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

static void monitor(dfanoutRecord *pdfanout)
{
	unsigned short	monitor_mask;

	double		delta;

        monitor_mask = recGblResetAlarms(pdfanout);
        /* check for value change */
        delta = pdfanout->mlst - pdfanout->val;
        if(delta<0) delta = -delta;
        if (delta > pdfanout->mdel) {
                /* post events for value change */
                monitor_mask |= DBE_VALUE;
                /* update last value monitored */
                pdfanout->mlst = pdfanout->val;
        }
        /* check for archive change */
        delta = pdfanout->alst - pdfanout->val;
        if(delta<0) delta = -delta;
        if (delta > pdfanout->adel) {
                /* post events on value field for archive change */
                monitor_mask |= DBE_LOG;
                /* update last archive value monitored */
                pdfanout->alst = pdfanout->val;
        }

        /* send out monitors connected to the value field */
        if (monitor_mask){
                db_post_events(pdfanout,&pdfanout->val,monitor_mask);
	}
	return;
}

static void push_values(dfanoutRecord *pdfanout)
{
    struct link     *plink; /* structure of the link field  */
    int             i;
    long            status;
    unsigned short  state;

    switch (pdfanout->selm){
    case (dfanoutSELM_All):
        for(i=0, plink=&(pdfanout->outa); i<OUT_ARG_MAX; i++, plink++) {
                status=dbPutLink(plink,DBR_DOUBLE,&(pdfanout->val),1);
                if(status) recGblSetSevr(pdfanout,LINK_ALARM,MAJOR_ALARM);
        }
        break;
    case (dfanoutSELM_Specified):
        if(pdfanout->seln>OUT_ARG_MAX) {
            recGblSetSevr(pdfanout,SOFT_ALARM,INVALID_ALARM);
            break;
        }
        if(pdfanout->seln==0) break;
        plink=&(pdfanout->outa);
        plink += (pdfanout->seln -1);
        status=dbPutLink(plink,DBR_DOUBLE,&(pdfanout->val),1);
        if(status) recGblSetSevr(pdfanout,LINK_ALARM,MAJOR_ALARM);
        break;
    case (dfanoutSELM_Mask):
        if(pdfanout->seln==0) break;
        for(i=0, plink=&(pdfanout->outa), state=pdfanout->seln;
        i<OUT_ARG_MAX;
        i++, plink++, state>>=1) {
            if(state&1) {
                status=dbPutLink(plink,DBR_DOUBLE,&(pdfanout->val),1);
                if(status) recGblSetSevr(pdfanout,LINK_ALARM,MAJOR_ALARM);
            }
        }
        break;
    default:
        recGblSetSevr(pdfanout,SOFT_ALARM,INVALID_ALARM);
    }

}
