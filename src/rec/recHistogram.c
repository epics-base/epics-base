/* recHistogram.c */
/* share/src/rec $Id$ */

/* recHistogram.c - Record Support Routines for Histogram records
 *
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

#include        <alarm.h>
#include        <dbAccess.h>
#include        <dbDefs.h>
#include        <dbFldTypes.h>
#include        <devSup.h>
#include        <errMdef.h>
#include        <link.h>
#include	<special.h>
#include        <recSup.h>

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
long init_record();
long process();
#define special NULL
long get_value();
#define cvt_dbaddr NULL
long get_array_info();
long put_array_info();
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

};
void monitor();


static long put_count(phistogram)
    struct histogramRecord *phistogram;
{
	if(phistogram->aelm<0 || phistogram->aelm>=nelm) {
		if (phistogram->nsev<VALID_ALARM) {
			phistogram->nsta = SOFT_ALARM;
			phistogram->nsev = VALID_ALARM;
                }
		return(0);
	}
	if(phistogram->fta == 0)  {
		if(*(phistogram->aptr+phistogram->aelm== udfUshort) {
			if (phistogram->nsev<MAJOR_ALARM) {
				phistogram->nsta = SOFT_ALARM;
				phistogram->nsev = MAJOR_ALARM;
               	 }
		return(0);
	} else {
		if(*(phistogram->aptr+phistogram->aelm==udfUlong) {
			if (phistogram->nsev<MAJOR_ALARM) {
				phistogram->nsta = SOFT_ALARM;
				phistogram->nsev = MAJOR_ALARM;
               	 }
		return(0);
	}
	*(phistogram->aptr+phistogram->aelm) ++;
	return(0);
}
static long init_histogram(phistogram)
    struct histogramRecord *phistogram;
{
	int    i;
	for (i=0;i<=phistogram->nelm;i++)
		*(phistogram->aptr + i)=0;
	return(0);
	}
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
		if(phistogram->fta == 0) {
			size=2;
			phistogram->bptr = (char *)calloc(phistogram->nelm,size);
		} else {
			if(phistogram->fta<0|| phistogram->fta>1) phistogram->fta=1;
			size=4;
			phistogram->bptr = (char *)calloc(phistogram->nelm,size);
		}
	}
	/* initialize the array */
	init_histogram(phistogram);

	/* initialize the array element number */
	if (ppid->ael.type == CONSTANT)
		ppid->aelm = ppid->ael.value.value;

	/* increment frequency in array */
	put_count(phistogram);

	return(0);
}

static long process(paddr)
    struct dbAddr	*paddr;
{
	struct histogramRecord	*phistogram=(struct histogramRecord *)(paddr->precord);
	struct histogramdset	*pdset = (struct histogramdset *)(phistogram->dset);
	long		status;
        long		optionsi=0;
	long		nRequest=1;

	/* intialize the histogram array and return when init is nonzero */
	if (phistogram->init != 0){
		init_histogram(histogram);
		tsLocalTime(&phistogram->time);
		monitor(phistogram);
		return(0);
	}

	phistogram->pact = TRUE;

        /* fetch the array element number  */
        if(phistogram->ael.type == DB_LINK){
        	if(dbGetLink(&(phistogram->ael.value.db_link),phistogram,DBR_SHORT,
		&(phistogram->aelm),&options,&nRequest)!=NULL) {
                	if (phistogram->nsev<VALID_ALARM) {
                                phistogram->stat = READ_ALARM;
                                phistogram->sevr = VALID_ALARM;
                                return(0);
                        }
                }
        }
        /* increment frequency in histogram array */
	put_count(phistogram);

	tsLocalTime(&phistogram->time);

	/* check event list */
	monitor(phistogram);

	/* process the forward scan link record */
	if (phistogram->flnk.type==DB_LINK) dbScanPassive(phistogram->flnk.value.db_link.pdbAddr);

	phistogram->pact=FALSE;
	return(status);
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

    monitor_mask |= (DBE_LOG|DBE_VALUE);
    if(monitor_mask) db_post_events(phistogram,phistogram->bptr,monitor_mask);
    return;
}

static long get_value(phistogram,pvdes)
    struct histogramRecord *phistogram;
    struct valueDes	*pvdes;
{

    pvdes->no_elements=phistogram->nelm;
    pvdes->pvalue = phistogram->bptr;
    pvdes->field_type = phistogram->ftvl;
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
    paddr->field_type = phistogram->ftvl;
    if(phistogram->ftvl==0)  paddr->field_size = MAX_STRING_SIZE;
    else paddr->field_size = sizeofTypes[phistogram->ftvl];
    paddr->dbr_field_type = phistogram->ftvl;
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

static long put_array_info(paddr,nNew)
    struct dbAddr *paddr;
    long	  nNew;
{
    struct histogramRecord *phistogram=(struct histogramRecord *)paddr->precord;

    phistogram->nelm = nNew;
    return(0);
}
