/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devHistogramTestAsyn.c */
/* base/src/dev $Id$ */
/*
 *      Author:		Janet Anderson
 *      Date:		07/02/91
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
#include "histogramRecord.h"
#include "epicsExport.h"

/* Create the dset for devHistogramTestAsyn */
static long init_record();
static long read_histogram();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_histogram;
	DEVSUPFUN	special_linconv;
}devHistogramTestAsyn={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	read_histogram,
	NULL
};
epicsExportAddress(dset,devHistogramTestAsyn);

static long init_record(phistogram)
    struct histogramRecord	*phistogram;
{
    CALLBACK *pcallback;

    /* histogram.svl must be a CONSTANT*/
    switch (phistogram->svl.type) {
    case (CONSTANT) :
	pcallback = (CALLBACK *)(calloc(1,sizeof(CALLBACK)));
	phistogram->dpvt = (void *)pcallback;
        if(recGblInitConstantLink(&phistogram->svl,DBF_DOUBLE,&phistogram->sgnl))
            phistogram->udf = FALSE;
	break;
    default :
	recGblRecordError(S_db_badField,(void *)phistogram,
	    "devHistogramTestAsyn (init_record) Illegal SVL field");
        phistogram->pact=TRUE;
	return(S_db_badField);
    }
    return(0);
}

static long read_histogram(phistogram)
    struct histogramRecord	*phistogram;
{
    CALLBACK *pcallback=(CALLBACK *)(phistogram->dpvt);

    /* histogram.svl must be a CONSTANT*/
    switch (phistogram->svl.type) {
    case (CONSTANT) :
	if(phistogram->pact) {
		printf("Completed asynchronous processing: %s\n",
                    phistogram->name);
		return(0); /*add count*/
	} else {
                if(phistogram->disv<=0) return(2);
                printf("Starting asynchronous processing: %s\n",
                    phistogram->name);
                phistogram->pact=TRUE;
                callbackRequestProcessCallbackDelayed(
                    pcallback,phistogram->prio,phistogram,
                    (double)phistogram->disv);
		return(0);
	}
    default :
        if(recGblSetSevr(phistogram,SOFT_ALARM,INVALID_ALARM)){
		if(phistogram->stat!=SOFT_ALARM) {
			recGblRecordError(S_db_badField,(void *)phistogram,
			    "devHistogramTestAsyn (read_histogram) Illegal SVL field");
		}
	}
    }
    return(0);
}
