/* recCompress.c */
/* share/src/rec $Id$ */

/* recCompress.c - Record Support Routines for Compression records*/
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            7-14-89 
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
 * .01  11-27-89        lrd     using one more buffer entry than allocated
 * .02  11-27-89        lrd     throttle on monitor requests
 * .03  02-27-90        lrd     handle character value links for Joerger
 *                              digitizer (convert to short array for access
 * .04  03-05-90        lrd     add averaging of entire waveforms
 * .05  04-02-90        ba/lrd  fix the joerger processing and
 *                              add get_a_byte macro
 *                              fix the determination of buff_size
 * .06  04-11-90        lrd     make locals static
 * .07  05-02-90        lrd     fix mdct so that it would remain 0 on the
 *                              pass where the monitors are sent
 * .08  05-08-90        ba      mdct is never equal to mcnt during record pro-
 *                              cessing, causing some code never to run. Fix
 *                              is to check (mdct==mcnt || mdct==0) to indicate
 *                              first time through an averaging loop.
 * .09  07-26-90        lrd     fixed the N-to-1 character compression
 *                              value was not initialized
 * .10  10-11-90	mrk	Made changes for new record support
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>
#include	<math.h>

#include	<alarm.h>
#include	<dbAccess.h>
#include	<dbDefs.h>
#include	<dbFldTypes.h>
#include	<errMdef.h>
#include	<link.h>
#include	<special.h>
#include	<recSup.h>
#include	<compressRecord.h>

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
long init_record();
long process();
long special();
long get_value();
long cvt_dbaddr();
long get_array_info();
long put_array_info();
long get_units();
long get_precision();
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
long get_graphic_double();
long get_control_double();
#define get_alarm_double NULL

struct rset compressRSET={
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

#define compressNTO1LOW	 0
#define compressNTO1HIGH 1
#define compressNTO1AVG  2
#define AVERAGE  3
#define CIRBUF   4
void alarm();
void monitor();
void put_value();
int compress_array();
int compress_scalar();
int array_average();

static long init_record(pcompress)
    struct compressRecord	*pcompress;
{

    /* This routine may get called twice. Once by cvt_dbaddr. Once by iocInit*/
    if(pcompress->bptr==NULL) {
	pcompress->bptr = (double *)calloc(pcompress->nsam,sizeof(double));
	/* allocate memory for the summing buffer for conversions requiring it */
	if (pcompress->alg == AVERAGE){
                pcompress->sptr = (double *)calloc(pcompress->nsam,sizeof(double));
	}
    }
    if(pcompress->wptr==NULL && pcompress->inp.type==DB_LINK) {
	 struct dbAddr *pdbAddr = (struct dbAddr *)(pcompress->inp.value.db_link.pdbAddr);

	 pcompress->wptr = (double *)calloc(pdbAddr->no_elements,sizeof(double));
    }
    return(0);
}


static long process(paddr)
    struct dbAddr	*paddr;
{
    struct compressRecord *pcompress=(struct compressRecord *)(paddr->precord);
    long		 status=0;

	pcompress->pact = TRUE;

	if (pcompress->inp.type != DB_LINK) {
		status=0;
	}else if (pcompress->wptr == NULL) {
		if(pcompress->nsev<VALID_ALARM) {
			pcompress->nsta = READ_ALARM;
			pcompress->nsev = VALID_ALARM;
		}
		status=0;
	} else {
		struct dbAddr	*pdbAddr =
			(struct dbAddr *)(pcompress->inp.value.db_link.pdbAddr);
		long		options=0;
		long		no_elements=pdbAddr->no_elements;
		int			alg=pcompress->alg;

		(void)dbGetLink(&pcompress->inp.value.db_link,pcompress,DBR_DOUBLE,pcompress->wptr,
				&options,&no_elements);
		if(alg==AVERAGE) {
			status = array_average(pcompress,pcompress->wptr,no_elements);
		} else if(alg==CIRBUF) {
			(void)put_value(pcompress,pcompress->wptr,no_elements);
			status = 0;
		} else if(pdbAddr->no_elements>1) {
			status = compress_array(pcompress,pcompress->wptr,no_elements);
		}else if(no_elements==1){
			status = compress_scalar(pcompress,pcompress->wptr);
		}else status=1;
	}

	/* check event list */
	if(status!=1) {
		pcompress->udf=FALSE;
		tsLocalTime(&pcompress->time);	
		monitor(pcompress);
		/* process the forward scan link record */
		if (pcompress->flnk.type==DB_LINK) dbScanPassive(pcompress->flnk.value.db_link.pdbAddr);
	}

	pcompress->pact=FALSE;
	return(0);
}


static long special(paddr,after)
    struct dbAddr *paddr;
    int           after;
{
    struct compressRecord   *pcompress = (struct compressRecord *)(paddr->precord);
    int                 special_type = paddr->special;

    if(!after) return(0);
    switch(special_type) {
    case(SPC_RESET):
	pcompress->nuse = 0;
	pcompress->off= 0;
	pcompress->inx = 0;
	pcompress->cvb = 0.0;
	pcompress->res = 0;
        return(0);
    default:
        recGblDbaddrError(S_db_badChoice,paddr,"compress: special");
        return(S_db_badChoice);
    }
}

static long get_value(pcompress,pvdes)
    struct compressRecord *pcompress;
    struct valueDes	*pvdes;
{
    pvdes->field_type = DBF_DOUBLE;
    pvdes->no_elements=pcompress->nuse;
    (double *)(pvdes->pvalue) = pcompress->bptr;
    return(0);
}

static long cvt_dbaddr(paddr)
    struct dbAddr *paddr;
{
    struct compressRecord *pcompress=(struct compressRecord *)paddr->precord;

    /* This may get called before init_record. If so just call it*/
    if(pcompress->bptr==NULL) (void)init_record(pcompress);

    paddr->pfield = (caddr_t)(pcompress->bptr);
    paddr->no_elements = pcompress->nsam;
    paddr->field_type = DBF_DOUBLE;
    paddr->field_size = sizeof(double);
    paddr->dbr_field_type = DBF_DOUBLE;
    return(0);
}

static long get_array_info(paddr,no_elements,offset)
    struct dbAddr *paddr;
    long	  *no_elements;
    long	  *offset;
{
    struct compressRecord *pcompress=(struct compressRecord *)paddr->precord;

    *no_elements =  pcompress->nuse;
    if(pcompress->nuse==pcompress->nsam) *offset = pcompress->off;
    else *offset = 0;
    return(0);
}

static long put_array_info(paddr,nNew)
    struct dbAddr *paddr;
    long	  nNew;
{
    struct compressRecord *pcompress=(struct compressRecord *)paddr->precord;

    pcompress->off = (pcompress->off + nNew) % (pcompress->nsam);
    pcompress->nuse = (pcompress->nuse + nNew);
    if(pcompress->nuse > pcompress->nsam) pcompress->nuse = pcompress->nsam;
    return(0);
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct compressRecord *pcompress=(struct compressRecord *)paddr->precord;

    strncpy(units,pcompress->egu,sizeof(pcompress->egu));
    return(0);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct compressRecord	*pcompress=(struct compressRecord *)paddr->precord;

    *precision = pcompress->prec;
    return(0);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble *pgd;
{
    struct compressRecord *pcompress=(struct compressRecord *)paddr->precord;

    pgd->upper_disp_limit = pcompress->hopr;
    pgd->lower_disp_limit = pcompress->lopr;
    return(0);
}

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct compressRecord *pcompress=(struct compressRecord *)paddr->precord;

    pcd->upper_ctrl_limit = pcompress->hopr;
    pcd->lower_ctrl_limit = pcompress->lopr;
    return(0);
}


static void monitor(pcompress)
    struct compressRecord	*pcompress;
{
	unsigned short	monitor_mask;
	short		mdct;
        short           stat,sevr,nsta,nsev;

        /* get previous stat and sevr  and new stat and sevr*/
        stat=pcompress->stat;
        sevr=pcompress->sevr;
        nsta=pcompress->nsta;
        nsev=pcompress->nsev;
        /*set current stat and sevr*/
        pcompress->stat = nsta;
        pcompress->sevr = nsev;
        pcompress->nsta = 0;
        pcompress->nsev = 0;

        monitor_mask = 0;

        /* alarm condition changed this scan */
        if (stat!=nsta || sevr!=nsev) {
                /* post events for alarm condition change*/
                monitor_mask = DBE_ALARM;
                /* post stat and nsev fields */
                db_post_events(pcompress,&pcompress->stat,DBE_VALUE);
                db_post_events(pcompress,&pcompress->sevr,DBE_VALUE);
        }
	monitor_mask |= (DBE_LOG|DBE_VALUE);
	if(monitor_mask) db_post_events(pcompress,pcompress->bptr,monitor_mask);
	return;
}

static void put_value(pcompress,psource,n)
    struct compressRecord       *pcompress;
    double			*psource;
    long			n;
{
/* treat bptr as pointer to a circular buffer*/
	double *pdest;
	long offset=pcompress->off;
	long nuse=pcompress->nuse;
	long nsam=pcompress->nsam;
	long  i;

	pdest = pcompress->bptr + offset;
	for(i=0; i<n; i++, psource++) {
		*pdest=*psource;
		offset++;
		if(offset>=nsam) {
			pdest=pcompress->bptr;
			offset=0;
		} else pdest++;
	}
	nuse = nuse+n;
	if(nuse>nsam) nuse=nsam;
	pcompress->off = offset;
	pcompress->nuse = nuse;
	return;
}

static int compress_array(pcompress,psource,no_elements)
struct compressRecord	*pcompress;
double			*psource;
long			no_elements;
{
	long		i,j;
	long		nnew;
	long		nsam=pcompress->nsam;
	double		*pdest;
	double		value;
	long		n;

	/* skip out of limit data */
	if (pcompress->ilil < pcompress->ihil){
	    while (((*psource < pcompress->ilil) || (*psource > pcompress->ihil))
		 && (no_elements > 0)){
		    no_elements--;
		    psource++;
	    }
	}
	if(pcompress->n <= 0) pcompress->n = 1;
	n = pcompress->n;
	if(no_elements<n) return(1); /*dont do anything*/

	/* determine number of samples to take */
	if (no_elements < (nsam * n)) nnew = (no_elements / n);
	else nnew = nsam;

	/* compress according to specified algorithm */
	switch (pcompress->alg){
	case (compressNTO1LOW):
	    /* compress N to 1 keeping the lowest value */
	    for (i = 0; i < nnew; i++){
		value = *psource++;
		for (j = 1; j < n; j++, psource++){
		    if (value > *psource) value = *psource;
		}
		put_value(pcompress,&value,1);
	    }
	    break;
	case (compressNTO1HIGH):
	    /* compress N to 1 keeping the highest value */
	    for (i = 0; i < nnew; i++){
		value = *psource++;
		for (j = 1; j < n; j++, psource++){
		    if (value < *psource) value = *psource;
		}
		put_value(pcompress,&value,1);
	    }
	    break;
	case (compressNTO1AVG):
	    /* compress N to 1 keeping the average value */
	    for (i = 0; i < nnew; i++){
		value = 0;
		for (j = 0; j < n; j++, psource++)
			value += *psource;
		value /= n;
		put_value(pcompress,&value,1);
	    }
	    break;
	}
	return(0);
}

static int array_average(pcompress,psource,no_elements)
struct compressRecord	*pcompress;
double			*psource;
long			no_elements;
{
	long	i;
	long	nnow;
	long	nsam=pcompress->nsam;
	double	*psum;
	double	multiplier;
	long	inx=pcompress->inx;
	struct dbAddr *pdbAddr = (struct dbAddr *)(pcompress->inp.value.db_link.pdbAddr);
	long	ninp=pdbAddr->no_elements;
	long	nuse,n;

	nuse = nsam;
	if(nuse>ninp) nuse = ninp;
	nnow=nuse;
	if(nnow>no_elements) nnow=no_elements;
	psum = (double *)pcompress->sptr;

	/* add in the new waveform */
	if (inx == 0){
		for (i = 0; i < nnow; i++)
		    *psum++ = *psource++;
		for(i=nnow; i<nuse; i++) *psum++ = 0;
	}else{
		for (i = 0; i < nnow; i++)
		    *psum++ += *psource++;
	}

	/* do we need to calculate the result */
	inx++;
	if(pcompress->n<=0)pcompress->n=1;
	n = pcompress->n;
	if(inx<n) return(1);
	if(n>1) {
		psum = (double *)pcompress->sptr;
		multiplier = 1.0/((double)n);
		for (i = 0; i < nuse; i++, psum++)
	    		*psum = *psum * multiplier;
	}
	put_value(pcompress,pcompress->sptr,nuse);
	return(0);
}

static int compress_scalar(pcompress,psource)
struct	compressRecord	*pcompress;
double			*psource;
{
	double	value = *psource;
	double	*pdest=&pcompress->cvb;
	long	inx = pcompress->inx;

	/* compress according to specified algorithm */
	switch (pcompress->alg){
	case (compressNTO1LOW):
	    if ((value < *pdest) || (inx == 0))
		*pdest = value;
	    break;
	case (compressNTO1HIGH):
	    if ((value > *pdest) || (inx == 0))
		*pdest = value;
	    break;
	case (compressNTO1AVG):
	    if (inx == 0)
		*pdest = value;
	    else {
		*pdest += value;
		if(inx+1>=(pcompress->n)) *pdest = *pdest/(inx+1);
	    }
	    break;
	}
	inx++;
	if(inx>=pcompress->n) {
		put_value(pcompress,pdest,1);
		pcompress->inx = 0;
		return(0);
	} else {
		pcompress->inx = inx;
		return(1);
	}
}
