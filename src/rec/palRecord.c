/* recPal.c */
/* base/src/rec  $Id$ */

/* recPal.c - Record Support Routines for Pal Emulation records */
/*
 *      Original Author: Matthew Stettler
 *      Date:            12-02-93
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
 * .01	04-19-94	mcn	R3.12ified
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>
#include	<string.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbEvent.h>
#include	<dbFldTypes.h>
#include	<errMdef.h>
#include	<recSup.h>
#include	<pal.h>
#include	<special.h>
#define GEN_SIZE_OFFSET
#include	<palRecord.h>
#undef  GEN_SIZE_OFFSET

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
#define special NULL
#define get_value NULL
#define cvt_dbaddr NULL
#define get_array_info NULL
#define put_array_info NULL
static long get_units();
static long get_precision();
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double NULL

struct rset palRSET={
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

static void monitor();
static int fetch_values();
struct pal * palinit();


#define ARG_MAX 12
#define OUT_MAX 12


static long init_record(ppal,pass)
    struct palRecord	*ppal;
    int pass;
{
    struct link *plink;
    int i;
    double *pvalue;

    if (pass==0) return(0);

    plink = &ppal->inpa;
    pvalue = &ppal->a;
    for(i=0; i<ARG_MAX; i++, plink++, pvalue++) {
        if (plink->type==CONSTANT) {
	    recGblInitConstantLink(plink,DBF_DOUBLE,pvalue);
        }
    }
    if ((ppal->pptr = palinit(ppal->jedf,ppal->name,&ppal->a0)) == 0)
	return -1;
    return(0);
}

static long process(ppal)
	struct palRecord     *ppal;
{
	unsigned char *temp;
	int i;
	unsigned int result = 0;

	ppal->pact = TRUE;

	if (fetch_values(ppal)==0)
		{
		if(pal(ppal->pptr,&ppal->a,12,&result))
			printf("palrec - error, pptr = %p\n",ppal->pptr);
		ppal->val = result;
                ppal->udf = FALSE;
		}
	for (i = 0, temp = &ppal->q0; i < OUT_MAX; i++, temp++)
		{
		*temp = (ppal->val>>i) & 1;
		/*printf("%x ",*temp);*/
		}
	/*printf("\n");*/
	recGblGetTimeStamp(ppal);
	/* check event list */
	monitor(ppal);
	/* process the forward scan link record */
	recGblFwdLink(ppal);
	ppal->pact = FALSE;
	return(0);
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct palRecord	*ppal=(struct palRecord *)paddr->precord;

    strncpy(units,ppal->egu,DB_UNITS_SIZE);
    return(0);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct palRecord	*ppal=(struct palRecord *)paddr->precord;

    *precision = ppal->prec;
    if(paddr->pfield == (void *)&ppal->val) return(0);
    recGblGetPrec(paddr,precision);
    return(0);
}

static void monitor(ppal)
    struct palRecord	*ppal;
{
	unsigned short	monitor_mask;
	double		*pnew;
	double		*pprev;
	int		i;
	unsigned char	*temp;

        monitor_mask = recGblResetAlarms(ppal);

        /* check for value change */
        if (ppal->val != ppal->mlst)
		{
                monitor_mask |= DBE_VALUE; /* post events for value change */
        	}

        /* send out monitors connected to the value field */
        if (monitor_mask)
		{
                db_post_events(ppal,&ppal->val,monitor_mask);
		for (i = 0, temp = &ppal->q0; i < OUT_MAX; i++, temp++)
			if (((ppal->mlst>>i) & 1) ^ *temp) /* check each bit */
                		db_post_events(ppal,temp,monitor_mask);
		ppal->mlst = ppal->val;	/* update last value monitored */
		}
	/* check all input fields for changes*/
	for(i=0, pnew=&ppal->a, pprev=&ppal->la; i<ARG_MAX; i++, pnew++, pprev++)
		if(*pnew != *pprev)
			{
			db_post_events(ppal,pnew,monitor_mask|DBE_VALUE);
			*pprev = *pnew;
			}
        return;
}

static int fetch_values(ppal)
struct palRecord *ppal;
{
	struct link	*plink;	/* structure of the link field  */
	double		*pvalue;
	long		status;
	int		i;

	for(i=0, plink=&ppal->inpa, pvalue=&ppal->a; i<ARG_MAX; i++, plink++, pvalue++) {

                status = dbGetLink(plink,DBR_DOUBLE, pvalue,0,0);
		if (!RTN_SUCCESS(status)) return(status);
	}
	return(0);
}
