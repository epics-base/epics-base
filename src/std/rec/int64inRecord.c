/*************************************************************************\
* Copyright (c) 2016 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* int64inRecord.c - Record Support Routines for int64in records */
/*
 *      Original Author: Janet Anderson
 *      Date:   	9/23/91
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include "alarm.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "dbFldTypes.h"
#include "devSup.h"
#include "errMdef.h"
#include "recSup.h"
#include "recGbl.h"
#include "menuYesNo.h"

#define GEN_SIZE_OFFSET
#include "int64inRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

/* Hysterisis for alarm filtering: 1-1/e */
#define THRESHOLD 0.6321
/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(dbCommon *, int);
static long process(dbCommon *);
#define special NULL
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
static long get_alarm_double(DBADDR *, struct dbr_alDouble	*);

rset int64inRSET={
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
epicsExportAddress(rset,int64inRSET);


struct int64indset { /* int64in input dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record; /*returns: (-1,0)=>(failure,success)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_int64in; /*returns: (-1,0)=>(failure,success)*/
};
static void checkAlarms(int64inRecord *prec, epicsTimeStamp *timeLast);
static void monitor(int64inRecord *prec);
static long readValue(int64inRecord *prec);


static long init_record(dbCommon *pcommon, int pass)
{
    int64inRecord *prec = (int64inRecord*)pcommon;
    struct int64indset *pdset;
    long status;

    if (pass==0) return(0);

    /* int64in.siml must be a CONSTANT or a PV_LINK or a DB_LINK */
    if (prec->siml.type == CONSTANT) {
	recGblInitConstantLink(&prec->siml,DBF_USHORT,&prec->simm);
    }

    /* int64in.siol must be a CONSTANT or a PV_LINK or a DB_LINK */
    if (prec->siol.type == CONSTANT) {
	recGblInitConstantLink(&prec->siol,DBF_LONG,&prec->sval);
    }

    if(!(pdset = (struct int64indset *)(prec->dset))) {
	recGblRecordError(S_dev_noDSET,(void *)prec,"int64in: init_record");
	return(S_dev_noDSET);
    }
    /* must have read_int64in function defined */
    if( (pdset->number < 5) || (pdset->read_int64in == NULL) ) {
	recGblRecordError(S_dev_missingSup,(void *)prec,"int64in: init_record");
	return(S_dev_missingSup);
    }
    if( pdset->init_record ) {
	if((status=(*pdset->init_record)(prec))) return(status);
    }
    prec->mlst = prec->val;
    prec->alst = prec->val;
    prec->lalm = prec->val;
    return(0);
}

static long process(dbCommon *pcommon)
{
    int64inRecord *prec = (int64inRecord*)pcommon;
	struct int64indset	*pdset = (struct int64indset *)(prec->dset);
	long		 status;
	unsigned char    pact=prec->pact;
	epicsTimeStamp   timeLast;

	if( (pdset==NULL) || (pdset->read_int64in==NULL) ) {
		prec->pact=TRUE;
		recGblRecordError(S_dev_missingSup,(void *)prec,"read_int64in");
		return(S_dev_missingSup);
	}
	timeLast = prec->time;

	status=readValue(prec); /* read the new value */
	/* check if device support set pact */
	if ( !pact && prec->pact ) return(0);
	prec->pact = TRUE;

	recGblGetTimeStamp(prec);
	if (status==0) prec->udf = FALSE;

	/* check for alarms */
	checkAlarms(prec, &timeLast);
	/* check event list */
	monitor(prec);
	/* process the forward scan link record */
	recGblFwdLink(prec);

	prec->pact=FALSE;
	return(status);
}

#define indexof(field) int64inRecord##field

static long get_units(DBADDR *paddr,char *units)
{
    int64inRecord *prec=(int64inRecord *)paddr->precord;

    if(paddr->pfldDes->field_type == DBF_LONG) {
        strncpy(units,prec->egu,DB_UNITS_SIZE);
    }
    return(0);
}


static long get_graphic_double(DBADDR *paddr, struct dbr_grDouble *pgd)
{
    int64inRecord *prec=(int64inRecord *)paddr->precord;

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

static long get_control_double(DBADDR *paddr, struct dbr_ctrlDouble *pcd)
{
    int64inRecord *prec=(int64inRecord *)paddr->precord;

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

static long get_alarm_double(DBADDR *paddr, struct dbr_alDouble	*pad)
{
    int64inRecord *prec=(int64inRecord *)paddr->precord;

    if(dbGetFieldIndex(paddr) == indexof(VAL)){
         pad->upper_alarm_limit = prec->hihi;
         pad->upper_warning_limit = prec->high;
         pad->lower_warning_limit = prec->low;
         pad->lower_alarm_limit = prec->lolo;
    } else recGblGetAlarmDouble(paddr,pad);
    return(0);
}

static void checkAlarms(int64inRecord *prec, epicsTimeStamp *timeLast)
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
    
    double aftc, afvl;
    epicsInt64 val, hyst, lalm;
    epicsInt64 alev;
    epicsEnum16 asev;

    if (prec->udf) {
        recGblSetSevr(prec, UDF_ALARM, prec->udfs);
        prec->afvl = 0;
        return;
    }

    val = prec->val;
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
            double t = epicsTimeDiffInSeconds(&prec->time, timeLast);
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

/* DELTA calculates the absolute difference between its arguments
 * expressed as an unsigned 32-bit integer */
#define DELTA(last, val) \
    ((epicsUInt32) ((last) > (val) ? (last) - (val) : (val) - (last)))

static void monitor(int64inRecord *prec)
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

static long readValue(int64inRecord *prec)
{
	long status;
        struct int64indset *pdset = (struct int64indset *) (prec->dset);

	if (prec->pact == TRUE){
		status=(*pdset->read_int64in)(prec);
		return(status);
	}

	status=dbGetLink(&(prec->siml),DBR_USHORT, &(prec->simm),0,0);
	if (status)
		return(status);

	if (prec->simm == menuYesNoNO){
		status=(*pdset->read_int64in)(prec);
		return(status);
	}
	if (prec->simm == menuYesNoYES){
		status=dbGetLink(&(prec->siol),DBR_LONG,
			&(prec->sval),0,0);

		if (status==0) {
			prec->val=prec->sval;
			prec->udf=FALSE;
		}
	} else {
		status=-1;
		recGblSetSevr(prec,SOFT_ALARM,INVALID_ALARM);
		return(status);
	}
        recGblSetSevr(prec,SIMM_ALARM,prec->sims);

	return(status);
}
