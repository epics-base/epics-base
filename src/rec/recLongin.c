/* recLongin.c */
/* share/src/rec $Id$ */

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
 * .01  mm-dd-yy        iii     Comment
 */


#include        <vxWorks.h>
#include        <types.h>
#include        <stdioLib.h>
#include        <lstLib.h>

#include        <alarm.h>
#include        <dbAccess.h>
#include        <dbDefs.h>
#include        <dbFldTypes.h>
#include        <devSup.h>
#include        <errMdef.h>
#include        <link.h>
#include        <recSup.h>
#include	<longinRecord.h>

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
long init_record();
long process();
#define special NULL
long get_value();
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
long get_units();
#define get_precision NULL
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
long get_graphic_double();
long get_control_double();
long get_alarm_double();

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
	DEVSUPFUN	read_longin;/*(0,1)=> success, async */
};
void alarm();
void monitor();

static long init_record(plongin)
    struct longinRecord	*plongin;
{
    struct longindset *pdset;
    long status;

    if(!(pdset = (struct longindset *)(plongin->dset))) {
	recGblRecordError(S_dev_noDSET,plongin,"longin: init_record");
	return(S_dev_noDSET);
    }
    /* must have read_longin function defined */
    if( (pdset->number < 5) || (pdset->read_longin == NULL) ) {
	recGblRecordError(S_dev_missingSup,plongin,"longin: init_record");
	return(S_dev_missingSup);
    }
    if( pdset->init_record ) {
	if((status=(*pdset->init_record)(plongin,process))) return(status);
    }
    return(0);
}

static long process(paddr)
    struct dbAddr	*paddr;
{
    struct longinRecord	*plongin=(struct longinRecord *)(paddr->precord);
	struct longindset	*pdset = (struct longindset *)(plongin->dset);
	long		 status;

	if( (pdset==NULL) || (pdset->read_longin==NULL) ) {
		plongin->pact=TRUE;
		recGblRecordError(S_dev_missingSup,plongin,"read_longin");
		return(S_dev_missingSup);
	}

	status=(*pdset->read_longin)(plongin); /* read the new value */
	plongin->pact = TRUE;

        /* status is one if an asynchronous record is being processed*/
        if (status==1) return(0);

	tsLocalTime(&plongin->time);

	/* check for alarms */
	alarm(plongin);
	/* check event list */
	monitor(plongin);

	/* process the forward scan link record */
	if (plongin->flnk.type==DB_LINK) dbScanPassive(plongin->flnk.value.db_link.pdbAddr);

	plongin->pact=FALSE;
	return(status);
}

static long get_value(plongin,pvdes)
    struct longinRecord             *plongin;
    struct valueDes     *pvdes;
{
    pvdes->field_type = DBF_LONG;
    pvdes->no_elements=1;
    (long *)(pvdes->pvalue) = &plongin->val;
    return(0);
}


static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct longinRecord	*plongin=(struct longinRecord *)paddr->precord;

    strncpy(units,plongin->egu,sizeof(plongin->egu));
    return(0);
}


static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble	*pgd;
{
    struct longinRecord	*plongin=(struct longinRecord *)paddr->precord;

    pgd->upper_disp_limit = plongin->hopr;
    pgd->lower_disp_limit = plongin->lopr;
    return(0);
}

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct longinRecord	*plongin=(struct longinRecord *)paddr->precord;

    pcd->upper_ctrl_limit = plongin->hopr;
    pcd->lower_ctrl_limit = plongin->lopr;
    return(0);
}

static long get_alarm_double(paddr,pad)
    struct dbAddr *paddr;
    struct dbr_alDouble	*pad;
{
    struct longinRecord	*plongin=(struct longinRecord *)paddr->precord;

    pad->upper_alarm_limit = plongin->hihi;
    pad->upper_warning_limit = plongin->high;
    pad->lower_warning_limit = plongin->low;
    pad->lower_alarm_limit = plongin->lolo;
    return(0);
}

static void alarm(plongin)
    struct longinRecord	*plongin;
{
	long	ftemp;
	long	val=plongin->val;

	if(plongin->udf == TRUE ){
		if (plongin->nsev<VALID_ALARM){
			plongin->nsta = UDF_ALARM;
			plongin->nsev = VALID_ALARM;
		}
		return;
	}
	/* if difference is not > hysterisis use lalm not val */
	ftemp = plongin->lalm - plongin->val;
	if(ftemp<0) ftemp = -ftemp;
	if (ftemp < plongin->hyst) val=plongin->lalm;

	/* alarm condition hihi */
	if (plongin->nsev<plongin->hhsv){
		if (val > plongin->hihi){
			plongin->lalm = val;
			plongin->nsta = HIHI_ALARM;
			plongin->nsev = plongin->hhsv;
			return;
		}
	}

	/* alarm condition lolo */
	if (plongin->nsev<plongin->llsv){
		if (val < plongin->lolo){
			plongin->lalm = val;
			plongin->nsta = LOLO_ALARM;
			plongin->nsev = plongin->llsv;
			return;
		}
	}

	/* alarm condition high */
	if (plongin->nsev<plongin->hsv){
		if (val > plongin->high){
			plongin->lalm = val;
			plongin->nsta = HIGH_ALARM;
			plongin->nsev =plongin->hsv;
			return;
		}
	}

	/* alarm condition lolo */
	if (plongin->nsev<plongin->lsv){
		if (val < plongin->low){
			plongin->lalm = val;
			plongin->nsta = LOW_ALARM;
			plongin->nsev = plongin->lsv;
			return;
		}
	}
	return;
}

static void monitor(plongin)
    struct longinRecord	*plongin;
{
	unsigned short	monitor_mask;
	long		delta;
	short		stat,sevr,nsta,nsev;

	/* get previous stat and sevr  and new stat and sevr*/
	stat=plongin->stat;
	sevr=plongin->sevr;
	nsta=plongin->nsta;
	nsev=plongin->nsev;
	/*set current stat and sevr*/
	plongin->stat = nsta;
	plongin->sevr = nsev;
	plongin->nsta = 0;
	plongin->nsev = 0;

        /* Flags which events to fire on the value field */
	monitor_mask = 0;

	/* alarm condition changed this scan */
	if (stat!=nsta || sevr!=nsev) {
		/* post events for alarm condition change*/
		monitor_mask = DBE_ALARM;
		/* post stat and nsev fields */
		db_post_events(plongin,&plongin->stat,DBE_VALUE);
		db_post_events(plongin,&plongin->sevr,DBE_VALUE);
	}
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
