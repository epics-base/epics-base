/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* $Id$ */

/* recLongin.c - Record Support Routines for Longin records */
/*
 *      Author: 	Janet Anderson
 *      Date:   	9/23/91
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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
#define GEN_SIZE_OFFSET
#include "longinRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(longinRecord *, int);
static long process(longinRecord *);
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

rset longinRSET={
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
epicsExportAddress(rset,longinRSET);


struct longindset { /* longin input dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record; /*returns: (-1,0)=>(failure,success)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_longin; /*returns: (-1,0)=>(failure,success)*/
};
static void checkAlarms(longinRecord *plongin);
static void monitor(longinRecord *plongin);
static long readValue(longinRecord *plongin);


static long init_record(longinRecord *plongin, int pass)
{
    struct longindset *pdset;
    long status;

    if (pass==0) return(0);

    /* longin.siml must be a CONSTANT or a PV_LINK or a DB_LINK */
    if (plongin->siml.type == CONSTANT) {
	recGblInitConstantLink(&plongin->siml,DBF_USHORT,&plongin->simm);
    }

    /* longin.siol must be a CONSTANT or a PV_LINK or a DB_LINK */
    if (plongin->siol.type == CONSTANT) {
	recGblInitConstantLink(&plongin->siol,DBF_LONG,&plongin->sval);
    }

    if(!(pdset = (struct longindset *)(plongin->dset))) {
	recGblRecordError(S_dev_noDSET,(void *)plongin,"longin: init_record");
	return(S_dev_noDSET);
    }
    /* must have read_longin function defined */
    if( (pdset->number < 5) || (pdset->read_longin == NULL) ) {
	recGblRecordError(S_dev_missingSup,(void *)plongin,"longin: init_record");
	return(S_dev_missingSup);
    }
    if( pdset->init_record ) {
	if((status=(*pdset->init_record)(plongin))) return(status);
    }
    return(0);
}

static long process(longinRecord *plongin)
{
	struct longindset	*pdset = (struct longindset *)(plongin->dset);
	long		 status;
	unsigned char    pact=plongin->pact;

	if( (pdset==NULL) || (pdset->read_longin==NULL) ) {
		plongin->pact=TRUE;
		recGblRecordError(S_dev_missingSup,(void *)plongin,"read_longin");
		return(S_dev_missingSup);
	}

	status=readValue(plongin); /* read the new value */
	/* check if device support set pact */
	if ( !pact && plongin->pact ) return(0);
	plongin->pact = TRUE;

	recGblGetTimeStamp(plongin);
	if (status==0) plongin->udf = FALSE;

	/* check for alarms */
	checkAlarms(plongin);
	/* check event list */
	monitor(plongin);
	/* process the forward scan link record */
	recGblFwdLink(plongin);

	plongin->pact=FALSE;
	return(status);
}

static long get_units(DBADDR *paddr,char *units)
{
    longinRecord *plongin=(longinRecord *)paddr->precord;

    strncpy(units,plongin->egu,DB_UNITS_SIZE);
    return(0);
}


static long get_graphic_double(DBADDR *paddr, struct dbr_grDouble *pgd)
{
    longinRecord *plongin=(longinRecord *)paddr->precord;

    if(paddr->pfield==(void *)&plongin->val
    || paddr->pfield==(void *)&plongin->hihi
    || paddr->pfield==(void *)&plongin->high
    || paddr->pfield==(void *)&plongin->low
    || paddr->pfield==(void *)&plongin->lolo){
        pgd->upper_disp_limit = plongin->hopr;
        pgd->lower_disp_limit = plongin->lopr;
    } else recGblGetGraphicDouble(paddr,pgd);
    return(0);
}

static long get_control_double(DBADDR *paddr, struct dbr_ctrlDouble *pcd)
{
    longinRecord *plongin=(longinRecord *)paddr->precord;

    if(paddr->pfield==(void *)&plongin->val
    || paddr->pfield==(void *)&plongin->hihi
    || paddr->pfield==(void *)&plongin->high
    || paddr->pfield==(void *)&plongin->low
    || paddr->pfield==(void *)&plongin->lolo){
        pcd->upper_ctrl_limit = plongin->hopr;
        pcd->lower_ctrl_limit = plongin->lopr;
    } else recGblGetControlDouble(paddr,pcd);
    return(0);
}

static long get_alarm_double(DBADDR *paddr, struct dbr_alDouble	*pad)
{
    longinRecord *plongin=(longinRecord *)paddr->precord;

    if(paddr->pfield==(void *)&plongin->val){
         pad->upper_alarm_limit = plongin->hihi;
         pad->upper_warning_limit = plongin->high;
         pad->lower_warning_limit = plongin->low;
         pad->lower_alarm_limit = plongin->lolo;
    } else recGblGetAlarmDouble(paddr,pad);
    return(0);
}

static void checkAlarms(longinRecord *prec)
{
    epicsInt32 val, hyst, lalm;
    epicsInt32 alev;
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

static void monitor(longinRecord *plongin)
{
	unsigned short	monitor_mask;
	epicsInt32	delta;

	/* get previous stat and sevr  and new stat and sevr*/
        monitor_mask = recGblResetAlarms(plongin);
	/* check for value change */
	delta = plongin->mlst - plongin->val;
	if(delta<0) delta = -delta;
        if (delta > plongin->mdel || delta==0x80000000) {
		/* post events for value change */
		monitor_mask |= DBE_VALUE;
		/* update last value monitored */
		plongin->mlst = plongin->val;
	}

	/* check for archive change */
	delta = plongin->alst - plongin->val;
	if(delta<0) delta = -delta;
	if (delta > plongin->adel || delta==0x80000000) {
		/* post events on value field for archive change */
		monitor_mask |= DBE_LOG;
		/* update last archive value monitored */
		plongin->alst = plongin->val;
	}

	/* send out monitors connected to the value field */
	if (monitor_mask){
		db_post_events(plongin,&plongin->val,monitor_mask);
	}
	return;
}

static long readValue(longinRecord *plongin)
{
	long status;
        struct longindset *pdset = (struct longindset *) (plongin->dset);

	if (plongin->pact == TRUE){
		status=(*pdset->read_longin)(plongin);
		return(status);
	}

	status=dbGetLink(&(plongin->siml),DBR_USHORT, &(plongin->simm),0,0);
	if (status)
		return(status);

	if (plongin->simm == NO){
		status=(*pdset->read_longin)(plongin);
		return(status);
	}
	if (plongin->simm == YES){
		status=dbGetLink(&(plongin->siol),DBR_LONG,
			&(plongin->sval),0,0);

		if (status==0) {
			plongin->val=plongin->sval;
			plongin->udf=FALSE;
		}
	} else {
		status=-1;
		recGblSetSevr(plongin,SOFT_ALARM,INVALID_ALARM);
		return(status);
	}
        recGblSetSevr(plongin,SIMM_ALARM,plongin->sims);

	return(status);
}
