/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devBiTestAsyn.c */
/* base/src/dev $Id$ */

/* devBiTestAsyn.c - Device Support Routines for testing asynchronous processing*/
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
#include "biRecord.h"
#include "epicsExport.h"

/* Create the dset for devBiTestAsyn */
static long init_record();
static long read_bi();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_bi;
	DEVSUPFUN	special_linconv;
}devBiTestAsyn={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	read_bi,
	NULL
};
epicsExportAddress(dset,devBiTestAsyn);

static long init_record(pbi)
    struct biRecord	*pbi;
{
    CALLBACK *pcallback;

    /* bi.inp must be a CONSTANT*/
    switch (pbi->inp.type) {
    case (CONSTANT) :
	pcallback = (CALLBACK *)(calloc(1,sizeof(CALLBACK)));
	pbi->dpvt = (void *)pcallback;
	if(recGblInitConstantLink(&pbi->inp,DBF_ENUM,&pbi->val))
	    pbi->udf = FALSE;
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pbi,
		"devBiTestAsyn (init_record) Illegal INP field");
        pbi->pact=TRUE;
	return(S_db_badField);
    }
    return(0);
}

static long read_bi(pbi)
    struct biRecord	*pbi;
{
    CALLBACK *pcallback=(CALLBACK *)(pbi->dpvt);

    /* bi.inp must be a CONSTANT*/
    switch (pbi->inp.type) {
    case (CONSTANT) :
	if(pbi->pact) {
		printf("Completed asynchronous processing: %s\n",pbi->name);
		return(2); /* don't convert */
	} else {
                if(pbi->disv<=0) return(2);
                printf("Starting asynchronous processing: %s\n",pbi->name);
                pbi->pact=TRUE;
                callbackRequestProcessCallbackDelayed(
                    pcallback,pbi->prio,pbi,(double)pbi->disv);
		return(0);
	}
    default :
        if(recGblSetSevr(pbi,SOFT_ALARM,INVALID_ALARM)){
		if(pbi->stat!=SOFT_ALARM) {
			recGblRecordError(S_db_badField,(void *)pbi,
			    "devBiTestAsyn (read_bi) Illegal INP field");
		}
	}
    }
    return(0);
}
