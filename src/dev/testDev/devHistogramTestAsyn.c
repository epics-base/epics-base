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
#include	<vxWorks.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<wdLib.h>
#include	<memLib.h>
#include	<string.h>
#include	<alarm.h>
#include	<callback.h>
#include	<cvtTable.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<dbCommon.h>
#include	<histogramRecord.h>

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
	NULL};

/* control block for callback*/
struct callback {
        CALLBACK        callback;
        struct dbCommon *precord;
        WDOG_ID wd_id;
};

static void myCallback(pcallback)
    struct callback *pcallback;
{
    struct dbCommon *precord=pcallback->precord;
    struct rset     *prset=(struct rset *)(precord->rset);

    dbScanLock(precord);
    (*prset->process)(precord);
    dbScanUnlock(precord);
}

static long init_record(phistogram)
    struct histogramRecord	*phistogram;
{
    struct callback *pcallback;

    /* histogram.svl must be a CONSTANT*/
    switch (phistogram->svl.type) {
    case (CONSTANT) :
	pcallback = (struct callback *)(calloc(1,sizeof(struct callback)));
	phistogram->dpvt = (void *)pcallback;
	callbackSetCallback(myCallback,&pcallback->callback);
        pcallback->precord = (struct dbCommon *)phistogram;
	pcallback->wd_id = wdCreate();
        if(recGblInitConstantLink(&phistogram->svl,DBF_DOUBLE,&phistogram->sgnl))
            phistogram->udf = FALSE;
	break;
    default :
	recGblRecordError(S_db_badField,(void *)phistogram,
	    "devHistogramTestAsyn (init_record) Illegal SVL field");
	return(S_db_badField);
    }
    return(0);
}

static long read_histogram(phistogram)
    struct histogramRecord	*phistogram;
{
    struct callback *pcallback=(struct callback *)(phistogram->dpvt);
    int		wait_time;

    /* histogram.svl must be a CONSTANT*/
    switch (phistogram->svl.type) {
    case (CONSTANT) :
	if(phistogram->pact) {
		printf("%s Completed\n",phistogram->name);
		return(0); /*add count*/
	} else {
		wait_time = (int)(phistogram->disv * vxTicksPerSecond);
		if(wait_time<=0) return(0);
		callbackSetPriority(phistogram->prio,&pcallback->callback);
		printf("%s Starting asynchronous processing\n",phistogram->name);
		wdStart(pcallback->wd_id,wait_time,(FUNCPTR)callbackRequest,(int)pcallback);
		phistogram->pact=TRUE;
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
