/* recCompress.c */
/* base/src/rec  $Id$ */

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
 * .11  11-11-91        jba     Moved set and reset of alarm stat and sevr to macros
 * .12  02-05-92	jba	Changed function arguments from paddr to precord 
 * .13  02-28-92        jba     Changed get_precision,get_graphic_double,get_control_double
 * .14  02-28-92	jba	ANSI C changes
 * .15  06-02-92        jba     changed graphic/control limits for hil,ilil 
 * .16  07-15-92        jba     changed VALID_ALARM to INVALID alarm
 * .17  07-16-92        jba     added invalid alarm fwd link test and chngd fwd lnk to macro
 * .18  05-09-94        jba     Fixed the updating of pcompress->inx in array_average
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>
#include	<math.h>
#include	<string.h>
#include	<stdlib.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbFldTypes.h>
#include	<errMdef.h>
#include	<special.h>
#include	<recSup.h>
#define GEN_SIZE_OFFSET
#include	<compressRecord.h>
#undef  GEN_SIZE_OFFSET

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
static long special();
#define get_value NULL
static long cvt_dbaddr();
static long get_array_info();
static long put_array_info();
static long get_units();
static long get_precision();
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double();
static long get_control_double();
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
	get_alarm_double
};

static void reset(compressRecord *pcompress)
{
    pcompress->nuse = 0;
    pcompress->off= 0;
    pcompress->inx = 0;
    pcompress->cvb = 0.0;
    pcompress->res = 0;
}

static void monitor(compressRecord *pcompress)
{
	unsigned short	monitor_mask;

        monitor_mask = recGblResetAlarms(pcompress);
	monitor_mask |= (DBE_LOG|DBE_VALUE);
	if(monitor_mask) db_post_events(pcompress,pcompress->bptr,monitor_mask);
	return;
}

static void put_value(compressRecord *pcompress,double *psource, long n)
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

static int compress_array(compressRecord *pcompress,
	double *psource,long no_elements)
{
	long		i,j;
	long		nnew;
	long		nsam=pcompress->nsam;
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
	case (compressALG_N_to_1_Low_Value):
	    /* compress N to 1 keeping the lowest value */
	    for (i = 0; i < nnew; i++){
		value = *psource++;
		for (j = 1; j < n; j++, psource++){
		    if (value > *psource) value = *psource;
		}
		put_value(pcompress,&value,1);
	    }
	    break;
	case (compressALG_N_to_1_High_Value):
	    /* compress N to 1 keeping the highest value */
	    for (i = 0; i < nnew; i++){
		value = *psource++;
		for (j = 1; j < n; j++, psource++){
		    if (value < *psource) value = *psource;
		}
		put_value(pcompress,&value,1);
	    }
	    break;
	case (compressALG_N_to_1_Average):
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

static int array_average(compressRecord	*pcompress,
	double *psource,long	no_elements)
{
	long	i;
	long	nnow;
	long	nsam=pcompress->nsam;
	double	*psum;
	double	multiplier;
	long	inx=pcompress->inx;
	long	nuse,n;

	nuse = nsam;
	if(nuse>no_elements) nuse = no_elements;
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
	if (inx<n) {
		pcompress->inx = inx;
		return(1);
	}
	if(n>1) {
		psum = (double *)pcompress->sptr;
		multiplier = 1.0/((double)n);
		for (i = 0; i < nuse; i++, psum++)
	    		*psum = *psum * multiplier;
	}
	put_value(pcompress,pcompress->sptr,nuse);
	pcompress->inx = 0;
	return(0);
}

static int compress_scalar(struct compressRecord *pcompress,double *psource)
{
	double	value = *psource;
	double	*pdest=&pcompress->cvb;
	long	inx = pcompress->inx;

	/* compress according to specified algorithm */
	switch (pcompress->alg){
	case (compressALG_N_to_1_Low_Value):
	    if ((value < *pdest) || (inx == 0))
		*pdest = value;
	    break;
	case (compressALG_N_to_1_High_Value):
	    if ((value > *pdest) || (inx == 0))
		*pdest = value;
	    break;
	case (compressALG_N_to_1_Average):
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

/*Beginning of record support routines*/
static long init_record(pcompress,pass)
    compressRecord	*pcompress;
    int pass;
{
    if (pass==0){
	if(pcompress->nsam<1) pcompress->nsam = 1;
	pcompress->bptr = (double *)calloc(pcompress->nsam,sizeof(double));
	/* allocate memory for the summing buffer for conversions requiring it */
	if (pcompress->alg == compressALG_Average){
                pcompress->sptr = (double *)calloc(pcompress->nsam,sizeof(double));
	}
    }
    return(0);
}

static long process(pcompress)
	compressRecord     *pcompress;
{
    long	status=0;
    long	nelements = 0;
    int		alg = pcompress->alg;

    pcompress->pact = TRUE;
    if(!dbIsLinkConnected(&pcompress->inp)
    || !dbGetNelements(&pcompress->inp,&nelements)
    || nelements<=0) {
	recGblSetSevr(pcompress,READ_ALARM,INVALID_ALARM);
    } else {
	if(!pcompress->wptr || nelements!=pcompress->inpn) {
	    free(pcompress->wptr);
	    pcompress->wptr = (double *)dbCalloc(nelements,sizeof(double));
	    pcompress->inpn = nelements;
	    reset(pcompress);
	}
	status = dbGetLink(&pcompress->inp,DBF_DOUBLE,pcompress->wptr,0,&nelements);
	if(status || nelements<=0) {
            recGblSetSevr(pcompress,LINK_ALARM,INVALID_ALARM);
	    status = 0;
	} else {
	    if(alg==compressALG_Average) {
		status = array_average(pcompress,pcompress->wptr,nelements);
	    } else if(alg==compressALG_Circular_Buffer) {
		(void)put_value(pcompress,pcompress->wptr,nelements);
		status = 0;
	    } else if(nelements>1) {
		status = compress_array(pcompress,pcompress->wptr,nelements);
	    }else if(nelements==1){
		status = compress_scalar(pcompress,pcompress->wptr);
	    }else status=1;
	}
    }
    /* check event list */
    if(status!=1) {
		pcompress->udf=FALSE;
		recGblGetTimeStamp(pcompress);
		monitor(pcompress);
		/* process the forward scan link record */
		recGblFwdLink(pcompress);
    }
    pcompress->pact=FALSE;
    return(0);
}

static long special(paddr,after)
    struct dbAddr *paddr;
    int           after;
{
    compressRecord   *pcompress = (compressRecord *)(paddr->precord);
    int                 special_type = paddr->special;

    if(!after) return(0);
    switch(special_type) {
    case(SPC_RESET):
	reset(pcompress);
        return(0);
    default:
        recGblDbaddrError(S_db_badChoice,paddr,"compress: special");
        return(S_db_badChoice);
    }
}

static long cvt_dbaddr(paddr)
    struct dbAddr *paddr;
{
    compressRecord *pcompress=(compressRecord *)paddr->precord;

    paddr->pfield = (void *)(pcompress->bptr);
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
    compressRecord *pcompress=(compressRecord *)paddr->precord;

    *no_elements =  pcompress->nuse;
    if(pcompress->nuse==pcompress->nsam) *offset = pcompress->off;
    else *offset = 0;
    return(0);
}

static long put_array_info(paddr,nNew)
    struct dbAddr *paddr;
    long	  nNew;
{
    compressRecord *pcompress=(compressRecord *)paddr->precord;

    pcompress->off = (pcompress->off + nNew) % (pcompress->nsam);
    pcompress->nuse = (pcompress->nuse + nNew);
    if(pcompress->nuse > pcompress->nsam) pcompress->nuse = pcompress->nsam;
    return(0);
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    compressRecord *pcompress=(compressRecord *)paddr->precord;

    strncpy(units,pcompress->egu,DB_UNITS_SIZE);
    return(0);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    compressRecord	*pcompress=(compressRecord *)paddr->precord;

    *precision = pcompress->prec;
    if(paddr->pfield == (void *)pcompress->bptr) return(0);
    recGblGetPrec(paddr,precision);
    return(0);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble *pgd;
{
    compressRecord *pcompress=(compressRecord *)paddr->precord;

    if(paddr->pfield==(void *)pcompress->bptr
    || paddr->pfield==(void *)&pcompress->ihil
    || paddr->pfield==(void *)&pcompress->ilil){
        pgd->upper_disp_limit = pcompress->hopr;
        pgd->lower_disp_limit = pcompress->lopr;
    } else recGblGetGraphicDouble(paddr,pgd);
    return(0);
}

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    compressRecord *pcompress=(compressRecord *)paddr->precord;

    if(paddr->pfield==(void *)pcompress->bptr
    || paddr->pfield==(void *)&pcompress->ihil
    || paddr->pfield==(void *)&pcompress->ilil){
        pcd->upper_ctrl_limit = pcompress->hopr;
        pcd->lower_ctrl_limit = pcompress->lopr;
    } else recGblGetControlDouble(paddr,pcd);
    return(0);
}
