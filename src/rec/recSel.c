/* recSel.c */
/* share/src/rec $Id$ */

/* recSel.c - Record Support Routines for Select records */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            6-2-89
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
 * .01  11-16-89        lrd     fixed select algorithms not to compare against
 *                              the previous value
 * .02  10-12-90	mrk	changes for new record support
 * .03  11-11-91        jba     Moved set and reset of alarm stat and sevr to macros
 * .04  02-05-92	jba	Changed function arguments from paddr to precord 
 * .05  02-28-92        jba     Changed get_precision,get_graphic_double,get_control_double
 * .06  02-28-92	jba	ANSI C changes
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>
#include	<string.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbFldTypes.h>
#include	<errMdef.h>
#include	<recSup.h>
#include	<selRecord.h>

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
#define special NULL
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

struct rset selRSET={
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

#define	SEL_MAX	12
#define SELECTED	0
#define SELECT_HIGH	1
#define SELECT_LOW	2
#define SELECT_MEDIAN	3

void alarm();
void monitor();
int fetch_values();
int do_sel();

static long init_record(psel)
    struct selRecord     *psel;
{
    struct link *plink;
    int i;
    double *pvalue;

    /* get seln initial value if nvl is a constant*/
    if (psel->nvl.type == CONSTANT ) psel->seln = psel->nvl.value.value;

    plink = &psel->inpa;
    pvalue = &psel->a;
    for(i=0; i<SEL_MAX; i++, plink++, pvalue++) {
	if(plink->type==CONSTANT) *pvalue = plink->value.value;
    }
    return(0);
}

static long process(psel)
	struct selRecord	*psel;
{

	psel->pact = TRUE;
	if(fetch_values(psel)==0) {
		if(do_sel(psel)!=0) {
			recGblSetSevr(psel,CALC_ALARM,VALID_ALARM);
		}
	}

	tsLocalTime(&psel->time);
	/* check for alarms */
	alarm(psel);


	/* check event list */
	monitor(psel);

	/* process the forward scan link record */
	if (psel->flnk.type==DB_LINK)
		dbScanPassive(((struct dbAddr *)psel->flnk.value.db_link.pdbAddr)->precord);

	psel->pact=FALSE;
	return(0);
}


static long get_value(psel,pvdes)
    struct selRecord		*psel;
    struct valueDes	*pvdes;
{
    pvdes->field_type = DBF_DOUBLE;
    pvdes->no_elements=1;
    (double *)(pvdes->pvalue) = &psel->val;
    return(0);
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct selRecord	*psel=(struct selRecord *)paddr->precord;

    strncpy(units,psel->egu,sizeof(psel->egu));
    return(0);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct selRecord	*psel=(struct selRecord *)paddr->precord;
    double *pvalue,*plvalue;
    int i;

    *precision = psel->prec;
    if(paddr->pfield==(void *)&psel->val){
        return(0);
    }
    pvalue = &psel->a;
    plvalue = &psel->la;
    for(i=0; i<SEL_MAX; i++, pvalue++, plvalue++) {
        if(paddr->pfield==(void *)&pvalue
        || paddr->pfield==(void *)&plvalue){
            return(0);
        }
    }
    recGblGetPrec(paddr,precision);
    return(0);
}


static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble	*pgd;
{
    struct selRecord	*psel=(struct selRecord *)paddr->precord;
    double *pvalue,*plvalue;
    int i;

    if(paddr->pfield==(void *)&psel->val){
        pgd->upper_disp_limit = psel->hopr;
        pgd->lower_disp_limit = psel->lopr;
        return(0);
    }

    pvalue = &psel->a;
    plvalue = &psel->la;
    for(i=0; i<SEL_MAX; i++, pvalue++, plvalue++) {
        if(paddr->pfield==(void *)&pvalue
        || paddr->pfield==(void *)&plvalue){
            pgd->upper_disp_limit = psel->hopr;
            pgd->lower_disp_limit = psel->lopr;
            return(0);
        }
    }
    recGblGetGraphicDouble(paddr,pgd);
    return(0);
}

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct selRecord	*psel=(struct selRecord *)paddr->precord;
    double *pvalue;
    int i;

    if(paddr->pfield==(void *)&psel->val){
        pcd->upper_ctrl_limit = psel->hopr;
        pcd->lower_ctrl_limit = psel->lopr;
        return(0);
    }

    pvalue = &psel->a;
    for(i=0; i<SEL_MAX; i++, pvalue++) {
        if(paddr->pfield==(void *)&pvalue){
            pcd->upper_ctrl_limit = psel->hopr;
            pcd->lower_ctrl_limit = psel->lopr;
            return(0);
        }
    }
    recGblGetControlDouble(paddr,pcd);
    return(0);
}

static long get_alarm_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_alDouble	*pgd;
{
    struct selRecord	*psel=(struct selRecord *)paddr->precord;

    pgd->upper_alarm_limit = psel->hihi;
    pgd->upper_warning_limit = psel->high;
    pgd->lower_warning_limit = psel->low;
    pgd->lower_alarm_limit = psel->lolo;
    return(0);
}


static void alarm(psel)
    struct selRecord	*psel;
{
	double	ftemp;
	double	val=psel->val;

        /* undefined condition */
	if(psel->udf ==TRUE){
		if(recGblSetSevr(psel,UDF_ALARM,VALID_ALARM)) return;
        }
        /* if difference is not > hysterisis use lalm not val */
        ftemp = psel->lalm - psel->val;
        if(ftemp<0.0) ftemp = -ftemp;
        if (ftemp < psel->hyst) val=psel->lalm;

        /* alarm condition hihi */
        if (val > psel->hihi && recGblSetSevr(psel,HIHI_ALARM,psel->hhsv)){
                psel->lalm = val;
                return;
        }

        /* alarm condition lolo */
        if (val < psel->lolo && recGblSetSevr(psel,LOLO_ALARM,psel->llsv)){
                psel->lalm = val;
                return;
        }

        /* alarm condition high */
        if (val > psel->high && recGblSetSevr(psel,HIGH_ALARM,psel->hsv)){
                psel->lalm = val;
                return;
        }

        /* alarm condition low */
        if (val < psel->low && recGblSetSevr(psel,LOW_ALARM,psel->lsv)){
                psel->lalm = val;
                return;
        }
        return;
}

static void monitor(psel)
    struct selRecord	*psel;
{
	unsigned short	monitor_mask;
	double		delta;
        short           stat,sevr,nsta,nsev;
	double           *pnew;
	double           *pprev;
	int             i;

        /* get peevious stat and sevr  and new stat and sevr*/
        recGblResetSevr(psel,stat,sevr,nsta,nsev);

        monitor_mask = 0;

        /* alarm condition changed this scan */
        if (stat!=nsta || sevr!=nsev) {
                /* post events for alarm condition change*/
                monitor_mask = DBE_ALARM;
                /* post stat and nsev fields */
                db_post_events(psel,&psel->stat,DBE_VALUE);
                db_post_events(psel,&psel->sevr,DBE_VALUE);
        }
        /* check for value change */
        delta = psel->mlst - psel->val;
        if(delta<0.0) delta = -delta;
        if (delta > psel->mdel) {
                /* post events for value change */
                monitor_mask |= DBE_VALUE;
                /* update last value monitored */
                psel->mlst = psel->val;
        }
        /* check for archive change */
        delta = psel->alst - psel->val;
        if(delta<0.0) delta = -delta;
        if (delta > psel->adel) {
                /* post events on value field for archive change */
                monitor_mask |= DBE_LOG;
                /* update last archive value monitored */
                psel->alst = psel->val;
        }

        /* send out monitors connected to the value field */
        if (monitor_mask){
                db_post_events(psel,&psel->val,monitor_mask);
        	/* check all input fields for changes*/
        	for(i=0, pnew=&psel->a, pprev=&psel->la; i<SEL_MAX; i++, pnew++, pprev++) {
                	if(*pnew != *pprev) {
                        	db_post_events(psel,pnew,monitor_mask|DBE_VALUE);
                        	*pprev = *pnew;
                	}
        	}
        }
        return;
}

static int do_sel(psel)
struct selRecord *psel;  /* pointer to selection record  */
{
	double		*pvalue;
	struct link	*plink;
	double		order[SEL_MAX];
	unsigned short	order_inx;
	unsigned short	i,j;

	/* selection mechanism */
	pvalue = &psel->a;
	switch (psel->selm){
	case (SELECTED):
		psel->val = *(pvalue+psel->seln);
		break;
	case (SELECT_HIGH):
		psel->val = *pvalue;
		for (i = 0; i < SEL_MAX; i++,pvalue++){
			if (psel->val < *pvalue)
				psel->val = *pvalue;
		}
		break;
	case (SELECT_LOW):
		psel->val = *pvalue;
		for (i = 0; i < SEL_MAX; i++,pvalue++){
			if (psel->val > *pvalue)
				psel->val = *pvalue;
		}
		break;
	case (SELECT_MEDIAN):
		/* order only those fetched from another record */
		plink = &psel->inpa;
		order_inx = 0;
		for (i = 0; i < SEL_MAX; i++,pvalue++,plink++){
			if (plink->type == DB_LINK){
				j = order_inx;
				while ((order[j-1] > *pvalue) && (j > 0)){
					order[j] = order[j-1];
					j--;
				}
				order[j] = *pvalue;
				order_inx++;
			}
		}
		psel->val = order[order_inx/2];
		break;
	default:
		return(-1);
	}
	psel->udf=FALSE;
	/* initialize flag  */
	return(0);
}

/*
 * FETCH_VALUES
 *
 * fetch the values for the variables from which to select
 */
static int fetch_values(psel)
struct selRecord *psel;
{
	long		nRequest;
	long		options=0;
	struct link	*plink;
	double		*pvalue;
	int		i;
	long		status;

	plink = &psel->inpa;
	pvalue = &psel->a;
	/* If select mechanism is SELECTED only get selected input*/
	if(psel->selm == SELECTED) {
	        /* fetch the select index */
	        if(psel->nvl.type == DB_LINK ){
			options=0;
			nRequest=1;
			if(dbGetLink(&(psel->nvl.value.db_link),(struct dbCommon *)psel,DBR_USHORT,
				&(psel->seln),&options,&nRequest)!=NULL) {
		                recGblSetSevr(psel,LINK_ALARM,VALID_ALARM);
				return(-1);
			}
		}
		plink += psel->seln;
		pvalue += psel->seln;
                if(plink->type==DB_LINK) {
		        nRequest=1;
		        status=dbGetLink(&plink->value.db_link,(struct dbCommon *)psel,DBR_DOUBLE,
				pvalue,&options,&nRequest);
			if(status!=0) {
		                recGblSetSevr(psel,LINK_ALARM,VALID_ALARM);
				return(-1);
			}
                }
		return(0);
	}
	/* fetch all inputs*/
	for(i=0; i<SEL_MAX; i++, plink++, pvalue++) {
		if(plink->type==DB_LINK) {
			nRequest=1;
			status = dbGetLink(&plink->value.db_link,(struct dbCommon *)psel,DBR_DOUBLE,
				pvalue,&options,&nRequest);
			if(status!=0) {
		                recGblSetSevr(psel,LINK_ALARM,VALID_ALARM);
				return(-1);
			}
		}
	}
	return(0);
}
