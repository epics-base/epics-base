/* recAi.c */
/* share/src/rec  $Id$ */
  
/* recAi.c - Record Support Routines for Analog Input records */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            7-14-89
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 *
 * Modification Log:
 * -----------------
 * .01	8-12-88		lrd	Put in breakpoint conversions
 *				Fix conversions for bipolar xy566 cards
 * .02	12-12-88	lrd	Lock record on entry and unlock on exit
 * .03	12-15-88	lrd	Process the forward scan link
 * .04	12-23-88	lrd	Alarm on locked MAX_LOCKED times
 * .05	01-13-89	lrd	deleted db_read_ai
 * .06	01-18-89	lrd	modify adjustment offset to be independent of
 *				adjustment slope
 *				clamp xy566 breakpoint table negative values t o
 *				zero
 * .07	03-27-89	lrd	make hardware errors MAJOR alarms
 *				remove hardware severity from the database
 * .08	04-03-89	lrd	add monitor code
 *				removed signal units conversion stuff
 * .09	04-05-89	lrd	modified so that negative adjustment slope ASLO
 *				would work
 * .10	05-03-89	lrd	removed process mask from arg list
 * .11	08-01-89	lrd	only set value for constant when val != 0
 * .12	10-11-89	lrd	fix smoothing initial condition
 * .13	01-31-90	lrd	add AB plc flag to the ab_aidriver call
 * .14	03-21-90	lrd	add db_post_event for RVAL
 * .15	04-11-90	lrd	make locals static
 * .16  04-18-90	mrk	extensible record and device support
 * .17  09-18-91	jba	fixed bug in break point table conversion
 * .18  09-30-91	jba	Moved break point table conversion to libCom
 * .19  11-11-91	jba	Moved set and reset of alarm stat and sevr to macros
 * .20  12-18-91	jba	Changed E_IO_INTERRUPT to SCAN_IO_EVENT
 * .21  02-05-92	jba	Changed function arguments from paddr to precord 
 * .22  02-28-92	jba	Changed get_precision,get_graphic_double,get_control_double
 * .23  02-28-92	jba	ANSI C changes
 * .24  04-10-92	jba	pact now used to test for asyn processing, not status
 * .25  04-18-92        jba     removed process from dev init_record parms
 * .26  06-02-92        jba     changed graphic/control limits for hihi,high,low,lolo
 * .28  07-15-92        jba     changed VALID_ALARM to INVALID alarm
 * .29  07-16-92        jba     added invalid alarm fwd link test and chngd fwd lnk to macro
 * .30  07-21-92        jba     changed alarm limits for non val related fields
 * .31  08-06-92        jba     New algorithm for calculating analog alarms
 * .32  08-13-92        jba     Added simulation processing
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>
#include	<string.h>

#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbScan.h>
#include	<dbFldTypes.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<recSup.h>
#include	<special.h>
#include	<aiRecord.h>

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
static long special();
static long get_value();
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
 
struct rset aiRSET={
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
	get_alarm_double};

struct aidset { /* analog input dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record; /*returns: (-1,0)=>(failure,success)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_ai;/*(0,2)=> success and convert,don't convert)*/
			/* if convert then raw value stored in rval */
	DEVSUPFUN	special_linconv;
};


/*Following from timing system		*/
extern unsigned int     gts_trigger_counter;

static void alarm();
static void convert();
static void monitor();
static long readValue();

static long init_record(pai,pass)
    struct aiRecord	*pai;
    int pass;
{
    struct aidset *pdset;
    long status;

    if (pass==0) return(0);

    /* ai.siml must be a CONSTANT or a PV_LINK or a DB_LINK */
    switch (pai->siml.type) {
    case (CONSTANT) :
        pai->simm = pai->siml.value.value;
        break;
    case (PV_LINK) :
        status = dbCaAddInlink(&(pai->siml), (void *) pai, "SIMM");
	if(status) return(status);
	break;
    case (DB_LINK) :
        break;
    default :
        recGblRecordError(S_db_badField,(void *)pai,
                "ai: init_record Illegal SIML field");
        return(S_db_badField);
    }

    /* ai.siol must be a CONSTANT or a PV_LINK or a DB_LINK */
    switch (pai->siol.type) {
    case (CONSTANT) :
        pai->sval = pai->siol.value.value;
        break;
    case (PV_LINK) :
        status = dbCaAddInlink(&(pai->siol), (void *) pai, "SVAL");
	if(status) return(status);
	break;
    case (DB_LINK) :
        break;
    default :
        recGblRecordError(S_db_badField,(void *)pai,
                "ai: init_record Illegal SIOL field");
        return(S_db_badField);
    }

    if(!(pdset = (struct aidset *)(pai->dset))) {
	recGblRecordError(S_dev_noDSET,(void *)pai,"ai: init_record");
	return(S_dev_noDSET);
    }
    /* must have read_ai function defined */
    if( (pdset->number < 6) || (pdset->read_ai == NULL) ) {
	recGblRecordError(S_dev_missingSup,(void *)pai,"ai: init_record");
	return(S_dev_missingSup);
    }
    pai->init = TRUE;

    if( pdset->init_record ) {
	if((status=(*pdset->init_record)(pai))) return(status);
    }
    return(0);
}

static long process(pai)
	struct aiRecord	*pai;
{
	struct aidset	*pdset = (struct aidset *)(pai->dset);
	long		 status;
	unsigned char    pact=pai->pact;

	if( (pdset==NULL) || (pdset->read_ai==NULL) ) {
		pai->pact=TRUE;
		recGblRecordError(S_dev_missingSup,(void *)pai,"read_ai");
		return(S_dev_missingSup);
	}

        /* event throttling */
        if (pai->scan == SCAN_IO_EVENT){
                if ((pai->evnt != 0)  && (gts_trigger_counter != 0)){
                        if ((gts_trigger_counter % pai->evnt) != 0){
                                return(0);
                        }
                }
        }

	status=readValue(pai); /* read the new value */
	/* check if device support set pact */
	if ( !pact && pai->pact ) return(0);
	pai->pact = TRUE;

	tsLocalTime(&pai->time);
	if (status==0) convert(pai);
	else if (status==2) status=0;

	/* check for alarms */
	alarm(pai);
	/* check event list */
	monitor(pai);
	/* process the forward scan link record */
        recGblFwdLink(pai);

	pai->init=FALSE;
	pai->pact=FALSE;
	return(status);
}

static long special(paddr,after)
    struct dbAddr *paddr;
    int	   	  after;
{
    struct aiRecord  	*pai = (struct aiRecord *)(paddr->precord);
    struct aidset 	*pdset = (struct aidset *) (pai->dset);
    int           	special_type = paddr->special;

    switch(special_type) {
    case(SPC_LINCONV):
	if(pdset->number<6) {
	    recGblDbaddrError(S_db_noMod,paddr,"ai: special");
	    return(S_db_noMod);
	}
	pai->init=TRUE;
	if(!(pdset->special_linconv)) return(0);
	return((*pdset->special_linconv)(pai,after));
    default:
	recGblDbaddrError(S_db_badChoice,paddr,"ai: special");
	return(S_db_badChoice);
    }
}

static long get_value(pai,pvdes)
    struct aiRecord		*pai;
    struct valueDes	*pvdes;
{
    pvdes->field_type = DBF_DOUBLE;
    pvdes->no_elements=1;
    (double *)(pvdes->pvalue) = &pai->val;
    return(0);
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct aiRecord	*pai=(struct aiRecord *)paddr->precord;

    strncpy(units,pai->egu,sizeof(pai->egu));
    return(0);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct aiRecord	*pai=(struct aiRecord *)paddr->precord;

    *precision = pai->prec;
    if(paddr->pfield == (void *)&pai->val) return(0);
    recGblGetPrec(paddr,precision);
    return(0);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble	*pgd;
{
    struct aiRecord	*pai=(struct aiRecord *)paddr->precord;

    if(paddr->pfield==(void *)&pai->val ||
       paddr->pfield==(void *)&pai->hihi ||
       paddr->pfield==(void *)&pai->high ||
       paddr->pfield==(void *)&pai->low ||
       paddr->pfield==(void *)&pai->lolo){
        pgd->upper_disp_limit = pai->hopr;
        pgd->lower_disp_limit = pai->lopr;
    } else recGblGetGraphicDouble(paddr,pgd);
    return(0);
}

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct aiRecord	*pai=(struct aiRecord *)paddr->precord;

    if(paddr->pfield==(void *)&pai->val ||
       paddr->pfield==(void *)&pai->hihi ||
       paddr->pfield==(void *)&pai->high ||
       paddr->pfield==(void *)&pai->low ||
       paddr->pfield==(void *)&pai->lolo){
    pcd->upper_ctrl_limit = pai->hopr;
    pcd->lower_ctrl_limit = pai->lopr;
    } else recGblGetControlDouble(paddr,pcd);
    return(0);
}

static long get_alarm_double(paddr,pad)
    struct dbAddr *paddr;
    struct dbr_alDouble	*pad;
{
    struct aiRecord	*pai=(struct aiRecord *)paddr->precord;

    if(paddr->pfield==(void *)&pai->val){
         pad->upper_alarm_limit = pai->hihi;
         pad->upper_warning_limit = pai->high;
         pad->lower_warning_limit = pai->low;
         pad->lower_alarm_limit = pai->lolo;
    } else recGblGetAlarmDouble(paddr,pad);
    return(0);
}

static void alarm(pai)
    struct aiRecord	*pai;
{
	double		val;
	float		hyst, lalm, hihi, high, low, lolo;
	unsigned short	hhsv, llsv, hsv, lsv;

	if(pai->udf == TRUE ){
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

static void convert(pai)
struct aiRecord	*pai;
{
	double			 val;
	float		aslo=pai->aslo;
	float		aoff=pai->aoff;


	val = pai->rval + pai->roff;
	/* adjust slope and offset */
	if(aslo!=0.0) val*=aslo;
	if(aoff!=0.0) val+=aoff;

	/* convert raw to engineering units and signal units */
	if(pai->linr == 0) {
		; /* do nothing*/
	}
	else if(pai->linr == 1) {
		val = (val * pai->eslo) + pai->egul;
	}
	else { /* must use breakpoint table */
                if (cvtRawToEngBpt(&val,pai->linr,pai->init,(void *)&pai->pbrk,&pai->lbrk)!=0) {
                      recGblSetSevr(pai,SOFT_ALARM,INVALID_ALARM);
                }
	}

	/* apply smoothing algorithm */
	if (pai->smoo != 0.0){
	    if (pai->init == TRUE) pai->val = val;	/* initial condition */
	    pai->val = val * (1.00 - pai->smoo) + (pai->val * pai->smoo);
	}else{
	    pai->val = val;
	}
	pai->udf = FALSE;
	return;
}

static void monitor(pai)
    struct aiRecord	*pai;
{
	unsigned short	monitor_mask;
	double		delta;
	short		stat,sevr,nsta,nsev;

	/* get previous stat and sevr  and new stat and sevr*/
        recGblResetSevr(pai,stat,sevr,nsta,nsev);

	monitor_mask = 0;

	/* alarm condition changed this scan */
	if (stat!=nsta || sevr!=nsev) {
		/* post events for alarm condition change*/
		monitor_mask = DBE_ALARM;
		/* post stat and nsev fields */
		db_post_events(pai,&pai->stat,DBE_VALUE);
		db_post_events(pai,&pai->sevr,DBE_VALUE);
	}
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

static long readValue(pai)
	struct aiRecord	*pai;
{
	long		status;
        struct aidset 	*pdset = (struct aidset *) (pai->dset);
	long            nRequest=1;
	long            options=0;

	if (pai->pact == TRUE){
		status=(*pdset->read_ai)(pai);
		return(status);
	}

	status=recGblGetLinkValue(&(pai->siml),
		(void *)pai,DBR_ENUM,&(pai->simm),&options,&nRequest);
	if (status)
		return(status);

	if (pai->simm == NO){
		status=(*pdset->read_ai)(pai);
		return(status);
	}
	if (pai->simm == YES){
		status=recGblGetLinkValue(&(pai->siol),
				(void *)pai,DBR_DOUBLE,&(pai->sval),&options,&nRequest);
		if (status==0){
			 pai->val=pai->sval;
			 pai->udf=FALSE;
		}
                status=2; /* dont convert */
	} else {
		status=-1;
		recGblSetSevr(pai,SOFT_ALARM,INVALID_ALARM);
		return(status);
	}
        recGblSetSevr(pai,SIMM_ALARM,pai->sims);

	return(status);
}
