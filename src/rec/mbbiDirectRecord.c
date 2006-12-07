/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Copyright (c) 2002 Southeastern Universities Research Association, as
*     Operator of Thomas Jefferson National Accelerator Facility.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* recMbbiDirect.c */
/* share/src/rec @(#)recMbbiDirect.c	1.2	1/4/94 */

/* recMbbiDirect.c - Record Support routines for mbboDirect records */
/*
 *      Original Author: Bob Dalesio and Matthew Needes 10-07-93
 *      Current Author:  Johnny Tang
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include "alarm.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "dbFldTypes.h"
#include "devSup.h"
#include "errMdef.h"
#include "menuSimm.h"
#include "recSup.h"
#include "recGbl.h"
#include "special.h"
#define GEN_SIZE_OFFSET
#include "mbbiDirectRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

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
#define get_units NULL
#define get_precision NULL
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double NULL

rset mbbiDirectRSET={
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
epicsExportAddress(rset,mbbiDirectRSET);

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

/* refreshes all the bit fields based on a hardware value
   and sends monitors if the bit's value or the record's
   severity/status have changed */
static void refresh_bits(pmbbiDirect, monitor_mask)
    struct mbbiDirectRecord	*pmbbiDirect;
    unsigned short monitor_mask;
{
   unsigned short i;
   unsigned short mask = 1;
   unsigned short momask;
   unsigned char *bit;

   bit = &(pmbbiDirect->b0);
   for (i=0; i<NUM_BITS; i++, mask = mask << 1, bit++) {
      momask = monitor_mask;
      if (pmbbiDirect->val & mask) {
         if (*bit == 0) {
            *bit = 1;
            momask |= DBE_VALUE | DBE_LOG;
         }
      } else {
         if (*bit != 0) {
            *bit = 0;
            momask |= DBE_VALUE | DBE_LOG;
         }
      }
      if (momask)
	 db_post_events(pmbbiDirect,bit,momask);
   }
}

static long init_record(pmbbiDirect,pass)
    struct mbbiDirectRecord	*pmbbiDirect;
    int pass;
{
    struct mbbidset *pdset;
    long status;

    if (pass==0) return(0);

    /* siml must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    if (pmbbiDirect->siml.type == CONSTANT) {
	recGblInitConstantLink(&pmbbiDirect->siml,DBF_USHORT,&pmbbiDirect->simm);
    }

    /* siol must be a CONSTANT or a PV_LINK or a DB_LINK */
    if (pmbbiDirect->siol.type == CONSTANT) {
	recGblInitConstantLink(&pmbbiDirect->siol,DBF_USHORT,&pmbbiDirect->sval);
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
    pmbbiDirect->mask = (1 << pmbbiDirect->nobt) - 1;

    if( pdset->init_record ) {
	if((status=(*pdset->init_record)(pmbbiDirect))) return(status);
        refresh_bits(pmbbiDirect, 0);
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
		pmbbiDirect->udf=FALSE;

	}
	else if(status == 2) status = 0;

	/* check event list */
	monitor(pmbbiDirect);

	/* process the forward scan link record */
	recGblFwdLink(pmbbiDirect);

	pmbbiDirect->pact=FALSE;
	return(status);
}

static void monitor(pmbbiDirect)
    struct mbbiDirectRecord	*pmbbiDirect;
{
	unsigned short	monitor_mask;

        monitor_mask = recGblResetAlarms(pmbbiDirect);

	/* send out bit field monitors (value change and sevr change) */
        refresh_bits(pmbbiDirect, monitor_mask);

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
                db_post_events(pmbbiDirect,&pmbbiDirect->rval,
		    monitor_mask|DBE_VALUE|DBE_LOG);
                pmbbiDirect->oraw = pmbbiDirect->rval;
        }
        return;
}

static long readValue(pmbbiDirect)
	struct mbbiDirectRecord	*pmbbiDirect;
{
	long		status;
        struct mbbidset 	*pdset = (struct mbbidset *) (pmbbiDirect->dset);

	if (pmbbiDirect->pact == TRUE){
		status=(*pdset->read_mbbi)(pmbbiDirect);
		return(status);
	}

	status=dbGetLink(&(pmbbiDirect->siml),DBR_ENUM,
		&(pmbbiDirect->simm),0,0);
	if (status)
		return(status);

	if (pmbbiDirect->simm == NO){
		status=(*pdset->read_mbbi)(pmbbiDirect);
		return(status);
	}
	if (pmbbiDirect->simm == menuSimmYES){
		status=dbGetLink(&(pmbbiDirect->siol),
				 DBR_ULONG,&(pmbbiDirect->sval),0,0);
		if (status==0){
			pmbbiDirect->val=(unsigned short)pmbbiDirect->sval;
			pmbbiDirect->udf=FALSE;
		}
		status=2;	/* don't convert */
	}
	else if (pmbbiDirect->simm == menuSimmRAW){
		status=dbGetLink(&(pmbbiDirect->siol),
				 DBR_ULONG,&(pmbbiDirect->sval),0,0);
		if (status==0){
			pmbbiDirect->rval=pmbbiDirect->sval;
			pmbbiDirect->udf=FALSE;
		}
		status=0;	/* convert since we've written RVAL */
	} else {
		status=-1;
		recGblSetSevr(pmbbiDirect,SOFT_ALARM,INVALID_ALARM);
		return(status);
	}
	recGblSetSevr(pmbbiDirect,SIMM_ALARM,pmbbiDirect->sims);

	return(status);
}
