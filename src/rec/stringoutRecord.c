/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* $Id$ */

/* recStringout.c - Record Support Routines for Stringout records */
/*
 * Author: 	Janet Anderson
 * Date:	4/23/91
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
#define GEN_SIZE_OFFSET
#include "stringoutRecord.h"
#undef  GEN_SIZE_OFFSET
#include "menuOmsl.h"
#include "menuIvoa.h"
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(stringoutRecord *, int);
static long process(stringoutRecord *);
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

rset stringoutRSET={
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
epicsExportAddress(rset,stringoutRSET);

struct stringoutdset { /* stringout input dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record; /*returns: (-1,0)=>(failure,success)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_stringout;/*(-1,0)=>(failure,success)*/
};
static void monitor(stringoutRecord *);
static long writeValue(stringoutRecord *);


static long init_record(stringoutRecord *pstringout, int pass)
{
    struct stringoutdset *pdset;
    long status=0;

    if (pass==0) return(0);

    if (pstringout->siml.type == CONSTANT) {
	recGblInitConstantLink(&pstringout->siml,DBF_USHORT,&pstringout->simm);
    }

    if(!(pdset = (struct stringoutdset *)(pstringout->dset))) {
	recGblRecordError(S_dev_noDSET,(void *)pstringout,"stringout: init_record");
	return(S_dev_noDSET);
    }
    /* must have  write_stringout functions defined */
    if( (pdset->number < 5) || (pdset->write_stringout == NULL) ) {
	recGblRecordError(S_dev_missingSup,(void *)pstringout,"stringout: init_record");
	return(S_dev_missingSup);
    }
    /* get the initial value dol is a constant*/
    if (pstringout->dol.type == CONSTANT){
	if(recGblInitConstantLink(&pstringout->dol,DBF_STRING,pstringout->val))
	    pstringout->udf=FALSE;
    }
    if( pdset->init_record ) {
	if((status=(*pdset->init_record)(pstringout))) return(status);
    }
    return(0);
}

static long process(stringoutRecord *pstringout)
{
	struct stringoutdset	*pdset = (struct stringoutdset *)(pstringout->dset);
	long		 status=0;
	unsigned char    pact=pstringout->pact;

	if( (pdset==NULL) || (pdset->write_stringout==NULL) ) {
		pstringout->pact=TRUE;
		recGblRecordError(S_dev_missingSup,(void *)pstringout,"write_stringout");
		return(S_dev_missingSup);
	}
        if (!pstringout->pact
        && (pstringout->dol.type != CONSTANT)
        && (pstringout->omsl == menuOmslclosed_loop)) {
		status = dbGetLink(&(pstringout->dol),
			DBR_STRING,pstringout->val,0,0);
		if(pstringout->dol.type!=CONSTANT && RTN_SUCCESS(status)) pstringout->udf=FALSE;
	}

        if(pstringout->udf == TRUE ){
                recGblSetSevr(pstringout,UDF_ALARM,INVALID_ALARM);
                goto finish;
        }

        if (pstringout->nsev < INVALID_ALARM )
                status=writeValue(pstringout); /* write the new value */
        else {
                switch (pstringout->ivoa) {
                    case (menuIvoaContinue_normally) :
                        status=writeValue(pstringout); /* write the new value */
                        break;
                    case (menuIvoaDon_t_drive_outputs) :
                        break;
                    case (menuIvoaSet_output_to_IVOV) :
                        if(pstringout->pact == FALSE){
                                strcpy(pstringout->val,pstringout->ivov);
                        }
                        status=writeValue(pstringout); /* write the new value */
                        break;
                    default :
                        status=-1;
                        recGblRecordError(S_db_badField,(void *)pstringout,
                                "stringout:process Illegal IVOA field");
                }
        }

	/* check if device support set pact */
	if ( !pact && pstringout->pact ) return(0);
finish:
	pstringout->pact = TRUE;
	recGblGetTimeStamp(pstringout);
	monitor(pstringout);
	recGblFwdLink(pstringout);
	pstringout->pact=FALSE;
	return(status);
}

static void monitor(stringoutRecord *pstringout)
{
    unsigned short  monitor_mask;

    monitor_mask = recGblResetAlarms(pstringout);
    if(strncmp(pstringout->oval,pstringout->val,sizeof(pstringout->val))) {
	monitor_mask |= DBE_VALUE|DBE_LOG;
	strncpy(pstringout->oval,pstringout->val,sizeof(pstringout->val));
    }
    if (pstringout->mpst == stringoutPOST_Always)
	monitor_mask |= DBE_VALUE;
    if (pstringout->apst == stringoutPOST_Always)
	monitor_mask |= DBE_LOG;
    if(monitor_mask)
	db_post_events(pstringout,&(pstringout->val[0]),monitor_mask);
    return;
}

static long writeValue(stringoutRecord *pstringout)
{
	long		status;
        struct stringoutdset 	*pdset = (struct stringoutdset *) (pstringout->dset);

	if (pstringout->pact == TRUE){
		status=(*pdset->write_stringout)(pstringout);
		return(status);
	}

	status=dbGetLink(&(pstringout->siml),DBR_USHORT,
		&(pstringout->simm),0,0);
	if (status)
		return(status);

	if (pstringout->simm == NO){
		status=(*pdset->write_stringout)(pstringout);
		return(status);
	}
	if (pstringout->simm == YES){
		status=dbPutLink(&pstringout->siol,DBR_STRING,
			pstringout->val,1);
	} else {
		status=-1;
		recGblSetSevr(pstringout,SOFT_ALARM,INVALID_ALARM);
		return(status);
	}
        recGblSetSevr(pstringout,SIMM_ALARM,pstringout->sims);

	return(status);
}
