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
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>

#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbAccess.h>
#include	<dbDefs.h>
#include	<dbFldTypes.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<link.h>
#include	<recSup.h>
#include	<special.h>
#include	<aiRecord.h>

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
long init_record();
long process();
long special();
long get_value();
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
long get_units();
long get_precision();
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
long get_graphic_double();
long get_control_double();
long get_alarm_double();

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
	DEVSUPFUN	read_ai;/*(0,1,2)=> success and */
			/*(convert,don't continue, don't convert)*/
			/* if convert then raw value stored in rval */
	DEVSUPFUN	special_linconv;
};


/*NOTE FOLLOWING IS TAKEN FROM dbScan.c */
#define E_IO_INTERRUPT  7
/*Following from timing system		*/
extern unsigned int     gts_trigger_counter;

void alarm();
void convert();
void monitor();

static long init_record(pai)
    struct aiRecord	*pai;
{
    struct aidset *pdset;
    long status;

    if(!(pdset = (struct aidset *)(pai->dset))) {
	recGblRecordError(S_dev_noDSET,pai,"ai: init_record");
	return(S_dev_noDSET);
    }
    /* must have read_ai function defined */
    if( (pdset->number < 6) || (pdset->read_ai == NULL) ) {
	recGblRecordError(S_dev_missingSup,pai,"ai: init_record");
	return(S_dev_missingSup);
    }
    pai->init = TRUE;
    if( pdset->init_record ) {
	if((status=(*pdset->init_record)(pai,process))) return(status);
    }
    if(pai->linr >= 2) { /*must find breakpoint table*/
	if( !cvtTable || (cvtTable->number < pai->linr)
	||  (!(cvtTable->papBrkTable[pai->linr]))) {
		errMessage(S_db_badField,"Breakpoint Table not Found");
		return(S_db_badField);
	}
	pai->pbrk = (char *)(cvtTable->papBrkTable[pai->linr]);
	pai->lbrk=0;
    }
    return(0);
}

static long process(paddr)
    struct dbAddr	*paddr;
{
    struct aiRecord	*pai=(struct aiRecord *)(paddr->precord);
	struct aidset	*pdset = (struct aidset *)(pai->dset);
	long		 status;

	if( (pdset==NULL) || (pdset->read_ai==NULL) ) {
		pai->pact=TRUE;
		recGblRecordError(S_dev_missingSup,pai,"read_ai");
		return(S_dev_missingSup);
	}

        /* event throttling */
        if (pai->scan == E_IO_INTERRUPT){
                if ((pai->evnt != 0)  && (gts_trigger_counter != 0)){
                        if ((gts_trigger_counter % pai->evnt) != 0){
                                return(0);
                        }
                }
        }

	/*pact must not be set true until read_ai is called*/
	status=(*pdset->read_ai)(pai); /* read the new value */
	pai->pact = TRUE;
	/* status is one if an asynchronous record is being processed*/
	if (status==1) return(0);
	tsLocalTime(&pai->time);
	if (status==0) convert(pai);
	else if (status==2) status=0;

	/* check for alarms */
	alarm(pai);
	/* check event list */
	monitor(pai);
	/* process the forward scan link record */
	if (pai->flnk.type==DB_LINK) dbScanPassive(pai->flnk.value.db_link.pdbAddr);

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
    return(0);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble	*pgd;
{
    struct aiRecord	*pai=(struct aiRecord *)paddr->precord;

    pgd->upper_disp_limit = pai->hopr;
    pgd->lower_disp_limit = pai->lopr;
    return(0);
}

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct aiRecord	*pai=(struct aiRecord *)paddr->precord;

    pcd->upper_ctrl_limit = pai->hopr;
    pcd->lower_ctrl_limit = pai->lopr;
    return(0);
}

static long get_alarm_double(paddr,pad)
    struct dbAddr *paddr;
    struct dbr_alDouble	*pad;
{
    struct aiRecord	*pai=(struct aiRecord *)paddr->precord;

    pad->upper_alarm_limit = pai->hihi;
    pad->upper_warning_limit = pai->high;
    pad->lower_warning_limit = pai->low;
    pad->lower_alarm_limit = pai->lolo;
    return(0);
}

static void alarm(pai)
    struct aiRecord	*pai;
{
	double	ftemp;
	double	val=pai->val;

	if(pai->udf == TRUE ){
		if (pai->nsev<VALID_ALARM){
			pai->nsta = UDF_ALARM;
			pai->nsev = VALID_ALARM;
		}
		return;
	}
	/* if difference is not > hysterisis use lalm not val */
	ftemp = pai->lalm - pai->val;
	if(ftemp<0.0) ftemp = -ftemp;
	if (ftemp < pai->hyst) val=pai->lalm;

	/* alarm condition hihi */
	if (pai->nsev<pai->hhsv){
		if (val > pai->hihi){
			pai->lalm = val;
			pai->nsta = HIHI_ALARM;
			pai->nsev = pai->hhsv;
			return;
		}
	}

	/* alarm condition lolo */
	if (pai->nsev<pai->llsv){
		if (val < pai->lolo){
			pai->lalm = val;
			pai->nsta = LOLO_ALARM;
			pai->nsev = pai->llsv;
			return;
		}
	}

	/* alarm condition high */
	if (pai->nsev<pai->hsv){
		if (val > pai->high){
			pai->lalm = val;
			pai->nsta = HIGH_ALARM;
			pai->nsev =pai->hsv;
			return;
		}
	}

	/* alarm condition lolo */
	if (pai->nsev<pai->lsv){
		if (val < pai->low){
			pai->lalm = val;
			pai->nsta = LOW_ALARM;
			pai->nsev = pai->lsv;
			return;
		}
	}
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
	val = val * aslo + aoff;

	/* convert raw to engineering units and signal units */
	if(pai->linr == 0) {
		; /* do nothing*/
	}
	else if(pai->linr == 1) {
		val = (val * pai->eslo) + pai->egul;
	}
	else { /* must use breakpoint table */
	    struct brkTable *pbrkTable;
	    struct brkInt   *pInt;	
	    struct brkInt   *pnxtInt;
	    short	    lbrk;
	    int		    number;

	    pbrkTable = (struct brkTable *) pai->pbrk;
	    number = pbrkTable->number;
	    if(pai->init) lbrk = number/2; /* Just start in the middle */
	    else {
		lbrk = (pai->lbrk);
		/*make sure we dont go off end of table*/
		if( (lbrk+1) >= number ) lbrk--;
	    }
	    pInt = pbrkTable->papBrkInt[lbrk];
	    pnxtInt = pbrkTable->papBrkInt[lbrk+1];
	    /* find entry for increased value */
 	    while( (pnxtInt->raw) <= val ) {
		lbrk++;
		pInt = pbrkTable->papBrkInt[lbrk];
		if( lbrk >= number-1) {
		    if(pai->nsev < VALID_ALARM) {
			pai->nsta = SOFT_ALARM;
			pai->nsev = VALID_ALARM;
		    }
		    break; /* out of while */
		}
		pnxtInt = pbrkTable->papBrkInt[lbrk+1];
	    }
	    while( (pInt->raw) > val) {
		if(lbrk==0) {
		    if(pai->nsev < VALID_ALARM) {
			pai->nsta = SOFT_ALARM;
			pai->nsev = VALID_ALARM;
		    }
		    break; /* out of while */
		}
		lbrk--;
		pInt = pbrkTable->papBrkInt[lbrk];
	    }
	    pai->lbrk = lbrk;
	    val = pInt->eng + (val - pInt->raw) * pInt->slope;
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
	stat=pai->stat;
	sevr=pai->sevr;
	nsta=pai->nsta;
	nsev=pai->nsev;
	/*set current stat and sevr*/
	pai->stat = nsta;
	pai->sevr = nsev;
	pai->nsta = 0;
	pai->nsev = 0;

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
