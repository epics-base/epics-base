/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* recMbboDirect.c */
/* share/src/rec @(#)recMbboDirect.c	1.2	1/4/94 */

/* recMbboDirect.c - Record Support for mbboDirect records */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Matthew Needes
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
#define GEN_SIZE_OFFSET
#include "mbboDirectRecord.h"
#undef  GEN_SIZE_OFFSET
#include "menuOmsl.h"
#include "menuIvoa.h"
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
static long special();
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


static void convert();
static void monitor();
static long writeValue();

#define NUM_BITS  16

static long init_record(pmbboDirect,pass)
    struct mbboDirectRecord	*pmbboDirect;
    int pass;
{
    struct mbbodset *pdset;
    long status = 0;
    int	i;

    if (pass==0) return(0);

    /* mbboDirect.siml must be a CONSTANT or a PV_LINK or a DB_LINK */
    if (pmbboDirect->siml.type == CONSTANT) {
	recGblInitConstantLink(&pmbboDirect->siml,DBF_USHORT,&pmbboDirect->simm);
    }

    if(!(pdset = (struct mbbodset *)(pmbboDirect->dset))) {
	recGblRecordError(S_dev_noDSET,(void *)pmbboDirect,"mbboDirect: init_record");
	return(S_dev_noDSET);
    }
    /* must have write_mbbo function defined */
    if( (pdset->number < 5) || (pdset->write_mbbo == NULL) ) {
	recGblRecordError(S_dev_missingSup,(void *)pmbboDirect,"mbboDirect: init_record");
	return(S_dev_missingSup);
    }
    if (pmbboDirect->dol.type == CONSTANT){
	if(recGblInitConstantLink(&pmbboDirect->dol,DBF_USHORT,&pmbboDirect->val))
        pmbboDirect->udf = FALSE;
    }
    /* initialize mask*/
    pmbboDirect->mask = 0;
    for (i=0; i<pmbboDirect->nobt; i++) {
        pmbboDirect->mask <<= 1; /* shift left 1 bit*/
        pmbboDirect->mask |= 1;  /* set low order bit*/
    }
    if(pdset->init_record) {
	unsigned long rval;

	status=(*pdset->init_record)(pmbboDirect);
        /* init_record might set status */
	if(status==0){
		rval = pmbboDirect->rval;
		if(pmbboDirect->shft>0) rval >>= pmbboDirect->shft;
		pmbboDirect->val =  (unsigned short)rval;
		pmbboDirect->udf = FALSE;
	} else if (status == 2) status = 0;
    }
    return(status);
}

static long process(pmbboDirect)
    struct mbboDirectRecord     *pmbboDirect;
{
    struct mbbodset	*pdset = (struct mbbodset *)(pmbboDirect->dset);
    long		status=0;
    unsigned char    pact=pmbboDirect->pact;

    if( (pdset==NULL) || (pdset->write_mbbo==NULL) ) {
	pmbboDirect->pact=TRUE;
	recGblRecordError(S_dev_missingSup,(void *)pmbboDirect,"write_mbbo");
	return(S_dev_missingSup);
    }

    if (!pmbboDirect->pact) {
	if(pmbboDirect->dol.type!=CONSTANT && pmbboDirect->omsl==menuOmslclosed_loop){
	    long status;
	    unsigned short val;

	    status = dbGetLink(&pmbboDirect->dol,DBR_USHORT,&val,0,0);
	    if(status==0) {
		pmbboDirect->val= val;
		pmbboDirect->udf= FALSE;
	    } 
	    else {
		recGblSetSevr(pmbboDirect,LINK_ALARM,INVALID_ALARM);
		goto CONTINUE;
	    }
	}
	if(pmbboDirect->udf) {
	    recGblSetSevr(pmbboDirect,UDF_ALARM,INVALID_ALARM);
	    goto CONTINUE;
	}
	if(pmbboDirect->nsev < INVALID_ALARM
	&& pmbboDirect->sevr == INVALID_ALARM
	&& pmbboDirect->omsl == menuOmslsupervisory) {
	    /* reload value field with B0 - B15 */
	    int offset = 1, i;
	    unsigned char *bit = &(pmbboDirect->b0);
	    for (i=0; i<NUM_BITS; i++, offset = offset << 1, bit++) {
		if (*bit)
		    pmbboDirect->val |= offset;
		else
		    pmbboDirect->val &= ~offset;
	    }
	}
	/* convert val to rval */
	convert(pmbboDirect);
    }

CONTINUE:
    if (pmbboDirect->nsev < INVALID_ALARM )
	status=writeValue(pmbboDirect); /* write the new value */
    else
	switch (pmbboDirect->ivoa) {
	    case (menuIvoaContinue_normally) :
		status=writeValue(pmbboDirect); /* write the new value */
		break;
	    case (menuIvoaDon_t_drive_outputs) :
		break;
	    case (menuIvoaSet_output_to_IVOV) :
		if (pmbboDirect->pact == FALSE){
		    pmbboDirect->val=pmbboDirect->ivov;
		    convert(pmbboDirect);
		}
		status=writeValue(pmbboDirect); /* write the new value */
		break;
	    default :
		status=-1;
		recGblRecordError(S_db_badField,(void *)pmbboDirect,
		    "mbboDirect: process Illegal IVOA field");
	}

    /* check if device support set pact */
    if ( !pact && pmbboDirect->pact ) return(0);
    pmbboDirect->pact = TRUE;

    recGblGetTimeStamp(pmbboDirect);
    /* check event list */
    monitor(pmbboDirect);
    /* process the forward scan link record */
    recGblFwdLink(pmbboDirect);
    pmbboDirect->pact=FALSE;
    return(status);
}

static long special(paddr,after)
    struct dbAddr *paddr;
    int           after;
{
    struct mbboDirectRecord     *pmbboDirect = (struct mbboDirectRecord *)(paddr->precord);
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
        if (pmbboDirect->omsl == menuOmslclosed_loop)
           return(0);

        offset = 1 << (((unsigned char *)paddr->pfield) - &(pmbboDirect->b0));
 
        if (*((char *)paddr->pfield)) {
          /* set field */
           pmbboDirect->val |= offset;
        }
        else {
           /* zero field */
           pmbboDirect->val &= ~offset;
        }
	pmbboDirect->udf = FALSE;
 
        convert(pmbboDirect);
        return(0);
      case(SPC_RESET):
       /*
        *  If OMSL changes from closed_loop to supervisory,
        *     reload value field with B0 - B15
        */
        bit = &(pmbboDirect->b0);
        if (pmbboDirect->omsl == menuOmslsupervisory) {
           for (i=0; i<NUM_BITS; i++, offset = offset << 1, bit++) {
              if (*bit)
                  pmbboDirect->val |= offset;
              else
                  pmbboDirect->val &= ~offset;
            }
        }
	pmbboDirect->udf = FALSE;

        return(0);
      default:
        recGblDbaddrError(S_db_badChoice,paddr,"mbboDirect: special");
        return(S_db_badChoice);
    }
}

static void monitor(pmbboDirect)
    struct mbboDirectRecord	*pmbboDirect;
{
	unsigned short	monitor_mask;

        monitor_mask = recGblResetAlarms(pmbboDirect);

        /* check for value change */
        if (pmbboDirect->mlst != pmbboDirect->val){
                /* post events for value change and archive change */
                monitor_mask |= (DBE_VALUE | DBE_LOG);
                /* update last value monitored */
                pmbboDirect->mlst = pmbboDirect->val;
        }
        /* send out monitors connected to the value field */
        if (monitor_mask){
                db_post_events(pmbboDirect,&pmbboDirect->val,monitor_mask);
	}
        if(pmbboDirect->oraw!=pmbboDirect->rval) {
                db_post_events(pmbboDirect,&pmbboDirect->rval,
		    monitor_mask|DBE_VALUE|DBE_LOG);
                pmbboDirect->oraw = pmbboDirect->rval;
        }
        if(pmbboDirect->orbv!=pmbboDirect->rbv) {
                db_post_events(pmbboDirect,&pmbboDirect->rbv,
		    monitor_mask|DBE_VALUE|DBE_LOG);
                pmbboDirect->orbv = pmbboDirect->rbv;
        }
        return;
}

static void convert(pmbboDirect)
	struct mbboDirectRecord  *pmbboDirect;
{
       /* convert val to rval */
	pmbboDirect->rval = (unsigned long)(pmbboDirect->val);
	if(pmbboDirect->shft>0)
             pmbboDirect->rval <<= pmbboDirect->shft;

	return;
}

static long writeValue(pmbboDirect)
	struct mbboDirectRecord	*pmbboDirect;
{
	long		status;
        struct mbbodset *pdset = (struct mbbodset *) (pmbboDirect->dset);

	if (pmbboDirect->pact == TRUE){
		status=(*pdset->write_mbbo)(pmbboDirect);
		return(status);
	}

	status=dbGetLink(&(pmbboDirect->siml),
		DBR_ENUM,&(pmbboDirect->simm),0,0);
	if (status)
		return(status);

	if (pmbboDirect->simm == NO){
		status=(*pdset->write_mbbo)(pmbboDirect);
		return(status);
	}
	if (pmbboDirect->simm == YES){
		status=dbPutLink(&pmbboDirect->siol,DBR_USHORT,
			&pmbboDirect->val,1);
	} else {
		status=-1;
		recGblSetSevr(pmbboDirect,SOFT_ALARM,INVALID_ALARM);
		return(status);
	}
        recGblSetSevr(pmbboDirect,SIMM_ALARM,pmbboDirect->sims);

	return(status);
}
