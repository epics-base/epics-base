/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devAiTestAsyn.c */
/* base/src/dev $Id$ */

/* devAiTestAsyn.c - Device Support Routines for testing asynchronous processing*/
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            6-1-90
 */

#include	<vxWorks.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>
#include	<wdLib.h>
#include	<memLib.h>

#include	<alarm.h>
#include	<callback.h>
#include	<cvtTable.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<dbCommon.h>
#include	<aiRecord.h>

/* Create the dset for devAiTestAsyn */
static long init_record();
static long read_ai();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_ai;
	DEVSUPFUN	special_linconv;
}devAiTestAsyn={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	read_ai,
	NULL};

/* control block for callback*/
struct callback {
	CALLBACK	callback;
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

static long init_record(pai)
    struct aiRecord	*pai;
{
    struct callback *pcallback;

    /* ai.inp must be a CONSTANT*/
    switch (pai->inp.type) {
    case (CONSTANT) :
	pcallback = (struct callback *)(calloc(1,sizeof(struct callback)));
	pai->dpvt = (void *)pcallback;
	callbackSetCallback(myCallback,&pcallback->callback);
	pcallback->precord = (struct dbCommon *)pai;
	pcallback->wd_id = wdCreate();
	if(recGblInitConstantLink(&pai->inp,DBF_DOUBLE,&pai->val))
	    pai->udf = FALSE;
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pai,
		"devAiTestAsyn (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long read_ai(pai)
    struct aiRecord	*pai;
{
    struct callback *pcallback=(struct callback *)(pai->dpvt);
    int		wait_time;

    /* ai.inp must be a CONSTANT*/
    switch (pai->inp.type) {
    case (CONSTANT) :
	if(pai->pact) {
		printf("Completed asynchronous processing: %s\n",pai->name);
		return(2); /* don`t convert*/
	} else {
		wait_time = (int)(pai->disv * vxTicksPerSecond);
		if(wait_time<=0) return(2);
		callbackSetPriority(pai->prio,&pcallback->callback);
		printf("Starting asynchronous processing: %s\n",pai->name);
		wdStart(pcallback->wd_id,wait_time,(FUNCPTR)callbackRequest,(int)pcallback);
		pai->pact=TRUE;
    		return(0);
	}
    default :
        if(recGblSetSevr(pai,SOFT_ALARM,INVALID_ALARM)){
		if(pai->stat!=SOFT_ALARM) {
			recGblRecordError(S_db_badField,(void *)pai,
			   "devAiTestAsyn (read_ai) Illegal INP field");
		}
	}
    }
    return(0);
}
