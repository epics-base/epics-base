/* recHistogram.c */
/* share/src/rec $Id$ */

/* recHistogram.c - Record Support Routines for Histogram records */
/*
 *      Author: 	Janet Anderson
 *      Date:   	5/20/91
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


#include        <vxWorks.h>
#include        <types.h>
#include        <stdioLib.h>
#include        <lstLib.h>
#include	<strLib.h>
#include	<math.h>
#include	<limits.h>

#include        <alarm.h>
#include        <dbAccess.h>
#include        <dbDefs.h>
#include        <dbFldTypes.h>
#include        <devSup.h>
#include        <errMdef.h>
#include        <link.h>
#include	<special.h>
#include        <recSup.h>
#include	<histogramRecord.h>

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
long init_record();
long process();
long special();
long get_value();
long cvt_dbaddr();
long get_array_info();
#define  put_array_info NULL
#define get_units NULL
#define get_precision NULL
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double NULL

struct rset histogramRSET={
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

void monitor();


static long put_count(phistogram)
    struct histogramRecord *phistogram;
{
	double	width,temp;
	unsigned long	*pdest;
	int    i;

	if(phistogram->llim >= phistogram->ulim) {
                if (phistogram->nsev<VALID_ALARM) {
                        phistogram->stat = SOFT_ALARM;
                        phistogram->sevr = VALID_ALARM;
                        return(-1);
                }
        }
	if(phistogram->sgnl<phistogram->llim || phistogram->sgnl >= phistogram->ulim) return(0);
	width=(phistogram->ulim-phistogram->llim)/phistogram->nelm;
	temp=phistogram->sgnl-phistogram->llim;
	for (i=1;i<=phistogram->nelm;i++){
	if (temp<=(double)i*width) break;
	}
	pdest=phistogram->bptr+i-1;
	if ( *pdest==ULONG_MAX) *pdest=0.0;
	/*if ( *pdest==4294967294) *pdest=0.0;	*/
	(*pdest)++;
	phistogram->mcnt++;
	return(0);
}
static long init_histogram(phistogram)
    struct histogramRecord *phistogram;
{
	int    i;
	for (i=0;i<=phistogram->nelm-1;i++)
	      *(phistogram->bptr+i)=0.0;
	phistogram->init=0;
	phistogram->mcnt=phistogram->mdel;
	return(0);
}
static long init_record(phistogram)
    struct histogramRecord	*phistogram;
{
	struct histogramdset *pdset;
	long status;

	/* allocate space for array */
	/* This routine may get called twice. Once by cvt_dbaddr. Once by iocInit*/
	if(phistogram->bptr==NULL) {
		if(phistogram->nelm<=0) phistogram->nelm=1;
		phistogram->bptr = (unsigned long *)calloc(phistogram->nelm,sizeof(long));
	}
	/* this initialization may not be necessary*/
	/* initialize the array */
	init_histogram(phistogram);

	phistogram->udf = FALSE;

	/* initialize the array element number if CONSTANT and nonzero */
	if (phistogram->svl.type == CONSTANT && phistogram->svl.value.value != 0.){
		phistogram->sgnl = phistogram->svl.value.value;

		/* increment frequency in array */
		put_count(phistogram);
	}
	return(0);
}

static long process(paddr)
    struct dbAddr	*paddr;
{
	struct histogramRecord	*phistogram=(struct histogramRecord *)(paddr->precord);
	struct histogramdset	*pdset = (struct histogramdset *)(phistogram->dset);
	long		status=0;
        long		options=0;
	long		nRequest=1;

	phistogram->pact = TRUE;

        /* fetch the array element number  */
        if(phistogram->svl.type == DB_LINK){
        	status=dbGetLink(&(phistogram->svl.value.db_link),phistogram,DBR_DOUBLE,
		&(phistogram->sgnl),&options,&nRequest);
		if(status!=0) {
                	if (phistogram->nsev<VALID_ALARM) {
                                phistogram->stat = READ_ALARM;
                                phistogram->sevr = VALID_ALARM;
                        }
                }
        }
        /* increment frequency in histogram array */
	if(status==0) put_count(phistogram);

	tsLocalTime(&phistogram->time);

	/* check event list */
	monitor(phistogram);

	/* process the forward scan link record */
	if (phistogram->flnk.type==DB_LINK) dbScanPassive(phistogram->flnk.value.db_link.pdbAddr);

	phistogram->pact=FALSE;
	return(0);
}

static long special(paddr,after)
    struct dbAddr *paddr;
    int           after;
{
    struct histogramRecord   *phistogram = (struct histogramRecord *)(paddr->precord);
    int                 special_type = paddr->special;

    if(!after) return(0);
    switch(special_type) {
    case(SPC_RESET):
        init_histogram(phistogram);
	return(0);
    default:
        recGblDbaddrError(S_db_badChoice,paddr,"histogram: special");
        return(S_db_badChoice);
    }
}
static void monitor(phistogram)
    struct histogramRecord             *phistogram;
{
    unsigned short  monitor_mask;
    short           stat,sevr,nsta,nsev;

    /* get previous stat and sevr  and new stat and sevr*/
    stat=phistogram->stat;
    sevr=phistogram->sevr;
    nsta=phistogram->nsta;
    nsev=phistogram->nsev;
    /*set current stat and sevr*/
    phistogram->stat = nsta;
    phistogram->sevr = nsev;
    phistogram->nsta = 0;
    phistogram->nsev = 0;

    /* Flags which events to fire on the value field */
    monitor_mask = 0;

    /* alarm condition changed this scan */
    if (stat!=nsta || sevr!=nsev) {
            /* post events for alarm condition change*/
            monitor_mask = DBE_ALARM;
            /* post stat and nsev fields */
            db_post_events(phistogram,&phistogram->stat,DBE_VALUE);
            db_post_events(phistogram,&phistogram->sevr,DBE_VALUE);
    }

    /* check for count change */
    if(phistogram->mcnt - phistogram->mdel>=0){
	/* post events for count change */
	monitor_mask |= DBE_VALUE;
	/* update last value monitored */
	phistogram->mcnt = 0;
    }

    /* send out monitors connected to the value field */
    if(monitor_mask) db_post_events(phistogram,phistogram->bptr,monitor_mask);
    return;
}

static long get_value(phistogram,pvdes)
    struct histogramRecord *phistogram;
    struct valueDes	*pvdes;
{

    pvdes->no_elements=phistogram->nelm;
    (unsigned long *)(pvdes->pvalue) = phistogram->bptr;
    pvdes->field_type = DBF_ULONG;
    return(0);
}

static long cvt_dbaddr(paddr)
    struct dbAddr *paddr;
{
    struct histogramRecord *phistogram=(struct histogramRecord *)paddr->precord;

    /* This may get called before init_record. If so just call it*/
    if(phistogram->bptr==NULL) init_record(phistogram);
    paddr->pfield = (caddr_t)(phistogram->bptr);
    paddr->no_elements = phistogram->nelm;
    paddr->field_type = DBF_ULONG;
    paddr->field_size = sizeof(long);
    paddr->dbr_field_type = DBF_ULONG;
    return(0);
}

static long get_array_info(paddr,no_elements,offset)
    struct dbAddr *paddr;
    long	  *no_elements;
    long	  *offset;
{
    struct histogramRecord *phistogram=(struct histogramRecord *)paddr->precord;

    *no_elements =  phistogram->nelm;
    *offset = 0;
    return(0);
}
