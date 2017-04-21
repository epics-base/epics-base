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

/* Hysterisis for alarm filtering: 1-1/e */
#define THRESHOLD 0.6321

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


static void checkAlarms(aiRecord *prec, epicsTimeStamp *lastTime);
static void convert(aiRecord *prec);
static void monitor(aiRecord *prec);
static long readValue(aiRecord *prec);

static long init_record(struct dbCommon *pcommon, int pass)
{
    struct aiRecord *prec = (struct aiRecord *)pcommon;
    aidset	*pdset;
    double	eoff = prec->eoff, eslo = prec->eslo;

    if (pass==0) return(0);

    recGblInitConstantLink(&prec->siml,DBF_USHORT,&prec->simm);
    recGblInitConstantLink(&prec->siol,DBF_DOUBLE,&prec->sval);

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

static long process(struct dbCommon *pcommon)
{
    struct aiRecord *prec = (struct aiRecord *)pcommon;
    aidset		*pdset = (aidset *)(prec->dset);
	long		 status;
	unsigned char    pact=prec->pact;
        epicsTimeStamp	 timeLast;

	if( (pdset==NULL) || (pdset->read_ai==NULL) ) {
		prec->pact=TRUE;
		recGblRecordError(S_dev_missingSup,(void *)prec,"read_ai");
		return(S_dev_missingSup);
	}
        timeLast = prec->time;

	status=readValue(prec); /* read the new value */
	/* check if device support set pact */
	if ( !pact && prec->pact ) return(0);
	prec->pact = TRUE;

	recGblGetTimeStamp(prec);
	if (status==0) convert(prec);
	else if (status==2) status=0;

	/* check for alarms */
	checkAlarms(prec,&timeLast);
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

#define indexof(field) aiRecord##field

static long get_units(DBADDR *paddr, char *units)
{
    aiRecord	*prec=(aiRecord *)paddr->precord;

    if(paddr->pfldDes->field_type == DBF_DOUBLE) {
        switch (dbGetFieldIndex(paddr)) {
            case indexof(ASLO):
            case indexof(AOFF):
            case indexof(SMOO):
                break;
            default:
                strncpy(units,prec->egu,DB_UNITS_SIZE);
        }
    }
    return(0);
}

static long get_precision(const DBADDR *paddr, long *precision)
{
    aiRecord	*prec=(aiRecord *)paddr->precord;

    *precision = prec->prec;
    if (dbGetFieldIndex(paddr) == indexof(VAL)) return(0);
    recGblGetPrec(paddr,precision);
    return(0);
}

static long get_graphic_double(DBADDR *paddr,struct dbr_grDouble *pgd)
{
    aiRecord	*prec=(aiRecord *)paddr->precord;

    switch (dbGetFieldIndex(paddr)) {
        case indexof(VAL):
        case indexof(HIHI):
        case indexof(HIGH):
        case indexof(LOW):
        case indexof(LOLO):
        case indexof(LALM):
        case indexof(ALST):
        case indexof(MLST):
        case indexof(SVAL):
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
    aiRecord	*prec=(aiRecord *)paddr->precord;

    switch (dbGetFieldIndex(paddr)) {
        case indexof(VAL):
        case indexof(HIHI):
        case indexof(HIGH):
        case indexof(LOW):
        case indexof(LOLO):
        case indexof(LALM):
        case indexof(ALST):
        case indexof(MLST):
        case indexof(SVAL):
	    pcd->upper_ctrl_limit = prec->hopr;
	    pcd->lower_ctrl_limit = prec->lopr;
            break;
        default:
            recGblGetControlDouble(paddr,pcd);
    }
    return(0);
}

static long get_alarm_double(DBADDR *paddr,struct dbr_alDouble *pad)
{
    aiRecord	*prec=(aiRecord *)paddr->precord;

    if (dbGetFieldIndex(paddr) == indexof(VAL)) {
        pad->upper_alarm_limit = prec->hhsv ? prec->hihi : epicsNAN;
        pad->upper_warning_limit = prec->hsv ? prec->high : epicsNAN;
        pad->lower_warning_limit = prec->lsv ? prec->low : epicsNAN;
        pad->lower_alarm_limit = prec->llsv ? prec->lolo : epicsNAN;
    } else recGblGetAlarmDouble(paddr,pad);
    return(0);
}

static void checkAlarms(aiRecord *prec, epicsTimeStamp *lastTime)
{
    enum {
        range_Lolo = 1,
        range_Low,
        range_Normal,
        range_High,
        range_Hihi
    } alarmRange;
    static const epicsEnum16 range_stat[] = {
        SOFT_ALARM, LOLO_ALARM, LOW_ALARM,
        NO_ALARM, HIGH_ALARM, HIHI_ALARM
    };
    double val, hyst, lalm, alev, aftc, afvl;
    epicsEnum16 asev;

    if (prec->udf) {
        recGblSetSevr(prec, UDF_ALARM, prec->udfs);
        prec->afvl = 0;
        return;
    }

    val  = prec->val;
    hyst = prec->hyst;
    lalm = prec->lalm;

    /* check VAL against alarm limits */
    if ((asev = prec->hhsv) &&
        (val >= (alev = prec->hihi) ||
         ((lalm == alev) && (val >= alev - hyst))))
        alarmRange = range_Hihi;
    else
    if ((asev = prec->llsv) &&
        (val <= (alev = prec->lolo) ||
         ((lalm == alev) && (val <= alev + hyst))))
        alarmRange = range_Lolo;
    else
    if ((asev = prec->hsv) &&
        (val >= (alev = prec->high) ||
         ((lalm == alev) && (val >= alev - hyst))))
        alarmRange = range_High;
    else
    if ((asev = prec->lsv) &&
        (val <= (alev = prec->low) ||
         ((lalm == alev) && (val <= alev + hyst))))
        alarmRange = range_Low;
    else {
        alev = val;
        asev = NO_ALARM;
        alarmRange = range_Normal;
    }

    aftc = prec->aftc;
    afvl = 0;

    if (aftc > 0) {
        /* Apply level filtering */
        afvl = prec->afvl;
        if (afvl == 0) {
            afvl = (double)alarmRange;
        } else {
            double t = epicsTimeDiffInSeconds(&prec->time, lastTime);
            double alpha = aftc / (t + aftc);

            /* The sign of afvl indicates whether the result should be
             * rounded up or down.  This gives the filter hysteresis.
             * If afvl > 0 the floor() function rounds to a lower alarm
             * level, otherwise to a higher.
             */
            afvl = alpha * afvl +
                   ((afvl > 0) ? (1 - alpha) : (alpha - 1)) * alarmRange;
            if (afvl - floor(afvl) > THRESHOLD)
                afvl = -afvl; /* reverse rounding */

            alarmRange = abs((int)floor(afvl));
            switch (alarmRange) {
            case range_Hihi:
                asev = prec->hhsv;
                alev = prec->hihi;
                break;
            case range_High:
                asev = prec->hsv;
                alev = prec->high;
                break;
            case range_Normal:
                asev = NO_ALARM;
                break;
            case range_Low:
                asev = prec->lsv;
                alev = prec->low;
                break;
            case range_Lolo:
                asev = prec->llsv;
                alev = prec->lolo;
                break;
            }
        }
    }
    prec->afvl = afvl;

    if (asev) {
        /* Report alarm condition, store LALM for future HYST calculations */
        if (recGblSetSevr(prec, range_stat[alarmRange], asev))
            prec->lalm = alev;
    } else {
        /* No alarm condition, reset LALM */
        prec->lalm = val;
    }
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
    unsigned monitor_mask = recGblResetAlarms(prec);

    /* check for value change */
    recGblCheckDeadband(&prec->mlst, prec->val, prec->mdel, &monitor_mask, DBE_VALUE);

    /* check for archive change */
    recGblCheckDeadband(&prec->alst, prec->val, prec->adel, &monitor_mask, DBE_ARCHIVE);

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
