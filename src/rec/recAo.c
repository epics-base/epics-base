/* recAo.c */
/* share/src/rec  $Id$ */
  
/* recAo.c - Record Support Routines for Analog Output records */
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
 * Modification Log:
 * -----------------
 * .01  09-26-88        lrd     interface the Ziomek085 card
 * .02  12-12-88        lrd     lock record on entry unlock on exit
 * .03  12-15-88        lrd     Process the forward scan link
 * .04  12-23-88        lrd     Alarm on locked MAX_LOCKED times
 * .05  01-13-89        lrd     deleted db_write_ao
 * .06  01-20-89        lrd     fixed vx includes
 * .07  03-03-89        lrd     mods for closed loop/supervisory control
 * .08  03-17-89        lrd     add read_ao routine and call at initialization
 * .09  03-23-89        lrd     convert AB readbacks
 * .10  03-29-89        lrd     make hardware errors MAJOR
 *                              remove hw severity spec from database
 * .11  04-06-89        lrd     remove signal conversions
 * .12  05-03-89        lrd     removed process mask from arg list
 * .13  05-08-89        lrd     fixed init to unlock on return condition
 * .14  05-25-89        lrd     added rate of change add incremental/absolute
 * .15  01-31-90        lrd     add plc_flag to ab_aodriver
 * .16  03-21-90        lrd     add db_post_events for RVAL and RBV
 * .17  04-11-90        lrd     make locals static
 * .18  07-27-90        lrd     implement the output to a database record
 * .19  10-10-90	mrk	extensible record and device support
 * .20  09-25-91	jba	added breakpoint table conversion
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>

#include	<alarm.h>
#include	<dbAccess.h>
#include	<dbDefs.h>
#include	<dbFldTypes.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<link.h>
#include	<special.h>
#include	<recSup.h>
#include	<aoRecord.h>

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

struct rset aoRSET={
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

struct aodset { /* analog input dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record; /*returns: (0,2)=>(success,success no convert)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_ao;/*(0,1)=>success and */
			/*(continue, don`t continue) */
	DEVSUPFUN	special_linconv;
};


/* the following definitions must match those in choiceRec.ascii */
#define OUTPUT_FULL 		0
#define OUTPUT_INCREMENTAL	1

void alarm();
int convert();
long cvtRawToEngBpt();
long cvtEngToRawBpt();
void monitor();


static long init_record(pao)
    struct aoRecord	*pao;
{
    struct aodset *pdset;
    long	status=0;
    double	value;

    if(!(pdset = (struct aodset *)(pao->dset))) {
	recGblRecordError(S_dev_noDSET,pao,"ao: init_record");
	return(S_dev_noDSET);
    }
    /* get the initial value if dol is a constant*/
    if (pao->dol.type == CONSTANT ){
	    pao->udf = FALSE;
            pao->val = pao->dol.value.value;
    }
    /* must have write_ao function defined */
    if( (pdset->number < 6) || (pdset->write_ao ==NULL) ) {
	recGblRecordError(S_dev_missingSup,pao,"ao: init_record");
	return(S_dev_missingSup);
    }
    pao->init = TRUE;

    if( pdset->init_record ) {
        status=(*pdset->init_record)(pao,process);
        switch(status){
        case(0): /* convert */
            if (pao->linr == 0){
                pao->val = pao->rval;
                pao->udf=FALSE;
            }else if (pao->linr == 1){
                     pao->val = (pao->rval + pao->roff)*pao->eslo + pao->egul;
                     pao->udf=FALSE;
            }else{
		value=pao->rval;
                if (cvtRawToEngBpt(&value,pao->linr,pao->init,&pao->pbrk,&pao->lbrk)==0){
                     pao->val=value;
                     pao->udf=FALSE;
                }
            }
        break;
        case(2): /* no convert */
        break;
        default:
	     recGblRecordError(S_dev_badInitRet,pao,"ao: init_record");
	     return(S_dev_badInitRet);
        break;
        }
    }
    pao->pval = pao->val;
    return(0);
}

static long process(paddr)
    struct dbAddr	*paddr;
{
	struct aoRecord	*pao=(struct aoRecord *)(paddr->precord);
	struct aodset	*pdset = (struct aodset *)(pao->dset);
	long		 status=0;

	if( (pdset==NULL) || (pdset->write_ao==NULL) ) {
		pao->pact=TRUE;
		recGblRecordError(S_dev_missingSup,pao,"write_ao");
		return(S_dev_missingSup);
	}

	if(pao->pact == FALSE) { /* convert and if success write*/
		if(convert(pao)==0) status=(*pdset->write_ao)(pao);
	} else {
		status=(*pdset->write_ao)(pao);
	}
	pao->pact = TRUE;
	if(status==1) return(0);
	tsLocalTime(&pao->time);

	/* check for alarms */
	alarm(pao);


	/* check event list */
	monitor(pao);

	/* process the forward scan link record */
	if (pao->flnk.type==DB_LINK) dbScanPassive(pao->flnk.value.db_link.pdbAddr);

	pao->init=FALSE;
	pao->pact=FALSE;
	return(status);
}

static long special(paddr,after)
    struct dbAddr *paddr;
    int           after;
{
    struct aoRecord     *pao = (struct aoRecord *)(paddr->precord);
    struct aodset       *pdset = (struct aodset *) (pao->dset);
    int                 special_type = paddr->special;

    switch(special_type) {
    case(SPC_LINCONV):
        if(pdset->number<6 ) {
            recGblDbaddrError(S_db_noMod,paddr,"ao: special");
            return(S_db_noMod);
        }
	pao->init=TRUE;
        if(!(pdset->special_linconv)) return(0);
        return((*pdset->special_linconv)(pao,after));
    default:
        recGblDbaddrError(S_db_badChoice,paddr,"ao: special");
        return(S_db_badChoice);
    }
}

static long get_value(pao,pvdes)
    struct aoRecord		*pao;
    struct valueDes	*pvdes;
{
    pvdes->field_type = DBF_DOUBLE;
    pvdes->no_elements=1;
    (double *)(pvdes->pvalue) = &pao->val;
    return(0);
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct aoRecord	*pao=(struct aoRecord *)paddr->precord;

    strncpy(units,pao->egu,sizeof(pao->egu));
    return(0);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct aoRecord	*pao=(struct aoRecord *)paddr->precord;

    *precision = pao->prec;
    return(0);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble	*pgd;
{
    struct aoRecord	*pao=(struct aoRecord *)paddr->precord;

    pgd->upper_disp_limit = pao->hopr;
    pgd->lower_disp_limit = pao->lopr;
    return(0);
}

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct aoRecord	*pao=(struct aoRecord *)paddr->precord;

    pcd->upper_ctrl_limit = pao->hopr;
    pcd->lower_ctrl_limit = pao->lopr;
    return(0);
}
static long get_alarm_double(paddr,pad)
    struct dbAddr *paddr;
    struct dbr_alDouble	*pad;
{
    struct aoRecord	*pao=(struct aoRecord *)paddr->precord;

    pad->upper_alarm_limit = pao->hihi;
    pad->upper_warning_limit = pao->high;
    pad->lower_warning_limit = pao->low;
    pad->lower_alarm_limit = pao->lolo;
    return(0);
}


static void alarm(pao)
    struct aoRecord	*pao;
{
	double	ftemp;
	double	val=pao->val;

        if(pao->udf == TRUE ){
                if (pao->nsev<VALID_ALARM){
                        pao->nsta = UDF_ALARM;
                        pao->nsev = VALID_ALARM;
                }
                return;
        }

        /* if difference is not > hysterisis use lalm not val */
        ftemp = pao->lalm - pao->val;
        if(ftemp<0.0) ftemp = -ftemp;
        if (ftemp < pao->hyst) val=pao->lalm;

        /* alarm condition hihi */
        if (pao->nsev<pao->hhsv){
                if (val > pao->hihi){
                        pao->lalm = val;
                        pao->nsta = HIHI_ALARM;
                        pao->nsev = pao->hhsv;
                        return;
                }
        }

        /* alarm condition lolo */
        if (pao->nsev<pao->llsv){
                if (val < pao->lolo){
                        pao->lalm = val;
                        pao->nsta = LOLO_ALARM;
                        pao->nsev = pao->llsv;
                        return;
                }
        }

        /* alarm condition high */
        if (pao->nsev<pao->hsv){
                if (val > pao->high){
                        pao->lalm = val;
                        pao->nsta = HIGH_ALARM;
                        pao->nsev =pao->hsv;
                        return;
                }
        }

        /* alarm condition lolo */
        if (pao->nsev<pao->lsv){
                if (val < pao->low){
                        pao->lalm = val;
                        pao->nsta = LOW_ALARM;
                        pao->nsev = pao->lsv;
                        return;
                }
        }
        return;
}


static int convert(pao)
    struct aoRecord  *pao;
{
	double		value;

        if ((pao->dol.type == DB_LINK) && (pao->omsl == CLOSED_LOOP)){
		long		nRequest;
		long		options;
		short		save_pact;
		long		status;

		options=0;
		nRequest=1;
		save_pact = pao->pact;
		pao->pact = TRUE;
		/* don't allow dbputs to val field */
		pao->val=pao->pval;
                status = dbGetLink(&pao->dol.value.db_link,pao,DBR_DOUBLE,
			&value,&options,&nRequest);
		pao->pact = save_pact;
		if(status) {
			if(pao->nsev<VALID_ALARM) {
				pao->nsta = LINK_ALARM;
				pao->nsev=VALID_ALARM;
			}
			return(1);
		}
                if (pao->oif == OUTPUT_INCREMENTAL) value += pao->val;
        } else value = pao->val;

        /* check drive limits */
	if(pao->drvh > pao->drvl) {
        	if (value > pao->drvh) value = pao->drvh;
        	else if (value < pao->drvl) value = pao->drvl;
	}
	pao->val = value;
	pao->pval = value;
	pao->udf = FALSE;

	/* now set value equal to desired output value */
        /* apply the output rate of change */
        if ( pao->oroc ){/*must be defined and >0*/
		float		diff;

                diff = value - pao->oval;
                if (diff < 0){
                        if (pao->oroc < -diff) value = pao->oval - pao->oroc;
                }else if (pao->oroc < diff) value = pao->oval + pao->oroc;
        }
	pao->oval = value;

        /* convert */
        if (pao->linr == 0){
                pao->rval = value;
        } else if (pao->linr == 1){
              if (pao->eslo == 0.0) pao->rval = 0;
              else {
                   pao->rval = (value - pao->egul) / pao->eslo;
                   pao->rval -= pao->roff;
              }
        }else{
	      if(cvtEngToRawBpt(&value,pao->linr,pao->init,&pao->pbrk,&pao->lbrk)!=0){
	           if (pao->nsev<VALID_ALARM){
		        pao->nsta=SOFT_ALARM;
		        pao->nsev=VALID_ALARM;
		   }
		   return(1);
	     }
	     pao->rval=value;
        }
	return(0);
}


static void monitor(pao)
    struct aoRecord	*pao;
{
	unsigned short	monitor_mask;
	double		delta;
        short           stat,sevr,nsta,nsev;

        /* get previous stat and sevr  and new stat and sevr*/
        stat=pao->stat;
        sevr=pao->sevr;
        nsta=pao->nsta;
        nsev=pao->nsev;
        /*set current stat and sevr*/
        pao->stat = nsta;
        pao->sevr = nsev;
        pao->nsta = 0;
        pao->nsev = 0;

	monitor_mask = 0;

	/* alarm condition changed this scan */
        if (stat!=nsta || sevr!=nsev) {
                /* post events for alarm condition change*/
                monitor_mask = DBE_ALARM;
                /* post stat and nsev fields */
                db_post_events(pao,&pao->stat,DBE_VALUE);
                db_post_events(pao,&pao->sevr,DBE_VALUE);
        }
        /* check for value change */
        delta = pao->mlst - pao->val;
        if(delta<0.0) delta = -delta;
        if (delta > pao->mdel) {
                /* post events for value change */
                monitor_mask |= DBE_VALUE;
                /* update last value monitored */
                pao->mlst = pao->val;
        }
        /* check for archive change */
        delta = pao->alst - pao->val;
        if(delta<0.0) delta = -delta;
        if (delta > pao->adel) {
                /* post events on value field for archive change */
                monitor_mask |= DBE_LOG;
                /* update last archive value monitored */
                pao->alst = pao->val;
        }


        /* send out monitors connected to the value field */
        if (monitor_mask){
                db_post_events(pao,&pao->val,monitor_mask);
	}
	if(pao->oval!=pao->val) monitor_mask |= (DBE_VALUE|DBE_LOG);
	if(monitor_mask) {
		db_post_events(pao,&pao->oval,monitor_mask);
		if(pao->oraw != pao->rval) {
                	db_post_events(pao,&pao->rval,monitor_mask|DBE_VALUE);
			pao->oraw = pao->rval;
		}
		if(pao->orbv != pao->rbv) {
                	db_post_events(pao,&pao->rbv,monitor_mask|DBE_VALUE);
			pao->orbv = pao->rbv;
		}
	}
	return;
}
