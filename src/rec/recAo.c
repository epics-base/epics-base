/* recAo.c */
/* share/src/rec $Id$ */

/* recAo.c - Record Support Routines for Analog Output records
 *
 * Author: 	Bob Dalesio
 * Date:	7-9-87
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
long report();
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

struct rset aoRSET={
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

struct aodset { /* analog input dset */
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record; /*returns: (-1,0)=>(failure,success)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_ao;/*(0,1)=>success and */
			/*(continue, don`t continue) */
	DEVSUPFUN	special_linconv;
};

/* the following definitions must match those in choiceGbl.ascii */
#define SUPERVISORY 0
#define CLOSED_LOOP 1

/* the following definitions must match those in choiceRec.ascii */
#define OUTPUT_FULL 		0
#define OUTPUT_INCREMENTAL	1

/* The following must match the definition in choiceGbl.ascii */
#define LINEAR 1

void alarm();
void convert();
void monitor();


static long report(fp,paddr)
    FILE	  *fp;
    struct dbAddr *paddr;
{
    struct aoRecord	*pao=(struct aoRecord*)(paddr->precord);

    if(recGblReportDbCommon(fp,paddr)) return(-1);
    if(fprintf(fp,"VAL  %-12.4G OROC %-12.4G\n",pao->val,pao->oroc)) return(-1);
    if(recGblReportLink(fp,"OUT ",&(pao->out))) return(-1);
    if(fprintf(fp,"PREC %d\n",pao->prec)) return(-1);
    if(recGblReportCvtChoice(fp,"LINR",pao->linr)) return(-1);
    if(fprintf(fp,"EGUF %-12.4G EGUL %-12.4G  EGU %-8s\n",
	pao->eguf,pao->egul,pao->egu)) return(-1);
    if(fprintf(fp,"HOPR %-12.4G LOPR %-12.4G\n",
	pao->hopr,pao->lopr)) return(-1);
    if(fprintf(fp,"AOFF %-12.4G ASLO %-12.4G\n",
	pao->aoff,pao->aslo)) return(-1);
    if(recGblReportLink(fp,"FLNK",&(pao->flnk))) return(-1);
    if(fprintf(fp,"HIHI %-12.4G HIGH %-12.4G  LOW %-12.4G LOLO %-12.4G\n",
	pao->hihi,pao->high,pao->low,pao->lolo)) return(-1);
    if(recGblReportGblChoice(fp,pao,"HHSV",pao->hhsv)) return(-1);
    if(recGblReportGblChoice(fp,pao,"HSV ",pao->hsv)) return(-1);
    if(recGblReportGblChoice(fp,pao,"LSV ",pao->lsv)) return(-1);
    if(recGblReportGblChoice(fp,pao,"LLSV",pao->llsv)) return(-1);
    if(fprintf(fp,"HYST %-12.4G ADEL %-12.4G MDEL %-12.4G ESLO %-12.4G\n",
	pao->hyst,pao->adel,pao->mdel,pao->eslo)) return(-1);
    if(fprintf(fp,"RVAL 0x%-8X\n", pao->rval)) return(-1);
    if(fprintf(fp,"LALM %-12.4G ALST %-12.4G MLST %-12.4G\n",
	pao->lalm,pao->alst,pao->mlst)) return(-1);
    return(0);
}

static long init_record(pao)
    struct aoRecord	*pao;
{
    struct aodset *pdset;
    long status;

    /* initialize so that first alarm, archive, and monitor get generated*/
    pao->lalm = 1e30;
    pao->alst = 1e30;
    pao->mlst = 1e30;

    if(!(pdset = (struct aodset *)(pao->dset))) {
	recGblRecordError(S_dev_noDSET,pao,"ao: init_record");
	return(S_dev_noDSET);
    }
    /* must have write_ao function defined */
    if( (pdset->number < 6) || (pdset->write_ao ==NULL) ) {
	recGblRecordError(S_dev_missingSup,pao,"ao: init_record");
	return(S_dev_missingSup);
    }
    if( pdset->init_record ) {
	if((status=(*pdset->init_record)(pao,process))) return(status);
    }
    /* get the intial value */
    if ((pao->dol.type == CONSTANT) && (pao->dol.value.value != 0)){
            pao->val = pao->dol.value.value;
    }
    return(0);
}

static long process(paddr)
    struct dbAddr	*paddr;
{
	struct aoRecord	*pao=(struct aoRecord *)(paddr->precord);
	struct aodset	*pdset = (struct aodset *)(pao->dset);
	long		 status;

	if( (pdset==NULL) || (pdset->write_ao==NULL) ) {
		pao->pact=TRUE;
		recGblRecordError(S_dev_missingSup,pao,"write_ao");
		return(S_dev_missingSup);
	}
	if(pao->pact == 0) convert(pao);
	status=(*pdset->write_ao)(pao); /* write the new value */
	pao->pact = TRUE;

	/* status is one if an asynchronous record is being processed*/
	if(status==1) return(0);

	/* check for alarms */
	alarm(pao);


	/* check event list */
	monitor(pao);

	/* process the forward scan link record */
	if (pao->flnk.type==DB_LINK) dbScanPassive(&pao->flnk.value.db_link.pdbAddr);

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
        if(pdset->number<6 || !(pdset->special_linconv)) {
            recGblDbaddrError(S_db_noMod,paddr,"ao: special");
            return(S_db_noMod);
        }
        return((*pdset->special_linconv)(pao,after));
    default:
        recGblDbaddrError(S_db_badChoice,paddr,"ao: special");
        return(S_db_badChoice);
    }
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct aoRecord	*pao=(struct aoRecord *)paddr->precord;

    *precision = pao->prec;
    return(0);
}

static long get_value(pao,pvdes)
    struct aoRecord		*pao;
    struct valueDes	*pvdes;
{
    pvdes->field_type = DBF_FLOAT;
    pvdes->no_elements=1;
    (float *)(pvdes->pvalue) = &pao->val;
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

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble	*pgd;
{
    struct aoRecord	*pao=(struct aoRecord *)paddr->precord;

    pgd->upper_disp_limit = pao->hopr;
    pgd->lower_disp_limit = pao->lopr;
    pgd->upper_alarm_limit = pao->hihi;
    pgd->upper_warning_limit = pao->high;
    pgd->lower_warning_limit = pao->low;
    pgd->lower_alarm_limit = pao->lolo;
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

static void alarm(pao)
    struct aoRecord	*pao;
{
	float	ftemp;

        /* if difference is not > hysterisis don't bother */
        ftemp = pao->lalm - pao->val;
        if(ftemp<0.0) ftemp = -ftemp;
        if (ftemp < pao->hyst) return;

        /* alarm condition hihi */
        if (pao->nsev<pao->hhsv){
                if (pao->val > pao->hihi){
                        pao->lalm = pao->val;
                        pao->nsta = HIHI_ALARM;
                        pao->nsev = pao->hhsv;
                        return;
                }
        }

        /* alarm condition lolo */
        if (pao->nsev<pao->llsv){
                if (pao->val < pao->lolo){
                        pao->lalm = pao->val;
                        pao->nsta = LOLO_ALARM;
                        pao->nsev = pao->llsv;
                        return;
                }
        }

        /* alarm condition high */
        if (pao->nsev<pao->hsv){
                if (pao->val > pao->high){
                        pao->lalm = pao->val;
                        pao->nsta = HIGH_ALARM;
                        pao->nsev =pao->hsv;
                        return;
                }
        }

        /* alarm condition lolo */
        if (pao->nsev<pao->lsv){
                if (pao->val < pao->low){
                        pao->lalm = pao->val;
                        pao->nsta = LOW_ALARM;
                        pao->nsev = pao->lsv;
                        return;
                }
        }
        return;
}

static void monitor(pao)
    struct aoRecord	*pao;
{
	unsigned short	monitor_mask;
	float		delta;
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

	/* anyone waiting for an event on this record */
	if (pao->mlis.count == 0) return;

	/* Flags which events to fire on the value field */
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
        if(delta<0.0) delta = 0.0;
        if (delta > pao->adel) {
                /* post events on value field for archive change */
                monitor_mask |= DBE_LOG;
                /* update last archive value monitored */
                pao->alst = pao->val;
        }

        /* send out monitors connected to the value field */
        if (monitor_mask){
                db_post_events(pao,&pao->val,monitor_mask);
                db_post_events(pao,&pao->rval,monitor_mask);
		db_post_events(pao,&pao->rbv,monitor_mask);
	}
	return;
}

static void convert(pao)
struct aoRecord  *pao;
{
        /* fetch the desired output if there is a database link */
        if ((pao->dol.type == DB_LINK) && (pao->omsl == CLOSED_LOOP)){
		long		nRequest;
		long		options;
		short		save_pact;
		float		value;

		options=0;
		nRequest=1;
		save_pact = pao->pact;
		pao->pact = TRUE;
                (void)dbGetLink(&pao->dol.value,DBR_FLOAT,&value,&options,&nRequest);
                if (pao->oif == OUTPUT_FULL)
                        pao->val = value;       /* output full */
                else
                        pao->val += value;      /* output incremental */
		pao->pact = save_pact;
        }

        /* apply the output rate of change */
        if (pao->oroc){
		float		diff;

                diff = pao->val - pao->oval;
                if (diff < 0){
                        if (pao->oroc < -diff){
                                pao->oval -= pao->oroc;
                        }else{
                                pao->oval = pao->val;
                        }
                }else if (pao->oroc < diff){
                        pao->oval += pao->oroc;
                }else{
                        pao->oval = pao->val;
                }
        }else{
                pao->oval = pao->val;
        }

        /* check drive limits */
        if (pao->oval > pao->drvh) pao->oval = pao->drvh;
        else if (pao->oval < pao->drvl) pao->oval = pao->drvl;

        /* convert */
        if (pao->linr == LINEAR){
                if (pao->eslo == 0) pao->rval = 0;
                else pao->rval = (pao->oval - pao->egul) / pao->eslo;
        }else{
                pao->rval = pao->oval;
        }
}

