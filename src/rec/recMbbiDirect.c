/* recMbbiDirect.c */
/* share/src/rec @(#)recMbbiDirect.c	1.2	1/4/94 */

/* recMbbiDirect.c - Record Support routines for mbboDirect records */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Matthew Needes
 *      Date:            10-07-93
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
 *     (modifications to mbbi apply, see mbbi record)
 *   1.   mcn    "Created" by borrowing mbbi record code, and modifying it.
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
#include	<devSup.h>
#include	<errMdef.h>
#include	<recSup.h>
#include	<special.h>
#include	<mbbiDirectRecord.h>

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
#define get_units NULL
#define get_precision NULL
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double NULL

struct rset mbbiDirectRSET={
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

struct mbbidset { /* multi bit binary input dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record; /*returns: (-1,0)=>(failure,success)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_mbbi;/*(0,2)=>(success, success no convert)*/
};

static void refresh_bits();
static void monitor();
static long readValue();

#define NUM_BITS  16

/* refreshes all the bit fields based on a hardware value */
static void refresh_bits(pmbbiDirect)
    struct mbbiDirectRecord	*pmbbiDirect;
{
   int i, mask = 1;
   char *bit;

   bit = &(pmbbiDirect->b0);
   for (i=0; i<NUM_BITS; i++, mask = mask << 1, bit++) {
      if (pmbbiDirect->val & mask) {
         if (*bit == 0) {
            *bit = 1;
            db_post_events(pmbbiDirect,bit,DBE_VALUE);
         }
      }
      else {
         if (*bit != 0) {
            *bit = 0;
            db_post_events(pmbbiDirect,bit,DBE_VALUE);
         }
      }
   }
}

static long init_record(pmbbiDirect,pass)
    struct mbbiDirectRecord	*pmbbiDirect;
    int pass;
{
    struct mbbidset *pdset;
    long status;
    int i;

    if (pass==0) return(0);

    /* mbbi.siml must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    switch (pmbbiDirect->siml.type) {
    case (CONSTANT) :
        pmbbiDirect->simm = pmbbiDirect->siml.value.value;
        break;
    case (PV_LINK) :
        status = dbCaAddInlink(&(pmbbiDirect->siml), (void *) pmbbiDirect, "SIMM");
	if(status) return(status);
	break;
    case (DB_LINK) :
        break;
    default :
        recGblRecordError(S_db_badField,(void *)pmbbiDirect,
                "mbbiDirect: init_record Illegal SIML field");
        return(S_db_badField);
    }

    /* mbbi.siol must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    switch (pmbbiDirect->siol.type) {
    case (CONSTANT) :
        pmbbiDirect->sval = pmbbiDirect->siol.value.value;
        break;
    case (PV_LINK) :
        status = dbCaAddInlink(&(pmbbiDirect->siol), (void *) pmbbiDirect, "SVAL");
	if(status) return(status);
	break;
    case (DB_LINK) :
        break;
    default :
        recGblRecordError(S_db_badField,(void *)pmbbiDirect,
                "mbbiDirect: init_record Illegal SIOL field");
        return(S_db_badField);
    }

    if(!(pdset = (struct mbbidset *)(pmbbiDirect->dset))) {
	recGblRecordError(S_dev_noDSET,(void *)pmbbiDirect,"mbbiDirect: init_record");
	return(S_dev_noDSET);
    }
    /* must have read_mbbi function defined */
    if( (pdset->number < 5) || (pdset->read_mbbi == NULL) ) {
	recGblRecordError(S_dev_missingSup,(void *)pmbbiDirect,"mbbiDirect: init_record");
	return(S_dev_missingSup);
    }
    /* initialize mask*/
    pmbbiDirect->mask = 0;
    for (i=0; i<pmbbiDirect->nobt; i++) {
	pmbbiDirect->mask <<= 1; /* shift left 1 bit*/
	pmbbiDirect->mask |= 1;  /* set low order bit*/
    }
    if( pdset->init_record ) {
	if((status=(*pdset->init_record)(pmbbiDirect))) return(status);
        refresh_bits(pmbbiDirect);
    }
    return(0);
}

static long process(pmbbiDirect)
        struct mbbiDirectRecord     *pmbbiDirect;
{
	struct mbbidset	*pdset = (struct mbbidset *)(pmbbiDirect->dset);
	long		status;
	unsigned char    pact=pmbbiDirect->pact;

	if( (pdset==NULL) || (pdset->read_mbbi==NULL) ) {
		pmbbiDirect->pact=TRUE;
		recGblRecordError(S_dev_missingSup,(void *)pmbbiDirect,"read_mbbi");
		return(S_dev_missingSup);
	}

	status=readValue(pmbbiDirect); /* read the new value */
	/* check if device support set pact */
	if ( !pact && pmbbiDirect->pact ) return(0);
	pmbbiDirect->pact = TRUE;

	recGblGetTimeStamp(pmbbiDirect);

	if(status==0) { /* convert the value */
		unsigned long rval = pmbbiDirect->rval;

		if(pmbbiDirect->shft>0) rval >>= pmbbiDirect->shft;
		pmbbiDirect->val =  (unsigned short)rval;

	}
	else if(status == 2) status = 0;

	/* check event list */
	monitor(pmbbiDirect);

	/* process the forward scan link record */
	recGblFwdLink(pmbbiDirect);

	pmbbiDirect->pact=FALSE;
	return(status);
}

static long get_value(pmbbiDirect,pvdes)
    struct mbbiDirectRecord		*pmbbiDirect;
    struct valueDes	*pvdes;
{
    pvdes->field_type = DBF_USHORT;
    pvdes->no_elements=1;
    (unsigned short *)(pvdes->pvalue) = &pmbbiDirect->val;
    return(0);
}

static void monitor(pmbbiDirect)
    struct mbbiDirectRecord	*pmbbiDirect;
{
	unsigned short	monitor_mask;

        /*
         *  Monitors for Bit Fields are done in the refresh_bits()
         *   routine.
         */

        monitor_mask = recGblResetAlarms(pmbbiDirect);
        monitor_mask |= (DBE_LOG|DBE_VALUE);
        if(monitor_mask)
           db_post_events(pmbbiDirect,pmbbiDirect->val,monitor_mask);

        /* check for value change */
        if (pmbbiDirect->mlst != pmbbiDirect->val) {
                /* post events for value change and archive change */
                monitor_mask |= (DBE_VALUE | DBE_LOG);
                /* update last value monitored */
                pmbbiDirect->mlst = pmbbiDirect->val;
        }
        /* send out monitors connected to the value field */
        if (monitor_mask){
                db_post_events(pmbbiDirect,&pmbbiDirect->val,monitor_mask);
	}
        if(pmbbiDirect->oraw!=pmbbiDirect->rval) {
                db_post_events(pmbbiDirect,&pmbbiDirect->rval,monitor_mask|DBE_VALUE);
                pmbbiDirect->oraw = pmbbiDirect->rval;
        }
        return;
}

static long readValue(pmbbiDirect)
	struct mbbiDirectRecord	*pmbbiDirect;
{
	long		status;
        struct mbbidset 	*pdset = (struct mbbidset *) (pmbbiDirect->dset);
	long            nRequest=1;
	long            options=0;

	if (pmbbiDirect->pact == TRUE){
		status=(*pdset->read_mbbi)(pmbbiDirect);
                refresh_bits(pmbbiDirect);
		return(status);
	}

	status=recGblGetLinkValue(&(pmbbiDirect->siml),
		(void *)pmbbiDirect,DBR_ENUM,&(pmbbiDirect->simm),&options,&nRequest);
	if (status)
		return(status);

	if (pmbbiDirect->simm == NO){
		status=(*pdset->read_mbbi)(pmbbiDirect);
                refresh_bits(pmbbiDirect);
		return(status);
	}
	if (pmbbiDirect->simm == YES){
		nRequest=1;
		status=recGblGetLinkValue(&(pmbbiDirect->siol),
			(void *)pmbbiDirect,DBR_USHORT,&(pmbbiDirect->sval),&options,&nRequest);
		if (status==0){
			pmbbiDirect->val=pmbbiDirect->sval;
			pmbbiDirect->udf=FALSE;
			refresh_bits(pmbbiDirect);
		}
                status=2; /* dont convert */
	} else {
		status=-1;
		recGblSetSevr(pmbbiDirect,SOFT_ALARM,INVALID_ALARM);
		return(status);
	}
        recGblSetSevr(pmbbiDirect,SIMM_ALARM,pmbbiDirect->sims);

	return(status);
}
