/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* recMbboDirect.c - Record Support for mbboDirect records */
/*
 *      Original Author: Bob Dalesio
 *      Date:            10-06-93
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
#include "recSup.h"
#include "recGbl.h"
#include "special.h"
#include "menuOmsl.h"
#include "menuIvoa.h"
#include "menuYesNo.h"

#define GEN_SIZE_OFFSET
#include "mbboDirectRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(mbboDirectRecord *, int);
static long process(mbboDirectRecord *);
static long special(DBADDR *, int);
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

rset mbboDirectRSET={
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
epicsExportAddress(rset,mbboDirectRSET);

struct mbbodset { /* multi bit binary output dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;  /*returns: (0,2)=>(success,success no convert)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_mbbo; /*returns: (0,2)=>(success,success no convert)*/
};


static void convert(mbboDirectRecord *);
static void monitor(mbboDirectRecord *);
static long writeValue(mbboDirectRecord *);

#define NUM_BITS  16

static long init_record(mbboDirectRecord *prec, int pass)
{
    struct mbbodset *pdset;
    long status = 0;
    int	i;

    if (pass==0) return(0);

    /* mbboDirect.siml must be a CONSTANT or a PV_LINK or a DB_LINK */
    if (prec->siml.type == CONSTANT) {
	recGblInitConstantLink(&prec->siml,DBF_USHORT,&prec->simm);
    }

    if(!(pdset = (struct mbbodset *)(prec->dset))) {
	recGblRecordError(S_dev_noDSET,(void *)prec,"mbboDirect: init_record");
	return(S_dev_noDSET);
    }
    /* must have write_mbbo function defined */
    if( (pdset->number < 5) || (pdset->write_mbbo == NULL) ) {
	recGblRecordError(S_dev_missingSup,(void *)prec,"mbboDirect: init_record");
	return(S_dev_missingSup);
    }
    if (prec->dol.type == CONSTANT){
	if(recGblInitConstantLink(&prec->dol,DBF_USHORT,&prec->val))
        prec->udf = FALSE;
    }
    /* initialize mask*/
    prec->mask = 0;
    for (i=0; i<prec->nobt; i++) {
        prec->mask <<= 1; /* shift left 1 bit*/
        prec->mask |= 1;  /* set low order bit*/
    }
    if(pdset->init_record) {
	epicsUInt32 rval;

	status=(*pdset->init_record)(prec);
        /* init_record might set status */
	if(status==0){
		rval = prec->rval;
		if(prec->shft>0) rval >>= prec->shft;
		prec->val =  (unsigned short)rval;
		prec->udf = FALSE;
	} else if (status == 2) status = 0;
    }
    prec->mlst = prec->val;
    prec->lalm = prec->val;
    prec->oraw = prec->rval;
    prec->orbv = prec->rbv;
    return(status);
}

static long process(mbboDirectRecord *prec)
{
    struct mbbodset	*pdset = (struct mbbodset *)(prec->dset);
    long		status=0;
    unsigned char    pact=prec->pact;

    if( (pdset==NULL) || (pdset->write_mbbo==NULL) ) {
	prec->pact=TRUE;
	recGblRecordError(S_dev_missingSup,(void *)prec,"write_mbbo");
	return(S_dev_missingSup);
    }

    if (!prec->pact) {
	if(prec->dol.type!=CONSTANT && prec->omsl==menuOmslclosed_loop){
	    long status;
	    unsigned short val;

	    status = dbGetLink(&prec->dol,DBR_USHORT,&val,0,0);
	    if(status==0) {
		prec->val= val;
		prec->udf= FALSE;
	    } 
	    else {
		recGblSetSevr(prec,LINK_ALARM,INVALID_ALARM);
		goto CONTINUE;
	    }
	}
	if(prec->udf) {
	    recGblSetSevr(prec,UDF_ALARM,INVALID_ALARM);
	    goto CONTINUE;
	}
	if(prec->nsev < INVALID_ALARM
	&& prec->sevr == INVALID_ALARM
	&& prec->omsl == menuOmslsupervisory) {
	    /* reload value field with B0 - B15 */
	    int offset = 1, i;
	    unsigned char *bit = &(prec->b0);
	    for (i=0; i<NUM_BITS; i++, offset = offset << 1, bit++) {
		if (*bit)
		    prec->val |= offset;
		else
		    prec->val &= ~offset;
	    }
	}
	/* convert val to rval */
	convert(prec);
    }

CONTINUE:
    if (prec->nsev < INVALID_ALARM )
	status=writeValue(prec); /* write the new value */
    else
	switch (prec->ivoa) {
	    case (menuIvoaContinue_normally) :
		status=writeValue(prec); /* write the new value */
		break;
	    case (menuIvoaDon_t_drive_outputs) :
		break;
	    case (menuIvoaSet_output_to_IVOV) :
		if (prec->pact == FALSE){
		    prec->val=prec->ivov;
		    convert(prec);
		}
		status=writeValue(prec); /* write the new value */
		break;
	    default :
		status=-1;
		recGblRecordError(S_db_badField,(void *)prec,
		    "mbboDirect: process Illegal IVOA field");
	}

    /* check if device support set pact */
    if ( !pact && prec->pact ) return(0);
    prec->pact = TRUE;

    recGblGetTimeStamp(prec);
    /* check event list */
    monitor(prec);
    /* process the forward scan link record */
    recGblFwdLink(prec);
    prec->pact=FALSE;
    return(status);
}

static long special(DBADDR *paddr, int after)
{
    mbboDirectRecord     *prec = (mbboDirectRecord *)(paddr->precord);
    int special_type = paddr->special, offset = 1, i;
    unsigned char *bit;

    if(!after) return(0);
    switch(special_type) {
      case(SPC_MOD):
       /*
        *  Set a bit in VAL corresponding to the bit changed
        *    offset equals the offset in bit array.  Only do
        *    this if in supervisory mode.
        */
        if (prec->omsl == menuOmslclosed_loop)
           return(0);

        offset = 1 << (((unsigned char *)paddr->pfield) - &(prec->b0));
 
        if (*((char *)paddr->pfield)) {
          /* set field */
           prec->val |= offset;
        }
        else {
           /* zero field */
           prec->val &= ~offset;
        }
	prec->udf = FALSE;
 
        convert(prec);
        return(0);
      case(SPC_RESET):
       /*
        *  If OMSL changes from closed_loop to supervisory,
        *     reload value field with B0 - B15
        */
        bit = &(prec->b0);
        if (prec->omsl == menuOmslsupervisory) {
           for (i=0; i<NUM_BITS; i++, offset = offset << 1, bit++) {
              if (*bit)
                  prec->val |= offset;
              else
                  prec->val &= ~offset;
            }
        }
	prec->udf = FALSE;

        return(0);
      default:
        recGblDbaddrError(S_db_badChoice,paddr,"mbboDirect: special");
        return(S_db_badChoice);
    }
}

static void monitor(mbboDirectRecord *prec)
{
	unsigned short	monitor_mask;

        monitor_mask = recGblResetAlarms(prec);

        /* check for value change */
        if (prec->mlst != prec->val){
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
        if(prec->orbv!=prec->rbv) {
                db_post_events(prec,&prec->rbv,
		    monitor_mask|DBE_VALUE|DBE_LOG);
                prec->orbv = prec->rbv;
        }
        return;
}

static void convert(mbboDirectRecord *prec)
{
       /* convert val to rval */
	prec->rval = (epicsUInt32)(prec->val);
	if(prec->shft>0)
             prec->rval <<= prec->shft;

	return;
}

static long writeValue(mbboDirectRecord *prec)
{
	long		status;
        struct mbbodset *pdset = (struct mbbodset *) (prec->dset);

	if (prec->pact == TRUE){
		status=(*pdset->write_mbbo)(prec);
		return(status);
	}

	status=dbGetLink(&(prec->siml),
		DBR_ENUM,&(prec->simm),0,0);
	if (status)
		return(status);

	if (prec->simm == menuYesNoNO){
		status=(*pdset->write_mbbo)(prec);
		return(status);
	}
	if (prec->simm == menuYesNoYES){
		status=dbPutLink(&prec->siol,DBR_USHORT,
			&prec->val,1);
	} else {
		status=-1;
		recGblSetSevr(prec,SOFT_ALARM,INVALID_ALARM);
		return(status);
	}
        recGblSetSevr(prec,SIMM_ALARM,prec->sims);

	return(status);
}
