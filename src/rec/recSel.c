/* recSel.c */
/* share/src/rec $Id$ */

/* recSel.c - Record Support Routines for Select records
 *
 * Author: 	Bob Dalesio
 * Date:        06-02-89
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
 * .01  11-16-89        lrd     fixed select algorithms not to compare against
 *                              the previous value
 * 
 * .02  10-12-90	mrk	changes for new record support
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>

#include	<alarm.h>
#include	<dbAccess.h>
#include	<dbDefs.h>
#include	<dbFldTypes.h>
#include	<errMdef.h>
#include	<link.h>
#include	<recSup.h>
#include	<selRecord.h>

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
long get_precision();
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
long get_graphic_double();
long get_control_double();
long get_alarm_double();

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

    plink = &psel->inpa;
    pvalue = &psel->a;
    for(i=0; i<SEL_MAX; i++, plink++, pvalue++) {
	if(plink->type==CONSTANT) *pvalue = plink->value.value;
    }
    return(0);
}

static long process(paddr)
    struct dbAddr	*paddr;
{
	struct selRecord	*psel=(struct selRecord *)(paddr->precord);

	psel->pact = TRUE;
	if(fetch_values(psel)==0) {
		if(do_sel(psel)!=0) {
			if(psel->nsev<MAJOR_ALARM) {
				psel->nsta = CALC_ALARM;
				psel->nsev = MAJOR_ALARM;
			}
		}
	}

	tsLocalTime(&psel->time);
	/* check for alarms */
	alarm(psel);


	/* check event list */
	monitor(psel);

	/* process the forward scan link record */
	if (psel->flnk.type==DB_LINK)
		dbScanPassive(psel->flnk.value.db_link.pdbAddr);

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

    *precision = psel->prec;
    return(0);
}


static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble	*pgd;
{
    struct selRecord	*psel=(struct selRecord *)paddr->precord;

    pgd->upper_disp_limit = psel->hopr;
    pgd->lower_disp_limit = psel->lopr;
    return(0);
}

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct selRecord	*psel=(struct selRecord *)paddr->precord;

    pcd->upper_ctrl_limit = psel->hopr;
    pcd->lower_ctrl_limit = psel->lopr;
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

	if(val>0.0 && val<udfDtest) return;
        /* if difference is not > hysterisis use lalm not val */
        ftemp = psel->lalm - psel->val;
        if(ftemp<0.0) ftemp = -ftemp;
        if (ftemp < psel->hyst) val=psel->lalm;

        /* alarm condition hihi */
        if (psel->nsev<psel->hhsv){
                if (val > psel->hihi){
                        psel->lalm = val;
                        psel->nsta = HIHI_ALARM;
                        psel->nsev = psel->hhsv;
                        return;
                }
        }
        /* alarm condition lolo */
        if (psel->nsev<psel->llsv){
                if (val < psel->lolo){
                        psel->lalm = val;
                        psel->nsta = LOLO_ALARM;
                        psel->nsev = psel->llsv;
                        return;
                }
        }
        /* alarm condition high */
        if (psel->nsev<psel->hsv){
                if (val > psel->high){
                        psel->lalm = val;
                        psel->nsta = HIGH_ALARM;
                        psel->nsev =psel->hsv;
                        return;
                }
        }
        /* alarm condition lolo */
        if (psel->nsev<psel->lsv){
                if (val < psel->low){
                        psel->lalm = val;
                        psel->nsta = LOW_ALARM;
                        psel->nsev = psel->lsv;
                        return;
                }
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

        /* get previous stat and sevr  and new stat and sevr*/
        stat=psel->stat;
        sevr=psel->sevr;
        nsta=psel->nsta;
        nsev=psel->nsev;
        /*set current stat and sevr*/
        psel->stat = nsta;
        psel->sevr = nsev;
        psel->nsta = 0;
        psel->nsev = 0;

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
			if (*pvalue>0.0 && *pvalue<udfDtest) continue;
			if (psel->val < *pvalue)
				psel->val = *pvalue;
		}
		break;
	case (SELECT_LOW):
		psel->val = *pvalue;
		for (i = 0; i < SEL_MAX; i++,pvalue++){
			if (*pvalue>0.0 && *pvalue<udfDtest) continue;
			if (psel->val > *pvalue)
				psel->val = *pvalue;
		}
		break;
	case (SELECT_MEDIAN):
		/* order only those fetched from another record */
		plink = &psel->inpa;
		order_inx = 0;
		for (i = 0; i < SEL_MAX; i++,pvalue++,plink++){
			if (*pvalue>0.0 && *pvalue<udfDtest) continue;
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
		plink += psel->seln;
		pvalue += psel->seln;
                if(plink->type==DB_LINK) {
		        nRequest=1;
		        status=dbGetLink(&plink->value.db_link,psel,DBR_DOUBLE,pvalue,&options,&nRequest);
			if(status!=0) {
				if(psel->nsev<VALID_ALARM) {
					psel->nsev=VALID_ALARM;
					psel->nsta=LINK_ALARM;
				}
				return(-1);
			}
			if(*pvalue>0.0 && *pvalue<udfDtest) {
				if(psel->nsev<VALID_ALARM) {
					psel->nsev=VALID_ALARM;
					psel->nsta=SOFT_ALARM;
				}
				return(-1);
			}
                }
		return(0);
	}
	/* fetch all inputs*/
	for(i=0; i<SEL_MAX; i++, plink++, pvalue++) {
		if(plink->type==DB_LINK) {
			nRequest=1;
			status = dbGetLink(&plink->value.db_link,psel,DBR_DOUBLE,pvalue,&options,&nRequest);
			if(status!=0) {
				if(psel->nsev<VALID_ALARM) {
					psel->nsev=VALID_ALARM;
					psel->nsta=LINK_ALARM;
				}
				return(-1);
			}
			if(*pvalue>0.0 && *pvalue<udfDtest) {
				if(psel->nsev<VALID_ALARM) {
					psel->nsev=VALID_ALARM;
					psel->nsta=SOFT_ALARM;
				}
				return(-1);
			}
		}
	}
	return(0);
}
