/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devSiTestAsyn.c */
/* base/src/dev $Id$ */

/* devSiTestAsyn.c - Device Support Routines for testing asynchronous processing*/
/*
 *      Author:          Janet Anderson
 *      Date:            5-1-91
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
#include "stringinRecord.h"
#include "epicsExport.h"

/* Create the dset for devSiTestAsyn */
static long init_record();
static long read_stringin();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_stringin;
	DEVSUPFUN	special_linconv;
}devSiTestAsyn={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	read_stringin,
	NULL
};
epicsExportAddress(dset,devSiTestAsyn);

static long init_record(pstringin)
    struct stringinRecord	*pstringin;
{
    CALLBACK *pcallback;

    /* stringin.inp must be a CONSTANT*/
    switch (pstringin->inp.type) {
    case (CONSTANT) :
	pcallback = (CALLBACK *)(calloc(1,sizeof(CALLBACK)));
	pstringin->dpvt = (void *)pcallback;
	if(recGblInitConstantLink(&pstringin->inp,DBF_STRING,pstringin->val))
	    pstringin->udf = FALSE;
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pstringin,
		"devSiTestAsyn (init_record) Illegal INP field");
        pstringin->pact=TRUE;
	return(S_db_badField);
    }
    return(0);
}

static long read_stringin(pstringin)
    struct stringinRecord	*pstringin;
{
    CALLBACK *pcallback=(CALLBACK *)(pstringin->dpvt);

    /* stringin.inp must be a CONSTANT*/
    switch (pstringin->inp.type) {
    case (CONSTANT) :
	if(pstringin->pact) {
		printf("Completed asynchronous processing: %s\n",
                    pstringin->name);
		return(0);
	} else {
                if(pstringin->disv<=0) return(2);
                printf("Starting asynchronous processing: %s\n",
                    pstringin->name);
                pstringin->pact=TRUE;
                callbackRequestProcessCallbackDelayed(
                    pcallback,pstringin->prio,pstringin,
                    (double)pstringin->disv);
		return(0);
	}
    default :
        if(recGblSetSevr(pstringin,SOFT_ALARM,INVALID_ALARM)){
		if(pstringin->stat!=SOFT_ALARM) {
			recGblRecordError(S_db_badField,(void *)pstringin,
			    "devSiTestAsyn (read_stringin) Illegal INP field");
		}
	}
    }
    return(0);
}
