/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Copyright (c) 2002 Southeastern Universities Research Association, as
*     Operator of Thomas Jefferson National Accelerator Facility.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* recMbbiDirect.c - Record Support routines for mbboDirect records */
/*
 *      Original Author: Bob Dalesio and Matthew Needes 10-07-93
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
static long init_record(mbbiDirectRecord *, int);
static long process(mbbiDirectRecord *);
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

static void refresh_bits(mbbiDirectRecord *, unsigned short);
static void monitor(mbbiDirectRecord *);
static long readValue(mbbiDirectRecord *);

#define NUM_BITS  16

/* refreshes all the bit fields based on a hardware value
   and sends monitors if the bit's value or the record's
   severity/status have changed */
static void refresh_bits(mbbiDirectRecord *prec, 
			 unsigned short monitor_mask)
{
   unsigned short i;
   unsigned short mask = 1;
   unsigned short momask;
   unsigned char *bit;

   bit = &(prec->b0);
   for (i=0; i<NUM_BITS; i++, mask = mask << 1, bit++) {
      momask = monitor_mask;
      if (prec->val & mask) {
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
	 db_post_events(prec,bit,momask);
   }
}

static long init_record(mbbiDirectRecord *prec, int pass)
{
    struct mbbidset *pdset;
    long status;

    if (pass==0) return(0);

    /* siml must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    if (prec->siml.type == CONSTANT) {
	recGblInitConstantLink(&prec->siml,DBF_USHORT,&prec->simm);
    }

    /* siol must be a CONSTANT or a PV_LINK or a DB_LINK */
    if (prec->siol.type == CONSTANT) {
	recGblInitConstantLink(&prec->siol,DBF_USHORT,&prec->sval);
    }

    if(!(pdset = (struct mbbidset *)(prec->dset))) {
	recGblRecordError(S_dev_noDSET,(void *)prec,"mbbiDirect: init_record");
	return(S_dev_noDSET);
    }
    /* must have read_mbbi function defined */
    if( (pdset->number < 5) || (pdset->read_mbbi == NULL) ) {
	recGblRecordError(S_dev_missingSup,(void *)prec,"mbbiDirect: init_record");
	return(S_dev_missingSup);
    }
    /* initialize mask*/
    prec->mask = (1 << prec->nobt) - 1;

    if( pdset->init_record ) {
	if((status=(*pdset->init_record)(prec))) return(status);
        refresh_bits(prec, 0);
    }
    prec->mlst = prec->val;
    prec->lalm = prec->val;
    prec->oraw = prec->rval;
    return(0);
}

static long process(mbbiDirectRecord *prec)
{
	struct mbbidset	*pdset = (struct mbbidset *)(prec->dset);
	long		status;
	unsigned char    pact=prec->pact;

	if( (pdset==NULL) || (pdset->read_mbbi==NULL) ) {
		prec->pact=TRUE;
		recGblRecordError(S_dev_missingSup,(void *)prec,"read_mbbi");
		return(S_dev_missingSup);
	}

	status=readValue(prec); /* read the new value */
	/* check if device support set pact */
	if ( !pact && prec->pact ) return(0);
	prec->pact = TRUE;

	recGblGetTimeStamp(prec);

	if(status==0) { /* convert the value */
		epicsUInt32 rval = prec->rval;

		if(prec->shft>0) rval >>= prec->shft;
		prec->val =  (unsigned short)rval;
		prec->udf=FALSE;

	}
	else if(status == 2) status = 0;

	/* check event list */
	monitor(prec);

	/* process the forward scan link record */
	recGblFwdLink(prec);

	prec->pact=FALSE;
	return(status);
}

static void monitor(mbbiDirectRecord *prec)
{
	unsigned short	monitor_mask;

        monitor_mask = recGblResetAlarms(prec);

	/* send out bit field monitors (value change and sevr change) */
        refresh_bits(prec, monitor_mask);

        /* check for value change */
        if (prec->mlst != prec->val) {
                /* post events for value change and archive change */
                monitor_mask |= (DBE_VALUE | DBE_LOG);
                /* update last value monitored */
                prec->mlst = prec->val;
        }
        /* send out monitors connected to the value field */
        if (monitor_mask){
                db_post_events(prec,&prec->val,monitor_mask);
	}
        if(prec->oraw!=prec->rval) {
                db_post_events(prec,&prec->rval,
		    monitor_mask|DBE_VALUE|DBE_LOG);
                prec->oraw = prec->rval;
        }
        return;
}

static long readValue(mbbiDirectRecord *prec)
{
	long		status;
        struct mbbidset 	*pdset = (struct mbbidset *) (prec->dset);

	if (prec->pact == TRUE){
		status=(*pdset->read_mbbi)(prec);
		return(status);
	}

	status=dbGetLink(&(prec->siml),DBR_ENUM,
		&(prec->simm),0,0);
	if (status)
		return(status);

	if (prec->simm == menuSimmNO){
		status=(*pdset->read_mbbi)(prec);
		return(status);
	}
	if (prec->simm == menuSimmYES){
		status=dbGetLink(&(prec->siol),
				 DBR_ULONG,&(prec->sval),0,0);
		if (status==0){
			prec->val=(unsigned short)prec->sval;
			prec->udf=FALSE;
		}
		status=2;	/* don't convert */
	}
	else if (prec->simm == menuSimmRAW){
		status=dbGetLink(&(prec->siol),
				 DBR_ULONG,&(prec->sval),0,0);
		if (status==0){
			prec->rval=prec->sval;
			prec->udf=FALSE;
		}
		status=0;	/* convert since we've written RVAL */
	} else {
		status=-1;
		recGblSetSevr(prec,SOFT_ALARM,INVALID_ALARM);
		return(status);
	}
	recGblSetSevr(prec,SIMM_ALARM,prec->sims);

	return(status);
}
