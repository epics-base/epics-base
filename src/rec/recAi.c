/* recAi.c */
/* share/src/rec $Id$ */

/* recAi.c - Record Support Routines for Analog Input records
 *
 * Author: 	Bob Dalesio
 * Date:	7-9-87
 * @(#)iocai.c	1.1	9/22/88
 *
 *	Control System Software for the GTA Project
 *
 *	Copyright 1988, 1989, the Regents of the University of California.
 *
 *	This software was produced under a U.S. Government contract
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory, which is
 *	operated by the University of California for the U.S. Department
 *	of Energy.
 *
 *	Developed by the Controls and Automation Group (AT-8)
 *	Accelerator Technology Division
 *	Los Alamos National Laboratory
 *
 *	Direct inqueries to:
 *	Bob Dalesio, AT-8, Mail Stop H820
 *	Los Alamos National Laboratory
 *	Los Alamos, New Mexico 87545
 *	Phone: (505) 667-3414
 *	E-mail: dalesio@luke.lanl.gov
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
long get_precision();
long get_value();
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
#define get_enum_str NULL
long get_units();
long get_graphic_double();
long get_control_double();
#define get_enum_strs NULL

struct rset aiRSET={
	RSETNUMBER,
	report,
	initialize,
	init_record,
	process,
	special,
	get_precision,
	get_value,
	cvt_dbaddr,
	get_array_info,
	put_array_info,
	get_enum_str,
	get_units,
	get_graphic_double,
	get_control_double,
	get_enum_strs };

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

void convert();
void alarm();
void monitor();

static long init_record(pai)
    struct aiRecord	*pai;
{
    struct aidset *pdset;
    long status;

    /* initialize so that first alarm, archive, and monitor get generated*/
    pai->lalm = 1e30;
    pai->alst = 1e30;
    pai->mlst = 1e30;

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

	/*pact must not be set true until read_ai completes*/
	status=(*pdset->read_ai)(pai); /* read the new value */
	pai->pact = TRUE;
	/* status is one if an asynchronous record is being processed*/
	if(status==1) return(0);
	if(status==2) status=0; else convert(pai);

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
	if(pdset->number<6 || !(pdset->special_linconv)) {
	    recGblDbaddrError(S_db_noMod,paddr,"ai: special");
	    return(S_db_noMod);
	}
	pai->init=TRUE;
	return((*pdset->special_linconv)(pai,after));
    default:
	recGblDbaddrError(S_db_badChoice,paddr,"ai: special");
	return(S_db_badChoice);
    }
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct aiRecord	*pai=(struct aiRecord *)paddr->precord;

    *precision = pai->prec;
    return(0);
}

static long get_value(pai,pvdes)
    struct aiRecord		*pai;
    struct valueDes	*pvdes;
{
    pvdes->field_type = DBF_FLOAT;
    pvdes->no_elements=1;
    (float *)(pvdes->pvalue) = &pai->val;
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

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble	*pgd;
{
    struct aiRecord	*pai=(struct aiRecord *)paddr->precord;

    pgd->upper_disp_limit = pai->hopr;
    pgd->lower_disp_limit = pai->lopr;
    pgd->upper_alarm_limit = pai->hihi;
    pgd->upper_warning_limit = pai->high;
    pgd->lower_warning_limit = pai->low;
    pgd->lower_alarm_limit = pai->lolo;
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

static void alarm(pai)
    struct aiRecord	*pai;
{
	float	ftemp;

	/* if difference is not > hysterisis don't bother */
	ftemp = pai->lalm - pai->val;
	if(ftemp<0.0) ftemp = -ftemp;
	if (ftemp < pai->hyst) return;

	/* alarm condition hihi */
	if (pai->nsev<pai->hhsv){
		if (pai->val > pai->hihi){
			pai->lalm = pai->val;
			pai->nsta = HIHI_ALARM;
			pai->nsev = pai->hhsv;
			return;
		}
	}

	/* alarm condition lolo */
	if (pai->nsev<pai->llsv){
		if (pai->val < pai->lolo){
			pai->lalm = pai->val;
			pai->nsta = LOLO_ALARM;
			pai->nsev = pai->llsv;
			return;
		}
	}

	/* alarm condition high */
	if (pai->nsev<pai->hsv){
		if (pai->val > pai->high){
			pai->lalm = pai->val;
			pai->nsta = HIGH_ALARM;
			pai->nsev =pai->hsv;
			return;
		}
	}

	/* alarm condition lolo */
	if (pai->nsev<pai->lsv){
		if (pai->val < pai->low){
			pai->lalm = pai->val;
			pai->nsta = LOW_ALARM;
			pai->nsev = pai->lsv;
			return;
		}
	}
	return;
}

static void monitor(pai)
    struct aiRecord	*pai;
{
	unsigned short	monitor_mask;
	float		delta;
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

	/* anyone waiting for an event on this record */
	if (pai->mlis.count == 0) return;

	/* Flags which events to fire on the value field */
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
	if(delta<0.0) delta = 0.0;
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

static void convert(pai)
struct aiRecord	*pai;
{
	float			 val;


	val = pai->rval;
	/* adjust slope and offset */
	if(pai->aslo != 0.0) val = val * pai->aslo + pai->aoff;

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
		if( lbrk >= number-1) {
		    if(pai->nsev < VALID_ALARM) {
			pai->nsta = SOFT_ALARM;
			pai->nsev = VALID_ALARM;
		    }
		    break; /* out of while */
		}
		lbrk++;
		pInt = pbrkTable->papBrkInt[lbrk];
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
	    if (pai->init == 0) pai->val = val;	/* initial condition */
	    pai->val = val * (1.00 - pai->smoo) + (pai->val * pai->smoo);
	}else{
	    pai->val = val;
	}
	return;
}
