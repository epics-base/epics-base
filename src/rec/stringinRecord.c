/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* $Id$ */

/* recStringin.c - Record Support Routines for Stringin records */
/*
 *      Author: 	Janet Anderson
 *      Date:   	4/23/91
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
#include "stringinRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(stringinRecord *, int);
static long process(stringinRecord *);
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

rset stringinRSET={
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
epicsExportAddress(rset,stringinRSET);

struct stringindset { /* stringin input dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record; /*returns: (-1,0)=>(failure,success)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_stringin; /*returns: (-1,0)=>(failure,success)*/
};
static void monitor(stringinRecord *);
static long readValue(stringinRecord *);


static long init_record(stringinRecord *pstringin, int pass)
{
    struct stringindset *pdset;
    long status;

    if (pass==0) return(0);

    if (pstringin->siml.type == CONSTANT) {
	recGblInitConstantLink(&pstringin->siml,DBF_USHORT,&pstringin->simm);
    }

    /* stringin.siol must be a CONSTANT or a PV_LINK or a DB_LINK */
    if (pstringin->siol.type == CONSTANT) {
        recGblInitConstantLink(&pstringin->siol,DBF_STRING,pstringin->sval);
    } 

    if(!(pdset = (struct stringindset *)(pstringin->dset))) {
	recGblRecordError(S_dev_noDSET,(void *)pstringin,"stringin: init_record");
	return(S_dev_noDSET);
    }
    /* must have read_stringin function defined */
    if( (pdset->number < 5) || (pdset->read_stringin == NULL) ) {
	recGblRecordError(S_dev_missingSup,(void *)pstringin,"stringin: init_record");
	return(S_dev_missingSup);
    }
    if( pdset->init_record ) {
	if((status=(*pdset->init_record)(pstringin))) return(status);
    }
    return(0);
}

/*
 */
static long process(stringinRecord *pstringin)
{
	struct stringindset	*pdset = (struct stringindset *)(pstringin->dset);
	long		 status;
	unsigned char    pact=pstringin->pact;

	if( (pdset==NULL) || (pdset->read_stringin==NULL) ) {
		pstringin->pact=TRUE;
		recGblRecordError(S_dev_missingSup,(void *)pstringin,"read_stringin");
		return(S_dev_missingSup);
	}

	status=readValue(pstringin); /* read the new value */
	/* check if device support set pact */
	if ( !pact && pstringin->pact ) return(0);
	pstringin->pact = TRUE;

	recGblGetTimeStamp(pstringin);

	/* check event list */
	monitor(pstringin);
	/* process the forward scan link record */
	recGblFwdLink(pstringin);

	pstringin->pact=FALSE;
	return(status);
}

static void monitor(stringinRecord *pstringin)
{
    unsigned short  monitor_mask;

    monitor_mask = recGblResetAlarms(pstringin);
    if(strncmp(pstringin->oval,pstringin->val,sizeof(pstringin->val))) {
	monitor_mask |= DBE_VALUE|DBE_LOG;
	strncpy(pstringin->oval,pstringin->val,sizeof(pstringin->val));
    }
    if (pstringin->mpst == stringinPOST_Always)
	monitor_mask |= DBE_VALUE;
    if (pstringin->apst == stringinPOST_Always)
	monitor_mask |= DBE_LOG;
    if(monitor_mask)
	db_post_events(pstringin,&(pstringin->val[0]),monitor_mask);
    return;
}

static long readValue(stringinRecord *pstringin)
{
	long		status;
        struct stringindset 	*pdset = (struct stringindset *) (pstringin->dset);

	if (pstringin->pact == TRUE){
		status=(*pdset->read_stringin)(pstringin);
		return(status);
	}

	status=dbGetLink(&(pstringin->siml),DBR_USHORT, &(pstringin->simm),0,0);
	if (status)
		return(status);

	if (pstringin->simm == NO){
		status=(*pdset->read_stringin)(pstringin);
		return(status);
	}
	if (pstringin->simm == YES){
		status=dbGetLink(&(pstringin->siol),DBR_STRING,
			pstringin->sval,0,0);
		if (status==0) {
			strcpy(pstringin->val,pstringin->sval);
			pstringin->udf=FALSE;
		}
	} else {
		status=-1;
		recGblSetSevr(pstringin,SOFT_ALARM,INVALID_ALARM);
		return(status);
	}
        recGblSetSevr(pstringin,SIMM_ALARM,pstringin->sims);

	return(status);
}
