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
static long init_record(struct histogramRecord *phistogram);
static long read_histogram(struct histogramRecord *phistogram);
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

static long init_record(struct histogramRecord *prec)
{
    CALLBACK *pcallback;

    /* histogram.svl must be a CONSTANT*/
    switch (prec->svl.type) {
    case (CONSTANT) :
	pcallback = (CALLBACK *)(calloc(1,sizeof(CALLBACK)));
	prec->dpvt = (void *)pcallback;
        if(recGblInitConstantLink(&prec->svl,DBF_DOUBLE,&prec->sgnl))
            prec->udf = FALSE;
	break;
    default :
	recGblRecordError(S_db_badField,(void *)prec,
	    "devHistogramTestAsyn (init_record) Illegal SVL field");
        prec->pact=TRUE;
	return(S_db_badField);
    }
    return(0);
}

static long read_histogram(struct histogramRecord *prec)
{
    CALLBACK *pcallback=(CALLBACK *)(prec->dpvt);

    /* histogram.svl must be a CONSTANT*/
    switch (prec->svl.type) {
    case (CONSTANT) :
	if(prec->pact) {
		printf("Completed asynchronous processing: %s\n",
                    prec->name);
		return(0); /*add count*/
	} else {
                if(prec->disv<=0) return(2);
                printf("Starting asynchronous processing: %s\n",
                    prec->name);
                prec->pact=TRUE;
                callbackRequestProcessCallbackDelayed(
                    pcallback,prec->prio,prec,
                    (double)prec->disv);
		return(0);
	}
    default :
        if(recGblSetSevr(prec,SOFT_ALARM,INVALID_ALARM)){
		if(prec->stat!=SOFT_ALARM) {
			recGblRecordError(S_db_badField,(void *)prec,
			    "devHistogramTestAsyn (read_histogram) Illegal SVL field");
		}
	}
    }
    return(0);
}
