/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devLoTestAsyn.c */

/* devLoTestAsyn.c - Device Support Routines for testing asynchronous processing*/
/*
 *      Author:  Marty Kraimer
 *      Date:    06NOV2003
 */


#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "alarm.h"
#include "callback.h"
#include "cvtTable.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "link.h"
#include "dbCommon.h"
#include "longoutRecord.h"
#include "epicsExport.h"

/* Create the dset for devLoTestAsyn */
static long init_record();
static long write_lo();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_lo;
	DEVSUPFUN	special_linconv;
}devLoTestAsyn={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	write_lo,
	NULL
};
epicsExportAddress(dset,devLoTestAsyn);

static long init_record(struct longoutRecord	*plo)
{
    CALLBACK *pcallback;

    /* ao.out must be a CONSTANT*/
    switch (plo->out.type) {
    case (CONSTANT) :
	pcallback = (CALLBACK *)(calloc(1,sizeof(CALLBACK)));
	plo->dpvt = (void *)pcallback;
	break;
    default :
	recGblRecordError(S_db_badField,(void *)plo,
		"devLoTestAsyn (init_record) Illegal OUT field");
	plo->pact=TRUE;
	return(S_db_badField);
    }
    return(2);
}

static long write_lo(longoutRecord *plo)
{
    CALLBACK *pcallback=(CALLBACK *)(plo->dpvt);

    /* ao.out must be a CONSTANT*/
    switch (plo->out.type) {
    case (CONSTANT) :
	if(plo->pact) {
		printf("Completed asynchronous processing: %s\n",plo->name);
		return(0);
	} else {
                if(plo->disv<=0) return(0);
		printf("Starting asynchronous processing: %s\n",plo->name);
		plo->pact=TRUE;
                callbackRequestProcessCallbackDelayed(
                    pcallback,plo->prio,plo,(double)plo->disv);
		return(0);
	}
    default :
        if(recGblSetSevr(plo,SOFT_ALARM,INVALID_ALARM)){
		if(plo->stat!=SOFT_ALARM) {
			recGblRecordError(S_db_badField,(void *)plo,
			   "devLoTestAsyn (write_lo) Illegal OUT field");
		}
	}
    }
    return(0);
}
