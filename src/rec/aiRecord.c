/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* base/src/rec $Id$ */
  
/* aiRecord.c - Record Support Routines for Analog Input records */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            7-14-89
 *
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "errlog.h"
#include "epicsMath.h"
#include "alarm.h"
#include "cvtTable.h"
#include "dbAccess.h"
#include "dbScan.h"
#include "dbEvent.h"
#include "dbFldTypes.h"
#include "devSup.h"
#include "menuSimm.h"
#include "recSup.h"
#include "recGbl.h"
#include "special.h"
#include "menuConvert.h"
#define GEN_SIZE_OFFSET
#include "aiRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

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
 
rset aiRSET={
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
epicsExportAddress(rset,aiRSET);

typedef struct aidset { /* analog input dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record; /*returns: (-1,0)=>(failure,success)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_ai;/*(0,2)=> success and convert,don't convert)*/
			/* if convert then raw value stored in rval */
	DEVSUPFUN	special_linconv;
}aidset;


/*Following from timing system		*/
/*
extern unsigned int     gts_trigger_counter;
*/

static void checkAlarms(aiRecord *pai);
static void convert(aiRecord *pai);
static void monitor(aiRecord *pai);
static long readValue(aiRecord *pai);

static long init_record(void *precord,int pass)
{
    aiRecord	*pai = (aiRecord *)precord;
    aidset	*pdset;
    double	eoff = pai->eoff, eslo = pai->eslo;

    if (pass==0) return(0);

    /* ai.siml must be a CONSTANT or a PV_LINK or a DB_LINK */
    if (pai->siml.type == CONSTANT) {
	recGblInitConstantLink(&pai->siml,DBF_USHORT,&pai->simm);
    }

    /* ai.siol must be a CONSTANT or a PV_LINK or a DB_LINK */
    if (pai->siol.type == CONSTANT) {
	recGblInitConstantLink(&pai->siol,DBF_DOUBLE,&pai->sval);
    }

    if(!(pdset = (aidset *)(pai->dset))) {
	recGblRecordError(S_dev_noDSET,(void *)pai,"ai: init_record");
	return(S_dev_noDSET);
    }
    /* must have read_ai function defined */
    if( (pdset->number < 6) || (pdset->read_ai == NULL) ) {
	recGblRecordError(S_dev_missingSup,(void *)pai,"ai: init_record");
	return(S_dev_missingSup);
    }
    pai->init = TRUE;
    /*The following is for old device support that doesnt know about eoff*/
    if ((pai->eslo==1.0) && (pai->eoff==0.0)) {
	pai->eoff = pai->egul;
    }

    if( pdset->init_record ) {
	long status=(*pdset->init_record)(pai);
	if (pai->linr == menuConvertSLOPE) {
	    pai->eoff = eoff;
	    pai->eslo = eslo;
	}
	return (status);
    }
    return(0);
}

static long process(void *precord)
{
	aiRecord	*pai = (aiRecord *)precord;
	aidset		*pdset = (aidset *)(pai->dset);
	long		 status;
	unsigned char    pact=pai->pact;

	if( (pdset==NULL) || (pdset->read_ai==NULL) ) {
		pai->pact=TRUE;
		recGblRecordError(S_dev_missingSup,(void *)pai,"read_ai");
		return(S_dev_missingSup);
	}
	status=readValue(pai); /* read the new value */
	/* check if device support set pact */
	if ( !pact && pai->pact ) return(0);
	pai->pact = TRUE;

	recGblGetTimeStamp(pai);
	if (status==0) convert(pai);
	else if (status==2) status=0;

	/* check for alarms */
	checkAlarms(pai);
	/* check event list */
	monitor(pai);
	/* process the forward scan link record */
        recGblFwdLink(pai);

	pai->init=FALSE;
	pai->pact=FALSE;
	return(status);
}

static long special(DBADDR *paddr,int after)
{
    aiRecord  	*pai = (aiRecord *)(paddr->precord);
    aidset 	*pdset = (aidset *) (pai->dset);
    int          special_type = paddr->special;

    switch(special_type) {
    case(SPC_LINCONV):
	if(pdset->number<6) {
	    recGblDbaddrError(S_db_noMod,paddr,"ai: special");
	    return(S_db_noMod);
	}
	pai->init=TRUE;
	if ((pai->linr == menuConvertLINEAR) && pdset->special_linconv) {
	    double eoff = pai->eoff;
	    double eslo = pai->eslo;
	    long status;
	    pai->eoff = pai->egul;
	    status = (*pdset->special_linconv)(pai,after);
	    if (eoff != pai->eoff)
		db_post_events(pai, &pai->eoff, DBE_VALUE|DBE_LOG);
	    if (eslo != pai->eslo)
		db_post_events(pai, &pai->eslo, DBE_VALUE|DBE_LOG);
	    return(status);
	}
	return(0);
    default:
	recGblDbaddrError(S_db_badChoice,paddr,"ai: special");
	return(S_db_badChoice);
    }
}

static long get_units(DBADDR *paddr, char *units)
{
    aiRecord	*pai=(aiRecord *)paddr->precord;

    strncpy(units,pai->egu,DB_UNITS_SIZE);
    return(0);
}

static long get_precision(DBADDR *paddr, long *precision)
{
    aiRecord	*pai=(aiRecord *)paddr->precord;

    *precision = pai->prec;
    if(paddr->pfield == (void *)&pai->val) return(0);
    recGblGetPrec(paddr,precision);
    return(0);
}

static long get_graphic_double(DBADDR *paddr,struct dbr_grDouble *pgd)
{
    aiRecord	*pai=(aiRecord *)paddr->precord;
    int		fieldIndex = dbGetFieldIndex(paddr);

    if(fieldIndex == aiRecordVAL
    || fieldIndex == aiRecordHIHI
    || fieldIndex == aiRecordHIGH
    || fieldIndex == aiRecordLOW
    || fieldIndex == aiRecordLOLO
    || fieldIndex == aiRecordHOPR
    || fieldIndex == aiRecordLOPR) {
        pgd->upper_disp_limit = pai->hopr;
        pgd->lower_disp_limit = pai->lopr;
    } else recGblGetGraphicDouble(paddr,pgd);
    return(0);
}

static long get_control_double(DBADDR *paddr,struct dbr_ctrlDouble *pcd)
{
    aiRecord	*pai=(aiRecord *)paddr->precord;
    int		fieldIndex = dbGetFieldIndex(paddr);

    if(fieldIndex == aiRecordVAL
    || fieldIndex == aiRecordHIHI
    || fieldIndex == aiRecordHIGH
    || fieldIndex == aiRecordLOW
    || fieldIndex == aiRecordLOLO) {
	pcd->upper_ctrl_limit = pai->hopr;
	pcd->lower_ctrl_limit = pai->lopr;
    } else recGblGetControlDouble(paddr,pcd);
    return(0);
}

static long get_alarm_double(DBADDR *paddr,struct dbr_alDouble *pad)
{
    aiRecord	*pai=(aiRecord *)paddr->precord;
    int		fieldIndex = dbGetFieldIndex(paddr);

    if(fieldIndex == aiRecordVAL) {
	pad->upper_alarm_limit = pai->hihi;
	pad->upper_warning_limit = pai->high;
	pad->lower_warning_limit = pai->low;
	pad->lower_alarm_limit = pai->lolo;
    } else recGblGetAlarmDouble(paddr,pad);
    return(0);
}

static void checkAlarms(aiRecord *pai)
{
	double		val;
	double		hyst, lalm, hihi, high, low, lolo;
	unsigned short	hhsv, llsv, hsv, lsv;

	if (pai->udf) {
 		recGblSetSevr(pai,UDF_ALARM,INVALID_ALARM);
		return;
	}
	hihi = pai->hihi; lolo = pai->lolo; high = pai->high; low = pai->low;
	hhsv = pai->hhsv; llsv = pai->llsv; hsv = pai->hsv; lsv = pai->lsv;
	val = pai->val; hyst = pai->hyst; lalm = pai->lalm;

	/* alarm condition hihi */
	if (hhsv && (val >= hihi || ((lalm==hihi) && (val >= hihi-hyst)))){
	        if (recGblSetSevr(pai,HIHI_ALARM,pai->hhsv)) pai->lalm = hihi;
		return;
	}

	/* alarm condition lolo */
	if (llsv && (val <= lolo || ((lalm==lolo) && (val <= lolo+hyst)))){
	        if (recGblSetSevr(pai,LOLO_ALARM,pai->llsv)) pai->lalm = lolo;
		return;
	}

	/* alarm condition high */
	if (hsv && (val >= high || ((lalm==high) && (val >= high-hyst)))){
	        if (recGblSetSevr(pai,HIGH_ALARM,pai->hsv)) pai->lalm = high;
		return;
	}

	/* alarm condition low */
	if (lsv && (val <= low || ((lalm==low) && (val <= low+hyst)))){
	        if (recGblSetSevr(pai,LOW_ALARM,pai->lsv)) pai->lalm = low;
		return;
	}

	/* we get here only if val is out of alarm by at least hyst */
	pai->lalm = val;
	return;
}

static void convert(aiRecord *pai)
{
	double val;


	val = (double)pai->rval + (double)pai->roff;
	/* adjust slope and offset */
	if(pai->aslo!=0.0) val*=pai->aslo;
	val+=pai->aoff;

	/* convert raw to engineering units and signal units */
	switch (pai->linr) {
	case menuConvertNO_CONVERSION:
		break; /* do nothing*/
	
	case menuConvertLINEAR:
	case menuConvertSLOPE:
		val = (val * pai->eslo) + pai->eoff;
		break;
	
	default: /* must use breakpoint table */
                if (cvtRawToEngBpt(&val,pai->linr,pai->init,(void *)&pai->pbrk,&pai->lbrk)!=0) {
                      recGblSetSevr(pai,SOFT_ALARM,MAJOR_ALARM);
                }
	}

	/* apply smoothing algorithm */
	if (pai->smoo != 0.0){
	    if (pai->init) pai->val = val;	/* initial condition */
	    pai->val = val * (1.00 - pai->smoo) + (pai->val * pai->smoo);
	}else{
	    pai->val = val;
	}
	pai->udf = isnan(pai->val);
	return;
}

static void monitor(aiRecord *pai)
{
	unsigned short	monitor_mask;
	double		delta;

        monitor_mask = recGblResetAlarms(pai);
	/* check for value change */
	delta = pai->mlst - pai->val;
	if(delta<0.0) delta = -delta;
	if (delta > pai->mdel) {
		/* post events for value change */
		monitor_mask |= DBE_VALUE;
		/* update last value monitored */
		pai->mlst = pai->val;
	}

	/* check for archive change */
	delta = pai->alst - pai->val;
	if(delta<0.0) delta = -delta;
	if (delta > pai->adel) {
		/* post events on value field for archive change */
		monitor_mask |= DBE_LOG;
		/* update last archive value monitored */
		pai->alst = pai->val;
	}

	/* send out monitors connected to the value field */
	if (monitor_mask){
		db_post_events(pai,&pai->val,monitor_mask);
		if(pai->oraw != pai->rval) {
			db_post_events(pai,&pai->rval,monitor_mask);
			pai->oraw = pai->rval;
		}
	}
	return;
}

static long readValue(aiRecord *pai)
{
	long		status;
        aidset 	*pdset = (aidset *) (pai->dset);

	if (pai->pact == TRUE){
		status=(*pdset->read_ai)(pai);
		return(status);
	}

	status = dbGetLink(&(pai->siml),DBR_USHORT,&(pai->simm),0,0);

	if (status)
		return(status);

	if (pai->simm == menuSimmNO){
		status=(*pdset->read_ai)(pai);
		return(status);
	}
	if (pai->simm == menuSimmYES){
		status = dbGetLink(&(pai->siol),DBR_DOUBLE,&(pai->sval),0,0);
		if (status==0){
			 pai->val=pai->sval;
			 pai->udf=isnan(pai->val);
		}
                status=2; /* dont convert */
	}
	if (pai->simm == menuSimmRAW){
		status = dbGetLink(&(pai->siol),DBR_DOUBLE,&(pai->sval),0,0);
		if (status==0) {
			pai->udf=isnan(pai->sval);
			if (!pai->udf) {
				pai->rval=(long)floor(pai->sval);
			}
		}
		status=0; /* convert since we've written RVAL */
	} else {
		status=-1;
		recGblSetSevr(pai,SOFT_ALARM,INVALID_ALARM);
		return(status);
	}
        recGblSetSevr(pai,SIMM_ALARM,pai->sims);

	return(status);
}
