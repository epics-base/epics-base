/* recLongout.c */
/* base/src/rec  $Id$ */
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
 * .01  11-11-91        jba     Moved set and reset of alarm stat and sevr to macros
 * .02  02-05-92	jba	Changed function arguments from paddr to precord 
 * .03  02-28-92	jba	ANSI C changes
 * .04  04-10-92        jba     pact now used to test for asyn processing, not status
 * .05  04-18-92        jba     removed process from dev init_record parms
 * .06  06-02-92        jba     changed graphic/control limits for hihi,high,low,lolo
 * .07  07-15-92        jba     changed VALID_ALARM to INVALID alarm
 * .08  07-16-92        jba     added invalid alarm fwd link test and chngd fwd lnk to macro
 * .09  07-21-92        jba     changed alarm limits for non val related fields
 * .10  08-06-92        jba     New algorithm for calculating analog alarms
 * .11  08-14-92        jba     Added simulation processing
 * .12  08-19-92        jba     Added code for invalid alarm output action
 * .13  09-10-92        jba     modified fetch of val from dol to call recGblGetLinkValue
 * .14  09-18-92        jba     pact now set in recGblGetLinkValue
 * .15  03-29-94        mcn     converted to fast links
 */ 
#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>
#include	<string.h>

#include        <alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbFldTypes.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<recSup.h>
#define GEN_SIZE_OFFSET
#include	<longoutRecord.h>
#undef  GEN_SIZE_OFFSET
#include      <menuIvoa.h>

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
	DEVSUPFUN	write_longout;/*(-1,0)=>(failure,success*/
};
static void alarm();
static void monitor();
static long writeValue();


static long init_record(plongout,pass)
    struct longoutRecord	*plongout;
    int pass;
{
    struct longoutdset *pdset;
    long status=0;

    if (pass==0) return(0);
    if (plongout->siml.type == CONSTANT) {
	recGblInitConstantLink(&plongout->siml,DBF_USHORT,&plongout->simm);
    }
    if(!(pdset = (struct longoutdset *)(plongout->dset))) {
	recGblRecordError(S_dev_noDSET,(void *)plongout,"longout: init_record");
	return(S_dev_noDSET);
    }
    /* must have  write_longout functions defined */
    if( (pdset->number < 5) || (pdset->write_longout == NULL) ) {
	recGblRecordError(S_dev_missingSup,(void *)plongout,"longout: init_record");
	return(S_dev_missingSup);
    }
    if (plongout->dol.type == CONSTANT) {
	if(recGblInitConstantLink(&plongout->dol,DBF_LONG,&plongout->val))
	    plongout->udf=FALSE;
    }
    if( pdset->init_record ) {
	if((status=(*pdset->init_record)(plongout))) return(status);
    }
    return(0);
}

static long process(plongout)
        struct longoutRecord     *plongout;
{
	struct longoutdset	*pdset = (struct longoutdset *)(plongout->dset);
	long		 status=0;
	unsigned char    pact=plongout->pact;

	if( (pdset==NULL) || (pdset->write_longout==NULL) ) {
		plongout->pact=TRUE;
		recGblRecordError(S_dev_missingSup,(void *)plongout,"write_longout");
		return(S_dev_missingSup);
	}
        if (!plongout->pact && plongout->omsl == CLOSED_LOOP) {
		status = dbGetLink(&(plongout->dol),DBR_LONG,
			&(plongout->val),0,0);
		if (plongout->dol.type!=CONSTANT && RTN_SUCCESS(status))
                   plongout->udf=FALSE;
	}

	/* check for alarms */
	alarm(plongout);

        if (plongout->nsev < INVALID_ALARM )
                status=writeValue(plongout); /* write the new value */
        else {
                switch (plongout->ivoa) {
                    case (menuIvoaContinue_normally) :
                        status=writeValue(plongout); /* write the new value */
                        break;
                    case (menuIvoaDon_t_drive_outputs) :
                        break;
                    case (menuIvoaSet_output_to_IVOV) :
                        if(plongout->pact == FALSE){
                                plongout->val=plongout->ivov;
                        }
                        status=writeValue(plongout); /* write the new value */
                        break;
                    default :
                        status=-1;
                        recGblRecordError(S_db_badField,(void *)plongout,
                                "longout:process Illegal IVOA field");
                }
        }

	/* check if device support set pact */
	if ( !pact && plongout->pact ) return(0);
	plongout->pact = TRUE;

	recGblGetTimeStamp(plongout);

	/* check for alarms */
	alarm(plongout);
	/* check event list */
	monitor(plongout);

	/* process the forward scan link record */
	recGblFwdLink(plongout);

	plongout->pact=FALSE;
	return(status);
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct longoutRecord	*plongout=(struct longoutRecord *)paddr->precord;

    strncpy(units,plongout->egu,DB_UNITS_SIZE);
    return(0);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble	*pgd;
{
    struct longoutRecord	*plongout=(struct longoutRecord *)paddr->precord;

    if(paddr->pfield==(void *)&plongout->val
    || paddr->pfield==(void *)&plongout->hihi
    || paddr->pfield==(void *)&plongout->high
    || paddr->pfield==(void *)&plongout->low
    || paddr->pfield==(void *)&plongout->lolo){
        pgd->upper_disp_limit = plongout->hopr;
        pgd->lower_disp_limit = plongout->lopr;
    } else recGblGetGraphicDouble(paddr,pgd);
    return(0);
}

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct longoutRecord	*plongout=(struct longoutRecord *)paddr->precord;

    if(paddr->pfield==(void *)&plongout->val
    || paddr->pfield==(void *)&plongout->hihi
    || paddr->pfield==(void *)&plongout->high
    || paddr->pfield==(void *)&plongout->low
    || paddr->pfield==(void *)&plongout->lolo){
        pcd->upper_ctrl_limit = plongout->hopr;
        pcd->lower_ctrl_limit = plongout->lopr;
    } else recGblGetControlDouble(paddr,pcd);
    return(0);
}
static long get_alarm_double(paddr,pad)
    struct dbAddr *paddr;
    struct dbr_alDouble	*pad;
{
    struct longoutRecord	*plongout=(struct longoutRecord *)paddr->precord;

    if(paddr->pfield==(void *)&plongout->val){
         pad->upper_alarm_limit = plongout->hihi;
         pad->upper_warning_limit = plongout->high;
         pad->lower_warning_limit = plongout->low;
         pad->lower_alarm_limit = plongout->lolo;
    } else recGblGetAlarmDouble(paddr,pad);
    return(0);
}

static void alarm(plongout)
    struct longoutRecord	*plongout;
{
	double		val;
	float		hyst, lalm, hihi, high, low, lolo;
	unsigned short	hhsv, llsv, hsv, lsv;

	if(plongout->udf == TRUE ){
 		recGblSetSevr(plongout,UDF_ALARM,INVALID_ALARM);
		return;
	}
	hihi = plongout->hihi; lolo = plongout->lolo; high = plongout->high; low = plongout->low;
	hhsv = plongout->hhsv; llsv = plongout->llsv; hsv = plongout->hsv; lsv = plongout->lsv;
	val = plongout->val; hyst = plongout->hyst; lalm = plongout->lalm;

	/* alarm condition hihi */
	if (hhsv && (val >= hihi || ((lalm==hihi) && (val >= hihi-hyst)))){
	        if (recGblSetSevr(plongout,HIHI_ALARM,plongout->hhsv)) plongout->lalm = hihi;
		return;
	}

	/* alarm condition lolo */
	if (llsv && (val <= lolo || ((lalm==lolo) && (val <= lolo+hyst)))){
	        if (recGblSetSevr(plongout,LOLO_ALARM,plongout->llsv)) plongout->lalm = lolo;
		return;
	}

	/* alarm condition high */
	if (hsv && (val >= high || ((lalm==high) && (val >= high-hyst)))){
	        if (recGblSetSevr(plongout,HIGH_ALARM,plongout->hsv)) plongout->lalm = high;
		return;
	}

	/* alarm condition low */
	if (lsv && (val <= low || ((lalm==low) && (val <= low+hyst)))){
	        if (recGblSetSevr(plongout,LOW_ALARM,plongout->lsv)) plongout->lalm = low;
		return;
	}

	/* we get here only if val is out of alarm by at least hyst */
	plongout->lalm = val;
	return;
}

static void monitor(plongout)
    struct longoutRecord	*plongout;
{
	unsigned short	monitor_mask;
	long		delta;

        monitor_mask = recGblResetAlarms(plongout);
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

static long writeValue(plongout)
	struct longoutRecord	*plongout;
{
	long		status;
        struct longoutdset 	*pdset = (struct longoutdset *) (plongout->dset);

	if (plongout->pact == TRUE){
		status=(*pdset->write_longout)(plongout);
		return(status);
	}

	status=dbGetLink(&(plongout->siml),DBR_USHORT,&(plongout->simm),0,0);
	if (!RTN_SUCCESS(status))
		return(status);

	if (plongout->simm == NO){
		status=(*pdset->write_longout)(plongout);
		return(status);
	}
	if (plongout->simm == YES){
		status=dbPutLink(&plongout->siol,DBR_LONG,&plongout->val,1);
	} else {
		status=-1;
		recGblSetSevr(plongout,SOFT_ALARM,INVALID_ALARM);
		return(status);
	}
        recGblSetSevr(plongout,SIMM_ALARM,plongout->sims);

	return(status);
}
