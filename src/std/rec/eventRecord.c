/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* recEvent.c - Record Support Routines for Event records */
/*
 *      Author:          Janet Anderson
 *      Date:            12-13-91
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
#include "dbScan.h"
#include "dbFldTypes.h"
#include "devSup.h"
#include "errMdef.h"
#include "recSup.h"
#include "recGbl.h"
#include "special.h"
#include "menuYesNo.h"

#define GEN_SIZE_OFFSET
#include "eventRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record(struct dbCommon *, int);
static long process(struct dbCommon *);
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

rset eventRSET={
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
epicsExportAddress(rset,eventRSET);

struct eventdset { /* event input dset */
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record; /*returns: (-1,0)=>(failure,success)*/
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_event;/*(0)=> success */
};
static void monitor(eventRecord *);
static long readValue(eventRecord *);


static long init_record(struct dbCommon *pcommon, int pass)
{
    struct eventRecord *prec = (struct eventRecord *)pcommon;
    struct eventdset *pdset;
    long status=0;

    if (pass==0) return(0);

    recGblInitConstantLink(&prec->siml, DBF_USHORT, &prec->simm);
    recGblInitConstantLink(&prec->siol, DBF_STRING, &prec->sval);

    if( (pdset=(struct eventdset *)(prec->dset)) && (pdset->init_record) ) 
		status=(*pdset->init_record)(prec);

    prec->epvt = eventNameToHandle(prec->val);

    return(status);
}

static long process(struct dbCommon *pcommon)
{
    struct eventRecord *prec = (struct eventRecord *)pcommon;
    struct eventdset  *pdset = (struct eventdset *)(prec->dset);
	long		 status=0;
	unsigned char    pact=prec->pact;

	if((pdset!=NULL) && (pdset->number >= 5) && pdset->read_event ) 
                status=readValue(prec); /* read the new value */
	/* check if device support set pact */
	if ( !pact && prec->pact ) return(0);
	prec->pact = TRUE;
 
	postEvent(prec->epvt);

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
    eventRecord *prec = (eventRecord *)paddr->precord;

    if (!after) return 0;
    if (dbGetFieldIndex(paddr) == eventRecordVAL) {
        prec->epvt = eventNameToHandle(prec->val);
    }
    return 0;
}


static void monitor(eventRecord *prec)
{
    unsigned short  monitor_mask;

    /* get previous stat and sevr  and new stat and sevr*/
    monitor_mask = recGblResetAlarms(prec);
    db_post_events(prec,&prec->val,monitor_mask|DBE_VALUE);
    return;
}

static long readValue(eventRecord *prec)
{
        long            status;
        struct eventdset   *pdset = (struct eventdset *) (prec->dset);

        if (prec->pact == TRUE){
                status=(*pdset->read_event)(prec);
                return(status);
        }

        status=dbGetLink(&(prec->siml),DBR_USHORT,&(prec->simm),0,0);

        if (status)
                return(status);

        if (prec->simm == menuYesNoNO){
                status=(*pdset->read_event)(prec);
                return(status);
        }
        if (prec->simm == menuYesNoYES){
                status=dbGetLink(&(prec->siol),DBR_STRING,
			&(prec->sval),0,0);
                if (status==0) {
                        if (strcmp(prec->sval, prec->val) != 0) {
                                strcpy(prec->val, prec->sval);
                                prec->epvt = eventNameToHandle(prec->val);
                        }
                        prec->udf=FALSE;
                }
        } else {
                status=-1;
                recGblSetSevr(prec,SOFT_ALARM,INVALID_ALARM);
                return(status);
        }
        recGblSetSevr(prec,SIMM_ALARM,prec->sims);

        return(status);
}
