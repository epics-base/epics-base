/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* $Id$ */
/*
 *      Original Author: Bob Dalesio
 *      Date:            7-14-89 
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include "alarm.h"
#include "dbStaticLib.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "dbFldTypes.h"
#include "errMdef.h"
#include "special.h"
#include "recSup.h"
#include "recGbl.h"
#define GEN_SIZE_OFFSET
#include "compressRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(compressRecord *, int);
static long process(compressRecord *);
static long special(DBADDR *, int);
#define get_value NULL
static long cvt_dbaddr(DBADDR *);
static long get_array_info(DBADDR *, long *, long *);
static long put_array_info(DBADDR *, long);
static long get_units(DBADDR *, char *);
static long get_precision(DBADDR *, long *);
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double(DBADDR *, struct dbr_grDouble *);
static long get_control_double(DBADDR *, struct dbr_ctrlDouble *);
#define get_alarm_double NULL

rset compressRSET={
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
epicsExportAddress(rset,compressRSET);

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

static void put_value(compressRecord *pcompress,double *psource, epicsInt32 n)
{
/* treat bptr as pointer to a circular buffer*/
	double *pdest;
	epicsInt32 offset=pcompress->off;
	epicsInt32 nuse=pcompress->nuse;
	epicsInt32 nsam=pcompress->nsam;
	epicsInt32 i;

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

/* qsort comparison function (for median calculation) */
static int compare(const void *arg1, const void *arg2)
{
    double a = *(double *)arg1;
    double b = *(double *)arg2;

    if      ( a <  b ) return -1;
    else if ( a == b ) return  0;
    else               return  1;
}

static int compress_array(compressRecord *pcompress,
	double *psource,epicsInt32 no_elements)
{
	epicsInt32	i,j;
	epicsInt32	nnew;
	epicsInt32	nsam=pcompress->nsam;
	double		value;
	epicsInt32	n;

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
        case (compressALG_N_to_1_Median):
            /* compress N to 1 keeping the median value */
	    /* note: sorts source array (OK; it's a work pointer) */
            for (i = 0; i < nnew; i++, psource+=nnew){
	        qsort(psource,n,sizeof(double),compare);
		value=psource[n/2];
                put_value(pcompress,&value,1);
            }
            break;
        }
	return(0);
}

static int array_average(compressRecord *pcompress,
	double *psource,epicsInt32 no_elements)
{
	epicsInt32	i;
	epicsInt32	nnow;
	epicsInt32	nsam=pcompress->nsam;
	double	*psum;
	double	multiplier;
	epicsInt32	inx=pcompress->inx;
	epicsInt32	nuse,n;

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
	epicsInt32	inx = pcompress->inx;

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
	/* for scalars, Median not implemented => use average */
	case (compressALG_N_to_1_Average):
	case (compressALG_N_to_1_Median):
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
static long init_record(compressRecord *pcompress, int pass)
{
    if (pass==0){
	if(pcompress->nsam<1) pcompress->nsam = 1;
	pcompress->bptr = (double *)calloc(pcompress->nsam,sizeof(double));
	/* allocate memory for the summing buffer for conversions requiring it */
	if (pcompress->alg == compressALG_Average){
                pcompress->sptr = (double *)calloc(pcompress->nsam,sizeof(double));
	}
        reset(pcompress);
    }
    return(0);
}

static long process(compressRecord *pcompress)
{
    long	status=0;
    long	nelements = 0;
    int		alg = pcompress->alg;

    pcompress->pact = TRUE;
    if(!dbIsLinkConnected(&pcompress->inp)
    || dbGetNelements(&pcompress->inp,&nelements)
    || nelements<=0) {
	recGblSetSevr(pcompress,LINK_ALARM,INVALID_ALARM);
    } else {
	if(!pcompress->wptr || nelements!=pcompress->inpn) {
            if(pcompress->wptr) {
                free(pcompress->wptr);
                reset(pcompress);
            }
	    pcompress->wptr = (double *)dbCalloc(nelements,sizeof(double));
	    pcompress->inpn = nelements;
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

static long special(DBADDR *paddr, int after)
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

static long cvt_dbaddr(DBADDR *paddr)
{
    compressRecord *pcompress=(compressRecord *)paddr->precord;

    paddr->pfield = (void *)(pcompress->bptr);
    paddr->no_elements = pcompress->nsam;
    paddr->field_type = DBF_DOUBLE;
    paddr->field_size = sizeof(double);
    paddr->dbr_field_type = DBF_DOUBLE;
    return(0);
}

static long get_array_info(DBADDR *paddr,long *no_elements, long *offset)
{
    compressRecord *pcompress=(compressRecord *)paddr->precord;

    *no_elements =  pcompress->nuse;
    if(pcompress->nuse==pcompress->nsam) *offset = pcompress->off;
    else *offset = 0;
    return(0);
}

static long put_array_info(DBADDR *paddr, long nNew)
{
    compressRecord *pcompress=(compressRecord *)paddr->precord;

    pcompress->off = (pcompress->off + nNew) % (pcompress->nsam);
    pcompress->nuse = (pcompress->nuse + nNew);
    if(pcompress->nuse > pcompress->nsam) pcompress->nuse = pcompress->nsam;
    return(0);
}

static long get_units(DBADDR *paddr,char *units)
{
    compressRecord *pcompress=(compressRecord *)paddr->precord;

    strncpy(units,pcompress->egu,DB_UNITS_SIZE);
    return(0);
}

static long get_precision(DBADDR *paddr, long *precision)
{
    compressRecord	*pcompress=(compressRecord *)paddr->precord;

    *precision = pcompress->prec;
    if(paddr->pfield == (void *)pcompress->bptr) return(0);
    recGblGetPrec(paddr,precision);
    return(0);
}

static long get_graphic_double(DBADDR *paddr,struct dbr_grDouble *pgd)
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

static long get_control_double(DBADDR *paddr, struct dbr_ctrlDouble *pcd)
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
