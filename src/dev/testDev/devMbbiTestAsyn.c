/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devMbbiTestAsyn.c */
/* base/src/dev $Id$ */

/* devMbbiTestAsyn.c - Device Support Routines for testing asynchronous processing*/
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
#include "dbDefs.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "link.h"
#include "dbCommon.h"
#include "mbbiRecord.h"
#include "epicsExport.h"

/* Create the dset for devMbbiTestAsyn */
static long init_record();
static long read_mbbi();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_mbbi;
	DEVSUPFUN	special_linconv;
}devMbbiTestAsyn={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	read_mbbi,
	NULL
};
epicsExportAddress(dset,devMbbiTestAsyn);

static long init_record(pmbbi)
    struct mbbiRecord	*pmbbi;
{
    CALLBACK *pcallback;

    /* mbbi.inp must be a CONSTANT*/
    switch (pmbbi->inp.type) {
    case (CONSTANT) :
	pcallback = (CALLBACK *)(calloc(1,sizeof(CALLBACK)));
	pmbbi->dpvt = (void *)pcallback;
	if(recGblInitConstantLink(&pmbbi->inp,DBF_ENUM,&pmbbi->val))
	    pmbbi->udf = FALSE;
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pmbbi,
		"devMbbiTestAsyn (init_record) Illegal INP field");
        pmbbi->pact=TRUE;
	return(S_db_badField);
    }
    return(0);
}

static long read_mbbi(pmbbi)
    struct mbbiRecord	*pmbbi;
{
    CALLBACK *pcallback=(CALLBACK *)(pmbbi->dpvt);

    /* mbbi.inp must be a CONSTANT*/
    switch (pmbbi->inp.type) {
    case (CONSTANT) :
	if(pmbbi->pact) {
		printf("Completed asynchronous processing: %s\n",pmbbi->name);
		return(2); /* don't convert */
	} else {
                if(pmbbi->disv<=0) return(2);
                printf("Starting asynchronous processing: %s\n",pmbbi->name);
                pmbbi->pact=TRUE;
                callbackRequestProcessCallbackDelayed(
                    pcallback,pmbbi->prio,pmbbi,(double)pmbbi->disv);
		return(0);
	}
    default :
        if(recGblSetSevr(pmbbi,SOFT_ALARM,INVALID_ALARM)){
		if(pmbbi->stat!=SOFT_ALARM) {
			recGblRecordError(S_db_badField,(void *)pmbbi,
			    "devMbbiTestAsyn (read_mbbi) Illegal INP field");
		}
	}
    }
    return(0);
}
