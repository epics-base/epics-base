/* recLongin.c */
/* base/src/rec  $Id$ */

/* recLongin.c - Record Support Routines for Longin records */
/*
 *      Author: 	Janet Anderson
 *      Date:   	9/23/91
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01  11-11-91        jba     Moved set and reset of alarm stat and sevr to macros
 * .02  02-05-92	jba	Changed function arguments from paddr to precord 
 * .03  02-28-92        jba     Changed get_precision,get_graphic_double,get_control_double
 * .04  02-28-92	jba	ANSI C changes
 * .05  04-10-92        jba     pact now used to test for asyn processing, not status
 * .06  04-18-92        jba     removed process from dev init_record parms
 * .07  06-02-92        jba     changed graphic/control limits for hihi,high,low,lolo
 * .08  07-15-92        jba     changed VALID_ALARM to INVALID alarm
 * .09  07-16-92        jba     added invalid alarm fwd link test and chngd fwd lnk to macro
 * .10  07-21-92        jba     changed alarm limits for non val related fields
 * .11  08-06-92        jba     New algorithm for calculating analog alarms
 * .12  08-13-92        jba     Added simulation processing
 * .13  03-29-94        mcn     Converted to Fast Links
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
#define get_precision NULL
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double();
static long get_control_double();
static long get_alarm_double();

struct rset longinRSET={
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


struct longindset { /* longin input dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record; /*returns: (-1,0)=>(failure,success)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_longin; /*returns: (-1,0)=>(failure,success)*/
};
static void alarm();
static void monitor();
static long readValue();


static long init_record(plongin,pass)
    struct longinRecord	*plongin;
    int pass;
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

static long process(plongin)
	struct longinRecord     *plongin;
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
	alarm(plongin);
	/* check event list */
	monitor(plongin);
	/* process the forward scan link record */
	recGblFwdLink(plongin);

	plongin->pact=FALSE;
	return(status);
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct longinRecord	*plongin=(struct longinRecord *)paddr->precord;

    strncpy(units,plongin->egu,DB_UNITS_SIZE);
    return(0);
}


static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble	*pgd;
{
    struct longinRecord	*plongin=(struct longinRecord *)paddr->precord;

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

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct longinRecord	*plongin=(struct longinRecord *)paddr->precord;

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

static long get_alarm_double(paddr,pad)
    struct dbAddr *paddr;
    struct dbr_alDouble	*pad;
{
    struct longinRecord	*plongin=(struct longinRecord *)paddr->precord;

    if(paddr->pfield==(void *)&plongin->val){
         pad->upper_alarm_limit = plongin->hihi;
         pad->upper_warning_limit = plongin->high;
         pad->lower_warning_limit = plongin->low;
         pad->lower_alarm_limit = plongin->lolo;
    } else recGblGetAlarmDouble(paddr,pad);
    return(0);
}

static void alarm(plongin)
    struct longinRecord	*plongin;
{
	long		val;
	long		hyst, lalm, hihi, high, low, lolo;
	unsigned short	hhsv, llsv, hsv, lsv;

	if(plongin->udf == TRUE ){
 		recGblSetSevr(plongin,UDF_ALARM,INVALID_ALARM);
		return;
	}
	hihi = plongin->hihi; lolo = plongin->lolo; high = plongin->high; low = plongin->low;
	hhsv = plongin->hhsv; llsv = plongin->llsv; hsv = plongin->hsv; lsv = plongin->lsv;
	val = plongin->val; hyst = plongin->hyst; lalm = plongin->lalm;

	/* alarm condition hihi */
	if (hhsv && (val >= hihi || ((lalm==hihi) && (val >= hihi-hyst)))){
	        if (recGblSetSevr(plongin,HIHI_ALARM,plongin->hhsv)) plongin->lalm = hihi;
		return;
	}

	/* alarm condition lolo */
	if (llsv && (val <= lolo || ((lalm==lolo) && (val <= lolo+hyst)))){
	        if (recGblSetSevr(plongin,LOLO_ALARM,plongin->llsv)) plongin->lalm = lolo;
		return;
	}

	/* alarm condition high */
	if (hsv && (val >= high || ((lalm==high) && (val >= high-hyst)))){
	        if (recGblSetSevr(plongin,HIGH_ALARM,plongin->hsv)) plongin->lalm = high;
		return;
	}

	/* alarm condition low */
	if (lsv && (val <= low || ((lalm==low) && (val <= low+hyst)))){
	        if (recGblSetSevr(plongin,LOW_ALARM,plongin->lsv)) plongin->lalm = low;
		return;
	}

	/* we get here only if val is out of alarm by at least hyst */
	plongin->lalm = val;
	return;
}

static void monitor(plongin)
    struct longinRecord	*plongin;
{
	unsigned short	monitor_mask;
	long		delta;

	/* get previous stat and sevr  and new stat and sevr*/
        monitor_mask = recGblResetAlarms(plongin);
	/* check for value change */
	delta = plongin->mlst - plongin->val;
	if(delta<0) delta = -delta;
	if (delta > plongin->mdel) {
		/* post events for value change */
		monitor_mask |= DBE_VALUE;
		/* update last value monitored */
		plongin->mlst = plongin->val;
	}

	/* check for archive change */
	delta = plongin->alst - plongin->val;
	if(delta<0) delta = -delta;
	if (delta > plongin->adel) {
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

static long readValue(plongin)
	struct longinRecord	*plongin;
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
