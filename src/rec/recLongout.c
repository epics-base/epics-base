/* recLongout.c */
/* share/src/rec $Id$ */

/* recLongout.c - Record Support Routines for Longout records */
/*
 * Author: 	Janet Anderson
 * Date:	9/23/91
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


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>

#include        <alarm.h>
#include	<dbAccess.h>
#include	<dbDefs.h>
#include	<dbFldTypes.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<link.h>
#include	<recSup.h>
#include	<longoutRecord.h>

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

struct rset longoutRSET={
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


struct longoutdset { /* longout input dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record; /*returns: (-1,0)=>(failure,success)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_longout;/*(-1,0,1)=>(failure,success,don't Continue*/
};
void alarm();
int convert();
void monitor();

static long init_record(plongout)
    struct longoutRecord	*plongout;
{
    struct longoutdset *pdset;
    long status=0;

    if(!(pdset = (struct longoutdset *)(plongout->dset))) {
	recGblRecordError(S_dev_noDSET,plongout,"longout: init_record");
	return(S_dev_noDSET);
    }
    /* must have  write_longout functions defined */
    if( (pdset->number < 5) || (pdset->write_longout == NULL) ) {
	recGblRecordError(S_dev_missingSup,plongout,"longout: init_record");
	return(S_dev_missingSup);
    }
    /* get the initial value dol is a constant*/
    if (plongout->dol.type == CONSTANT){
        plongout->val = plongout->dol.value.value;
	plongout->udf=FALSE;
    }
    if( pdset->init_record ) {
	if((status=(*pdset->init_record)(plongout,process))) return(status);
    }
    return(0);
}

static long process(paddr)
    struct dbAddr	*paddr;
{
    struct longoutRecord	*plongout=(struct longoutRecord *)(paddr->precord);
	struct longoutdset	*pdset = (struct longoutdset *)(plongout->dset);
	long		 status=0;

	if( (pdset==NULL) || (pdset->write_longout==NULL) ) {
		plongout->pact=TRUE;
		recGblRecordError(S_dev_missingSup,plongout,"write_longout");
		return(S_dev_missingSup);
	}
        if (!plongout->pact) {
		if((plongout->dol.type == DB_LINK) && (plongout->omsl == CLOSED_LOOP)){
			long options=0;
			long nRequest=1;

			plongout->pact = TRUE;
			status = dbGetLink(&plongout->dol.value.db_link,plongout,
				DBR_LONG,&plongout->val,&options,&nRequest);
			plongout->pact = FALSE;
			if(status!=0){
				if(plongout->nsev < VALID_ALARM) {
					plongout->nsev = VALID_ALARM;
					plongout->nsta = LINK_ALARM;
				}
			} else plongout->udf=FALSE;
		}
	}

	if(status==0) {
		status=(*pdset->write_longout)(plongout); /* write the new value */
	}
	plongout->pact = TRUE;

	/* status is one if an asynchronous record is being processed*/
	if (status==1) return(0);

	tsLocalTime(&plongout->time);

	/* check for alarms */
	alarm(plongout);
	/* check event list */
	monitor(plongout);

	/* process the forward scan link record */
	if (plongout->flnk.type==DB_LINK) dbScanPassive(plongout->flnk.value.db_link.pdbAddr);

	plongout->pact=FALSE;
	return(status);
}

static long get_value(plongout,pvdes)
    struct longoutRecord             *plongout;
    struct valueDes     *pvdes;
{
    pvdes->field_type = DBF_LONG;
    pvdes->no_elements=1;
    (long *)(pvdes->pvalue) = &plongout->val;
    return(0);
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct longoutRecord	*plongout=(struct longoutRecord *)paddr->precord;

    strncpy(units,plongout->egu,sizeof(plongout->egu));
    return(0);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble	*pgd;
{
    struct longoutRecord	*plongout=(struct longoutRecord *)paddr->precord;

    pgd->upper_disp_limit = plongout->hopr;
    pgd->lower_disp_limit = plongout->lopr;
    return(0);
}

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct longoutRecord	*plongout=(struct longoutRecord *)paddr->precord;

    pcd->upper_ctrl_limit = plongout->hopr;
    pcd->lower_ctrl_limit = plongout->lopr;
    return(0);
}
static long get_alarm_double(paddr,pad)
    struct dbAddr *paddr;
    struct dbr_alDouble	*pad;
{
    struct longoutRecord	*plongout=(struct longoutRecord *)paddr->precord;

    pad->upper_alarm_limit = plongout->hihi;
    pad->upper_warning_limit = plongout->high;
    pad->lower_warning_limit = plongout->low;
    pad->lower_alarm_limit = plongout->lolo;
    return(0);
}


static void alarm(plongout)
    struct longoutRecord	*plongout;
{
	long	ftemp;
	long	val=plongout->val;

        if(plongout->udf == TRUE ){
                if (plongout->nsev<VALID_ALARM){
                        plongout->nsta = UDF_ALARM;
                        plongout->nsev = VALID_ALARM;
                }
                return;
        }

        /* if difference is not > hysterisis use lalm not val */
        ftemp = plongout->lalm - plongout->val;
        if(ftemp<0) ftemp = -ftemp;
        if (ftemp < plongout->hyst) val=plongout->lalm;

        /* alarm condition hihi */
        if (plongout->nsev<plongout->hhsv){
                if (val > plongout->hihi){
                        plongout->lalm = val;
                        plongout->nsta = HIHI_ALARM;
                        plongout->nsev = plongout->hhsv;
                        return;
                }
        }

        /* alarm condition lolo */
        if (plongout->nsev<plongout->llsv){
                if (val < plongout->lolo){
                        plongout->lalm = val;
                        plongout->nsta = LOLO_ALARM;
                        plongout->nsev = plongout->llsv;
                        return;
                }
        }

        /* alarm condition high */
        if (plongout->nsev<plongout->hsv){
                if (val > plongout->high){
                        plongout->lalm = val;
                        plongout->nsta = HIGH_ALARM;
                        plongout->nsev =plongout->hsv;
                        return;
                }
        }

        /* alarm condition lolo */
        if (plongout->nsev<plongout->lsv){
                if (val < plongout->low){
                        plongout->lalm = val;
                        plongout->nsta = LOW_ALARM;
                        plongout->nsev = plongout->lsv;
                        return;
                }
        }
        return;
}


static void monitor(plongout)
    struct longoutRecord	*plongout;
{
	unsigned short	monitor_mask;

	long		delta;
        short           stat,sevr,nsta,nsev;

        /* get previous stat and sevr  and new stat and sevr*/
        stat=plongout->stat;
        sevr=plongout->sevr;
        nsta=plongout->nsta;
        nsev=plongout->nsev;
        /*set current stat and sevr*/
        plongout->stat = nsta;
        plongout->sevr = nsev;
        plongout->nsta = 0;
        plongout->nsev = 0;

        /* Flags which events to fire on the value field */
	monitor_mask = 0;

	/* alarm condition changed this scan */
        if (stat!=nsta || sevr!=nsev) {
                /* post events for alarm condition change*/
                monitor_mask = DBE_ALARM;
                /* post stat and nsev fields */
                db_post_events(plongout,&plongout->stat,DBE_VALUE);
                db_post_events(plongout,&plongout->sevr,DBE_VALUE);
        }
        /* check for value change */
        delta = plongout->mlst - plongout->val;
        if(delta<0) delta = -delta;
        if (delta > plongout->mdel) {
                /* post events for value change */
                monitor_mask |= DBE_VALUE;
                /* update last value monitored */
                plongout->mlst = plongout->val;
        }
        /* check for archive change */
        delta = plongout->alst - plongout->val;
        if(delta<0) delta = -delta;
        if (delta > plongout->adel) {
                /* post events on value field for archive change */
                monitor_mask |= DBE_LOG;
                /* update last archive value monitored */
                plongout->alst = plongout->val;
        }

        /* send out monitors connected to the value field */
        if (monitor_mask){
                db_post_events(plongout,&plongout->val,monitor_mask);
	}
	return;
}
