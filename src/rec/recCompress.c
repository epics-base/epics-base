
/* recCompress.c */
/* share/src/rec $Id$ */

/* recCompress.c - Record Support Routines for Compression records
 *
 * Author:      Bob Dalesio
 * Date:        7-14-89

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
#include	<cvtTable.h>
#include	<dbAccess.h>
#include	<dbDefs.h>
#include	<dbFldTypes.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<link.h>
#include	<recSup.h>
#include	<special.h>
#include	<compressRecord.h>

/* Create RSET - Record Support Entry Table*/
long report();
#define initialize NULL
long init_record();
long process();
#define special NULL
long get_precision();
long get_value();
long cvt_dbaddr();
long get_array_info();
long put_array_info();
#define get_enum_str NULL
long get_units();
long get_graphic_double();
long get_control_double();
#define get_enum_strs NULL

struct rset compressRSET={
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

static long report(fp,paddr)
    FILE	  *fp;
    struct dbAddr *paddr;
{
    struct compressRecord	*pcompress=(struct compressRecord*)(paddr->precord);

    if(recGblReportDbCommon(fp,paddr)) return(-1);
    return(0);
}

static long init_record(pcompress)
    struct compressRecord	*pcompress;
{
    long status;

    /* This routine may get called twice. Once by cvt_dbaddr. Once by iocInit*/
    /* allocate bptr and sptr */
    if(pcompress->bptr==NULL) {
	if(pcompress->nsam<=0) pcompress->nsam=1;
	pcompress->bptr = (float *)malloc(pcompress->nsam * sizeof(float));
	/* allocate memory for the summing buffer for conversions requiring it */
	if (pcompress->alg == AVERAGE){
                pcompress->sptr = (float *)malloc(pcompress->nsam * sizeof(float));
	}
    }
    /*allocate wptr*/
    if(pcompress->wptr==NULL && pcompress->inp.type==PV_LINK) {
	struct dbAddr *pdbAddr = (struct dbAddr *)(pcompress->inp.value.db_link.pdbAddr);
	 pcompress->sptr = (float *)malloc(pdbAddr->no_elements * sizeof(float));
    }
    /* initialize the monitor count */
    pcompress->mdct = pcompress->mcnt;
    return(0);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct compressRecord	*pcompress=(struct compressRecord *)paddr->precord;

    *precision = pcompress->prec;
    return(0L);
}

static long get_value(pcompress,pvdes)
    struct compressRecord		*pcompress;
    struct valueDes	*pvdes;
{
    pvdes->field_type = DBF_FLOAT;
    pvdes->no_elements=pcompress->nsam;
    (float *)(pvdes->pvalue) = pcompress->bptr;
    return(0);
}

static long cvt_dbaddr(paddr)
    struct dbAddr *paddr;
{
    struct compressRecord *pcompress=(struct compressRecord *)paddr->precord;

    /* This may get called before init_record. If so just call it*/
    if(pcompress->bptr==NULL) init_record(paddr);

    paddr->pfield = (caddr_t)(pcompress->bptr);
    paddr->no_elements = pcompress->nsam;
    paddr->field_type = DBF_FLOAT;
    paddr->field_size = sizeof(float);
    paddr->dbr_field_type = DBF_FLOAT;
    return(0);
}

static long get_array_info(paddr,no_elements,offset)
    struct dbAddr *paddr;
    long	  *no_elements;
    long	  *offset;
{
    struct compressRecord	*pcompress=(struct compressRecord *)paddr->precord;

    *no_elements =  pcompress->nuse;
    *offset = inx+1;
    return(0);
}

static long put_array_info(paddr,nNew)
    struct dbAddr *paddr;
    long	  nNew;
    long	  offset;
{
    struct compressRecord	*pcompress=(struct compressRecord *)paddr->precord;

    pcompress->inx = (pcompress->inx + nNew) % (pcompress->nsam);
    pcompress->nuse = (pcompress->nuse + nNew);
    if(pcompress->nuse > pcompress->nsam) pcompress->nuse = pcompress->nsam;
    return(0);
}


static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct compressRecord	*pcompress=(struct compressRecord *)paddr->precord;

    strncpy(units,pcompress->egu,sizeof(pcompress->egu));
    return(0L);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble *pgd;
{
    struct compressRecord     *pcompress=(struct compressRecord *)paddr->precord;

    pgd->upper_disp_limit = pcompress->hopr;
    pgd->lower_disp_limit = pcompress->lopr;
    pgd->upper_alarm_limit = 0.0;
    pgd->upper_warning_limit = 0.0;
    pgd->lower_warning_limit = 0.0;
    pgd->lower_alarm_limit = 0.0;
    return(0L);
}
static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct compressRecord     *pcompress=(struct compressRecord *)paddr->precord;

    pgd->upper_disp_limit = pcompress->hopr;
    pgd->lower_disp_limit = pcompress->lopr;
    return(0L);
}

static long process(paddr)
    struct dbAddr	*paddr;
{
    struct compressRecord	*pcompress=(struct compressRecord *)(paddr->precord);
	struct compressdset	*pdset = (struct compressdset *)(pcompress->dset);
	long		 status;

	pcompress->achn = 0;
	/* read inputs  */
	if (do_compression(pcompress) < 0){
		if (pcompress->stat != READ_ALARM){
			pcompress->stat = READ_ALARM;
			pcompress->sevr = MAJOR_ALARM;
			pcompress->achn = 1;
			monitor_compress(pcompress);
		}
		pcompress->pact = 0;
		return(0);
	}else{
		if (pcompress->stat == READ_ALARM){
			pcompress->stat = NO_ALARM;
			pcompress->sevr = NO_ALARM;
			pcompress->achn = 1;
		}
	}


	/* check event list */
	if(!pcompress->disa) status = monitor(pcompress);

	/* process the forward scan link record */
	if (pcompress->flnk.type==DB_LINK) dbScanPassive(&pcompress->flnk.value);

	pcompress->pact=FALSE;
	return(status);
}

static long monitor(pcompress)
    struct compressRecord	*pcompress;
{
	unsigned short	monitor_mask;
	float		delta;

	/* anyone wcompressting for an event on this record */
	if (pcompress->mlis.count == 0) return(0L);

	/* Flags which events to fire on the value field */
	monitor_mask = 0;

	/* alarm condition changed this scan */
	if (pcompress->achn){
		/* post events for alarm condition change and value change */
		monitor_mask = DBE_ALARM;

		/* post stat and sevr fields */
		db_post_events(pcompress,&pcompress->stat,DBE_VALUE);
		db_post_events(pcompress,&pcompress->sevr,DBE_VALUE);

		/* update last value monitored */
		pcompress->mlst = pcompress->val;
	}
	monitor_mask |= DBE_LOG|DBE_VALUE;
	db_post_events(pcompress,&pcompress->val,monitor_mask);
	return(0L);
}

static long do_compression(pcompress)
struct compress	*pcompress;
{
        struct dbAddr *pdbAddr = (struct dbAddr *)(pcompress->inp.value.db_link.pdbAddr);
	long options=0;
	long no_elements=pdbAddr->no_elements;

	if (pcompress->inp.type != DB_LINK) return(0);
	if (pcompress->wptr == NULL) return(-1);
	if(status=dbGetLink(&(pcompress->inp),DBR_FLOAT,pcompress->wprt,&options,&no_elements);

	if(pdbAddr->no_elements>1) {
		compress_array(pcompress,no_elements);
	}else if(no_elements==1){
		compress_value(pcompress);
	}
	return(0);
}

compress_array(pcompress,no_elements)
struct compress	*pcompress;
long		no_elements;
{
	long		i,j;
	int		buff_size;
	float		*psource=pcompress->wptr;
	short		*off=&(pcompress->off);
	short		nsam=pconpress->nsam;
	float		*pdest;
	float		value;

	/* for all algorithms but AVERAGE apply the front end filter */
	if (pcompress->alg != AVERAGE){
	    /* skip out of limit data */
	    if ((pcompress->ilil != 0.0) || (pcompress->ihil != 0.0)){
		while (((*psource < pcompress->ilil)
	 	 || (*psource > pcompress->ihil))
		 && (no_elements > 0)){
		    no_elements--;
		    psource++;
		}
	    }
	}

	/* determine number of samples to take */
	if ((no_elements - *off) < (pcompress->nsam * pcompress->n)){
		buff_size = (no_elements / pcompress->n);
	}else{
		buff_size = pcompress->nsam;
	}

	/* destination pointer */
	pdest = pcompress->bptr + pcompress->off;

	/* compress according to specified algorithm */
	switch (pcompress->alg){
	case (NTO1LOW):
	    /* compress N to 1 keeping the lowest value */
	    for (i = 0; i < buff_size; i++){
		value = *psource++;
		for (j = 1; j < pcompress->n; j++, psource++){
		    if (value > *psource)
			value = *psource;
		}
		*pdest = value;
		if( (*off)++ <nsam) {
		    pdest++;
		} else {
		    pdest = pcompress->bptr;
		    *off=0;
		}
	    }
	    break;
	case (NTO1HIGH):
	    /* compress N to 1 keeping the highest value */
	    for (i = 0; i < buff_size; i++){
		value = *psource++;
		for (j = 1; j < pcompress->n; j++, psource++){
		    if (value < *psource)
			value = *psource;
		}
		*pdest = value;
		if( (*off)++ <nsam) {
		    pdest++;
		} else {
		    pdest = pcompress->bptr;
		    *off=0;
		}
	    }
	    break;
	case (NTO1AVG):
	    /* compress N to 1 keeping the average value */
	    i = 0;
	    for (i = 0; i < buff_size; i++){
		value = 0;
		for (j = 0; j < pcompress->n; j++, psource++){
			value += *psource;
		}
		*pdest = value / j;
		if( (*off)++ <nsam) {
		    pdest++;
		} else {
		    pdest = pcompress->bptr;
		    *off=0;
		}
	    }
	    break;
	case (AVERAGE):
	{
	    register float	*psum;
	    register int	divider;

	    psum = (float *)pcompress->sptr;

	    /* add in the new waveform */
	    if (pcompress->mdct == pcompress->mcnt ||
		pcompress->mdct == 0){
		for (i = 0; i < buff_size; i++, psource++, psum++)
		    *psum = *psource;
	    }else{
		for (i = 0; i < buff_size; i++, psource++, psum++)
		    *psum += *psource;
	    }

	    /* do we need to calculate the result */
	    if (pcompress->mdct == 1){
		psum = (float *)pcompress->sptr;
		divider = pcompress->mcnt;
		for (i = 0; i < buff_size; i++, psum++) {
		    *pdest = *psum / divider;
		if( (*off)++ <nsam) {
		    pdest++;
		} else {
		    pdest = pcompress->bptr;
		    *off=0;
		}
		}
	    }
	    break;
	} /* end case average */
	}
    }
}

static compress_value(pcompress)
register struct	compress	*pcompress;
{
    float		value = *pcompress->wptr;;
    short		inx;


	float	*pdest;

	/* compress according to specified algorithm */
	switch (pcompress->alg){
	case (NTO1LOW):
	    pdest = pcompress->bptr + pcompress->inx;
	    if ((value < *pdest) || (pcompress->ccnt == 0))
		*pdest = value;
	    pcompress->ccnt++;
	    if (pcompress->ccnt >= pcompress->n){
		pcompress->ccnt = 0;
		if (++pcompress->inx >= pcompress->nsam)
		    pcompress->inx = 0;
	    }
	    break;
	case (NTO1HIGH):
	    pdest = pcompress->bptr + pcompress->inx;
	    if ((value > *pdest) || (pcompress->ccnt == 0))
		*pdest = value;
	    pcompress->ccnt++;
	    if (pcompress->ccnt >= pcompress->n){
		pcompress->ccnt = 0;
		if (++pcompress->inx >= pcompress->nsam)
		    pcompress->inx = 0;
	    }
	    break;
	case (NTO1AVG):
	    if (pcompress->ccnt == 0)
		pcompress->sum = value;
	    else
		pcompress->sum += value;
	    pcompress->ccnt++;
	    if (pcompress->ccnt >= pcompress->n){
		pdest=pcompress->bptr + pcompress->inx;
		*pdest = pcompress->sum / pcompress->n;
		pcompress->ccnt = 0;
		if (++pcompress->inx >= pcompress->nsam)
		    pcompress->inx = 0;
	    }
	    break;
	}
    }
}

