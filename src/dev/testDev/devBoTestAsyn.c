/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devBoTestAsyn.c */
/* base/src/dev $Id$ */

/* devBoTestAsyn.c - Device Support Routines for testing asynchronous processing*/
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            6-1-90
 */


#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
#include "boRecord.h"
#include "epicsExport.h"

/* Create the dset for devBoTestAsyn */
static long init_record();
static long write_bo();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_bo;
	DEVSUPFUN	special_linconv;
}devBoTestAsyn={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	write_bo,
	NULL
};
epicsExportAddress(dset,devBoTestAsyn);

static long init_record(pbo)
    struct boRecord	*pbo;
{
    CALLBACK *pcallback;

    /* bo.out must be a CONSTANT*/
    switch (pbo->out.type) {
    case (CONSTANT) :
	pcallback = (CALLBACK *)(calloc(1,sizeof(CALLBACK)));
	pbo->dpvt = (void *)pcallback;
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pbo,
		"devBoTestAsyn (init_record) Illegal OUT field");
        pbo->pact=TRUE;
	return(S_db_badField);
    }
    return(2);
}

static long write_bo(pbo)
    struct boRecord	*pbo;
{
    CALLBACK *pcallback=(CALLBACK *)(pbo->dpvt);

    /* bo.out must be a CONSTANT*/
    switch (pbo->out.type) {
    case (CONSTANT) :
	if(pbo->pact) {
		printf("Completed asynchronous processing: %s\n",pbo->name);
		return(0);
	} else {
                if(pbo->disv<=0) return(2);
                printf("Starting asynchronous processing: %s\n",pbo->name);
                pbo->pact=TRUE;
                callbackRequestProcessCallbackDelayed(
                    pcallback,pbo->prio,pbo,(double)pbo->disv);
		return(0);
	}
    default :
        if(recGblSetSevr(pbo,SOFT_ALARM,INVALID_ALARM)){
		if(pbo->stat!=SOFT_ALARM) {
			recGblRecordError(S_db_badField,(void *)pbo,
			    "devBoTestAsyn (read_bo) Illegal OUT field");
		}
	}
    }
    return(0);
}
