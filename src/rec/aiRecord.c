/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* aiRecord.c - Record Support Routines for Analog Input records */
/*
 *      Original Author: Bob Dalesio
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
static long init_record(void *, int);
static long process(void *);
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
static long get_control_double(DBADDR *, struct dbr_ctrlDouble *);
static long get_alarm_double(DBADDR *, struct dbr_alDouble *);
 
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

static void checkAlarms(aiRecord *prec);
static void convert(aiRecord *prec);
static void monitor(aiRecord *prec);
static long readValue(aiRecord *prec);

static long init_record(void *precord,int pass)
{
    aiRecord	*prec = (aiRecord *)precord;
    aidset	*pdset;
    double	eoff = prec->eoff, eslo = prec->eslo;

    if (pass==0) return(0);

    /* ai.siml must be a CONSTANT or a PV_LINK or a DB_LINK */
    if (prec->siml.type == CONSTANT) {
	recGblInitConstantLink(&prec->siml,DBF_USHORT,&prec->simm);
    }

    /* ai.siol must be a CONSTANT or a PV_LINK or a DB_LINK */
    if (prec->siol.type == CONSTANT) {
	recGblInitConstantLink(&prec->siol,DBF_DOUBLE,&prec->sval);
    }

    if(!(pdset = (aidset *)(prec->dset))) {
	recGblRecordError(S_dev_noDSET,(void *)prec,"ai: init_record");
	return(S_dev_noDSET);
    }
    /* must have read_ai function defined */
    if( (pdset->number < 6) || (pdset->read_ai == NULL) ) {
	recGblRecordError(S_dev_missingSup,(void *)prec,"ai: init_record");
	return(S_dev_missingSup);
    }
    prec->init = TRUE;
    /*The following is for old device support that doesnt know about eoff*/
    if ((prec->eslo==1.0) && (prec->eoff==0.0)) {
	prec->eoff = prec->egul;
    }

    if( pdset->init_record ) {
	long status=(*pdset->init_record)(prec);
	if (prec->linr == menuConvertSLOPE) {
	    prec->eoff = eoff;
	    prec->eslo = eslo;
	}
	return (status);
    }
    prec->mlst = prec->val;
    prec->alst = prec->val;
    prec->lalm = prec->val;
    prec->oraw = prec->rval;
    return(0);
}

static long process(void *precord)
{
	aiRecord	*prec = (aiRecord *)precord;
	aidset		*pdset = (aidset *)(prec->dset);
	long		 status;
	unsigned char    pact=prec->pact;

	if( (pdset==NULL) || (pdset->read_ai==NULL) ) {
		prec->pact=TRUE;
		recGblRecordError(S_dev_missingSup,(void *)prec,"read_ai");
		return(S_dev_missingSup);
	}
	status=readValue(prec); /* read the new value */
	/* check if device support set pact */
	if ( !pact && prec->pact ) return(0);
	prec->pact = TRUE;

	recGblGetTimeStamp(prec);
	if (status==0) convert(prec);
	else if (status==2) status=0;

	/* check for alarms */
	checkAlarms(prec);
	/* check event list */
	monitor(prec);
	/* process the forward scan link record */
        recGblFwdLink(prec);

	prec->init=FALSE;
	prec->pact=FALSE;
	return(status);
}

static long special(DBADDR *paddr,int after)
{
    aiRecord  	*prec = (aiRecord *)(paddr->precord);
    aidset 	*pdset = (aidset *) (prec->dset);
    int          special_type = paddr->special;

    switch(special_type) {
    case(SPC_LINCONV):
	if(pdset->number<6) {
	    recGblDbaddrError(S_db_noMod,paddr,"ai: special");
	    return(S_db_noMod);
	}
	prec->init=TRUE;
	if ((prec->linr == menuConvertLINEAR) && pdset->special_linconv) {
	    double eoff = prec->eoff;
	    double eslo = prec->eslo;
	    long status;
	    prec->eoff = prec->egul;
	    status = (*pdset->special_linconv)(prec,after);
	    if (eoff != prec->eoff)
		db_post_events(prec, &prec->eoff, DBE_VALUE|DBE_LOG);
	    if (eslo != prec->eslo)
		db_post_events(prec, &prec->eslo, DBE_VALUE|DBE_LOG);
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
    aiRecord	*prec=(aiRecord *)paddr->precord;

    strncpy(units,prec->egu,DB_UNITS_SIZE);
    return(0);
}

static long get_precision(DBADDR *paddr, long *precision)
{
    aiRecord	*prec=(aiRecord *)paddr->precord;

    *precision = prec->prec;
    if(paddr->pfield == (void *)&prec->val) return(0);
    recGblGetPrec(paddr,precision);
    return(0);
}

static long get_graphic_double(DBADDR *paddr,struct dbr_grDouble *pgd)
{
    aiRecord	*prec=(aiRecord *)paddr->precord;
    int		fieldIndex = dbGetFieldIndex(paddr);

    if(fieldIndex == aiRecordVAL
    || fieldIndex == aiRecordHIHI
    || fieldIndex == aiRecordHIGH
    || fieldIndex == aiRecordLOW
    || fieldIndex == aiRecordLOLO
    || fieldIndex == aiRecordHOPR
    || fieldIndex == aiRecordLOPR) {
        pgd->upper_disp_limit = prec->hopr;
        pgd->lower_disp_limit = prec->lopr;
    } else recGblGetGraphicDouble(paddr,pgd);
    return(0);
}

static long get_control_double(DBADDR *paddr,struct dbr_ctrlDouble *pcd)
{
    aiRecord	*prec=(aiRecord *)paddr->precord;
    int		fieldIndex = dbGetFieldIndex(paddr);

    if(fieldIndex == aiRecordVAL
    || fieldIndex == aiRecordHIHI
    || fieldIndex == aiRecordHIGH
    || fieldIndex == aiRecordLOW
    || fieldIndex == aiRecordLOLO) {
	pcd->upper_ctrl_limit = prec->hopr;
	pcd->lower_ctrl_limit = prec->lopr;
    } else recGblGetControlDouble(paddr,pcd);
    return(0);
}

static long get_alarm_double(DBADDR *paddr,struct dbr_alDouble *pad)
{
    aiRecord	*prec=(aiRecord *)paddr->precord;
    int		fieldIndex = dbGetFieldIndex(paddr);

    if(fieldIndex == aiRecordVAL) {
        pad->upper_alarm_limit = prec->hhsv ? prec->hihi : epicsNAN;
        pad->upper_warning_limit = prec->hsv ? prec->high : epicsNAN;
        pad->lower_warning_limit = prec->lsv ? prec->low : epicsNAN;
        pad->lower_alarm_limit = prec->llsv ? prec->lolo : epicsNAN;
    } else recGblGetAlarmDouble(paddr,pad);
    return(0);
}

static void checkAlarms(aiRecord *prec)
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

static void convert(aiRecord *prec)
{
	double val;


	val = (double)prec->rval + (double)prec->roff;
	/* adjust slope and offset */
	if(prec->aslo!=0.0) val*=prec->aslo;
	val+=prec->aoff;

	/* convert raw to engineering units and signal units */
	switch (prec->linr) {
	case menuConvertNO_CONVERSION:
		break; /* do nothing*/
	
	case menuConvertLINEAR:
	case menuConvertSLOPE:
		val = (val * prec->eslo) + prec->eoff;
		break;
	
	default: /* must use breakpoint table */
                if (cvtRawToEngBpt(&val,prec->linr,prec->init,(void *)&prec->pbrk,&prec->lbrk)!=0) {
                      recGblSetSevr(prec,SOFT_ALARM,MAJOR_ALARM);
                }
	}

	/* apply smoothing algorithm */
    if (prec->smoo != 0.0 && finite(prec->val)){
	    if (prec->init) prec->val = val;	/* initial condition */
	    prec->val = val * (1.00 - prec->smoo) + (prec->val * prec->smoo);
	}else{
	    prec->val = val;
	}
	prec->udf = isnan(prec->val);
	return;
}

static void monitor(aiRecord *prec)
{
	unsigned short	monitor_mask;
	double		delta;

        monitor_mask = recGblResetAlarms(prec);
	/* check for value change */
	delta = prec->mlst - prec->val;
	if(delta<0.0) delta = -delta;
	if (!(delta <= prec->mdel)) { /* Handles MDEL == NAN */
		/* post events for value change */
		monitor_mask |= DBE_VALUE;
		/* update last value monitored */
		prec->mlst = prec->val;
	}

	/* check for archive change */
	delta = prec->alst - prec->val;
	if(delta<0.0) delta = -delta;
	if (!(delta <= prec->adel)) { /* Handles ADEL == NAN */
		/* post events on value field for archive change */
		monitor_mask |= DBE_LOG;
		/* update last archive value monitored */
		prec->alst = prec->val;
	}

	/* send out monitors connected to the value field */
	if (monitor_mask){
		db_post_events(prec,&prec->val,monitor_mask);
		if(prec->oraw != prec->rval) {
			db_post_events(prec,&prec->rval,monitor_mask);
			prec->oraw = prec->rval;
		}
	}
	return;
}

static long readValue(aiRecord *prec)
{
	long		status;
        aidset 	*pdset = (aidset *) (prec->dset);

	if (prec->pact == TRUE){
		status=(*pdset->read_ai)(prec);
		return(status);
	}

	status = dbGetLink(&(prec->siml),DBR_USHORT,&(prec->simm),0,0);

	if (status)
		return(status);

	if (prec->simm == menuSimmNO){
		status=(*pdset->read_ai)(prec);
		return(status);
	}
	if (prec->simm == menuSimmYES){
		status = dbGetLink(&(prec->siol),DBR_DOUBLE,&(prec->sval),0,0);
		if (status==0){
			 prec->val=prec->sval;
			 prec->udf=isnan(prec->val);
		}
                status=2; /* dont convert */
	}
	else if (prec->simm == menuSimmRAW){
		status = dbGetLink(&(prec->siol),DBR_DOUBLE,&(prec->sval),0,0);
		if (status==0) {
			prec->udf=isnan(prec->sval);
			if (!prec->udf) {
				prec->rval=(long)floor(prec->sval);
			}
		}
		status=0; /* convert since we've written RVAL */
	} else {
		status=-1;
		recGblSetSevr(prec,SOFT_ALARM,INVALID_ALARM);
		return(status);
	}
        recGblSetSevr(prec,SIMM_ALARM,prec->sims);

	return(status);
}
