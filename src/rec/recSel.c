
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
#include	<cvtTable.h>
#include	<dbAccess.h>
#include	<dbDefs.h>
#include	<dbFldTypes.h>
#include	<errMdef.h>
#include	<link.h>
#include	<recSup.h>
#include	<special.h>
#include	<selRecord.h>

/* Create RSET - Record Support Entry Table*/
long report();
#define initialize NULL
long init_record();
long process();
#define special NULL
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

struct rset selRSET={
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

#define	SEL_MAX	6

static long report(fp,paddr)
    FILE	  *fp;
    struct dbAddr *paddr;
{
    struct selRecord	*psel=(struct selRecord*)(paddr->precord);

    if(recGblReportDbCommon(fp,paddr)) return(-1);
    if(fprintf(fp,"VAL  %-12.4G\n",psel->val)) return(-1);
    if(recGblReportLink(fp,"INP ",&(psel->inp))) return(-1);
    if(fprintf(fp,"PREC %d\n",psel->prec)) return(-1);
    if(recGblReportCvtChoice(fp,"LINR",psel->linr)) return(-1);
    if(fprintf(fp,"EGUF %-12.4G EGUL %-12.4G  EGU %-8s\n",
	psel->eguf,psel->egul,psel->egu)) return(-1);
    if(fprintf(fp,"HOPR %-12.4G LOPR %-12.4G\n",
	psel->hopr,psel->lopr)) return(-1);
    if(recGblReportLink(fp,"FLNK",&(psel->flnk))) return(-1);
    if(fprintf(fp,"HIHI %-12.4G HIGH %-12.4G  LOW %-12.4G LOLO %-12.4G\n",
	psel->hihi,psel->high,psel->low,psel->lolo)) return(-1);
    if(recGblReportGblChoice(fp,psel,"HHSV",psel->hhsv)) return(-1);
    if(recGblReportGblChoice(fp,psel,"HSV ",psel->hsv)) return(-1);
    if(recGblReportGblChoice(fp,psel,"LSV ",psel->lsv)) return(-1);
    if(recGblReportGblChoice(fp,psel,"LLSV",psel->llsv)) return(-1);
    if(fprintf(fp,"HYST %-12.4G ADEL %-12.4G MDEL %-12.4G ESLO %-12.4G\n",
	psel->hyst,psel->adel,psel->mdel,psel->eslo)) return(-1);
    if(fprintf(fp,"ACHN %d\n", psel->achn)) return(-1);
    if(fprintf(fp,"LALM %-12.4G ALST %-12.4G MLST %-12.4G\n",
	psel->lalm,psel->alst,psel->mlst)) return(-1);
    return(0);
}
^L
static long init_record(psel)
    struct selRecord     *psel;
{

    if(psel->inpa.type==CONSTANT) psel->a = psel->inpa.value.value;
    if(psel->inpb.type==CONSTANT) psel->b = psel->inpb.value.value;
    if(psel->inpc.type==CONSTANT) psel->c = psel->inpc.value.value;
    if(psel->inpd.type==CONSTANT) psel->d = psel->inpd.value.value;
    if(psel->inpe.type==CONSTANT) psel->e = psel->inpe.value.value;
    if(psel->inpf.type==CONSTANT) psel->f = psel->inpf.value.value;
    return(0);
}

static long process(paddr)
    struct dbAddr	*paddr;
{
    struct selRecord	*psel=(struct selRecord *)(paddr->precord);
	long		 status;

	psel->pact = TRUE;
	status=fetch_values(psel);
	if(status == -1) {
		if(psel->stat != READ_ALARM) {/* error. set alarm condition */
			psel->stat = READ_ALARM;
			psel->sevr = MAJOR_ALARM;
			psel->achn=1;
		}
	}else if(status!=0) return(status);
	else if(psel->stat == READ_ALARM || psel->stat ==  HW_LIMIT_ALARM) {
		psel->stat = NO_ALARM;
		psel->sevr = NO_ALARM;
		psel->achn=1;
	}
	if(status==0) do_select(psel);

	/* check for alarms */
	alarm(psel);


	/* check event list */
	if(!psel->disa) status = monitor(psel);

	/* process the forward scan link record */
	if (psel->flnk.type==DB_LINK) dbScanPassive(&psel->flnk.value);

	psel->pact=FALSE;
	return(status);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct selRecord	*psel=(struct selRecord *)paddr->precord;

    *precision = psel->prec;
    return(0L);
}

static long get_value(psel,pvdes)
    struct selRecord		*psel;
    struct valueDes	*pvdes;
{
    pvdes->field_type = DBF_FLOAT;
    pvdes->no_elements=1;
    (float *)(pvdes->pvalue) = &psel->val;
    return(0);
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct selRecord	*psel=(struct selRecord *)paddr->precord;

    strncpy(units,psel->egu,sizeof(psel->egu));
    return(0L);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble	*pgd;
{
    struct selRecord	*psel=(struct selRecord *)paddr->precord;

    pgd->upper_disp_limit = psel->hopr;
    pgd->lower_disp_limit = psel->lopr;
    pgd->upper_alarm_limit = psel->hihi;
    pgd->upper_warning_limit = psel->high;
    pgd->lower_warning_limit = psel->low;
    pgd->lower_alarm_limit = psel->lolo;
    return(0L);
}

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct selRecord	*psel=(struct selRecord *)paddr->precord;

    pcd->upper_ctrl_limit = psel->hopr;
    pcd->lower_ctrl_limit = psel->lopr;
    return(0L);
}

static long alarm(psel)
    struct selRecord	*psel;
{
	float	ftemp;

	/* check for a hardware alarm */
	if (psel->stat == READ_ALARM) return(0);

	/* if in alarm and difference is not > hysterisis don't bother */
	if (psel->stat != NO_ALARM){
		ftemp = psel->lalm - psel->val;
		if(ftemp<0.0) ftemp = -ftemp;
		if (ftemp < psel->hyst) return(0);
	}

	/* alarm condition hihi */
	if (psel->hhsv != NO_ALARM){
		if (psel->val > psel->hihi){
			psel->lalm = psel->val;
			if (psel->stat != HIHI_ALARM){
				psel->stat = HIHI_ALARM;
				psel->sevr = psel->hhsv;
				psel->achn = 1;
			}
			return(0);
		}
	}

	/* alarm condition lolo */
	if (psel->llsv != NO_ALARM){
		if (psel->val < psel->lolo){
			psel->lalm = psel->val;
			if (psel->stat != LOLO_ALARM){
				psel->stat = LOLO_ALARM;
				psel->sevr = psel->llsv;
				psel->achn = 1;
			}
			return(0);
		}
	}

	/* alarm condition high */
	if (psel->hsv != NO_ALARM){
		if (psel->val > psel->high){
			psel->lalm = psel->val;
			if (psel->stat != HIGH_ALARM){
				psel->stat = HIGH_ALARM;
				psel->sevr =psel->hsv;
				psel->achn = 1;
			}
			return(0);
		}
	}

	/* alarm condition lolo */
	if (psel->lsv != NO_ALARM){
		if (psel->val < psel->low){
			psel->lalm = psel->val;
			if (psel->stat != LOW_ALARM){
				psel->stat = LOW_ALARM;
				psel->sevr = psel->lsv;
				psel->achn = 1;
			}
			return(0);
		}
	}

	/* no alarm */
	if (psel->stat != NO_ALARM){
		psel->stat = NO_ALARM;
		psel->sevr = NO_ALARM;
		psel->achn = 1;
	}

	return(0);
}

static long monitor(psel)
    struct selRecord	*psel;
{
	unsigned short	monitor_mask;
	float		delta;

	/* anyone waiting for an event on this record */
	if (psel->mlis.count == 0) return(0L);

	/* Flags which events to fire on the value field */
	monitor_mask = 0;

	/* alarm condition changed this scan */
	if (psel->achn){
		/* post events for alarm condition change and value change */
		monitor_mask = DBE_ALARM | DBE_VALUE | DBE_LOG;

		/* post stat and sevr fields */
		db_post_events(psel,&psel->stat,DBE_VALUE);
		db_post_events(psel,&psel->sevr,DBE_VALUE);

		/* update last value monitored */
		psel->mlst = psel->val;

	/* check for value change */
	}else{
		delta = psel->mlst - psel->val;
		if(delta<0.0) delta = -delta;
		if (delta > psel->mdel) {
			/* post events for value change */
			monitor_mask = DBE_VALUE;

			/* update last value monitored */
			psel->mlst = psel->val;
		}
	}

	/* check for archive change */
	delta = psel->alst - psel->val;
	if(delta<0.0) delta = 0.0;
	if (delta > psel->adel) {
		/* post events on value field for archive change */
		monitor_mask |= DBE_LOG;

		/* update last archive value monitored */
		psel->alst = psel->val;
	}

	/* send out monitors connected to the value field */
	if (monitor_mask){
		db_post_events(psel,&psel->val,monitor_mask);
		db_post_events(psel,&psel->rval,monitor_mask);
	}
	return(0L);
}

static long do_sel(psel)
struct sel *psel;  /* pointer to selection record  */
{
	float		*pvalue;
	struct link	*plink;
	float		order[SEL_MAX];
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
	}

	/* initialize flag  */
	return(0);
}

/*
 * FETCH_VALUES
 *
 * fetch the values for the variables from which to select
 */
static fetch_values(psel)
register struct sel	*psel;
{
	long		nRequest;
	long		options=0;
	struct link	*plink;
	float		*pvalue;

	plink = &psel->inpa;
	pvalue = &psel->a;
	for(i=0; i<SEL_MAX; i++, plink++, pvalue++) {
		if(plink->type==DB_LINK) {
			nRequest=1;
			status=dbGetLink(plink,DBR_FLOAT,pvalue,&options,&nRequest);
			if(status!=0) return(status);
		}
	}
	return(0);
}
