/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devWfTestAsyn.c */
/* base/src/dev $Id$ */

/* devWfTestAsyn.c - Device Support Routines for testing asynchronous processing*/
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            6-1-90
 */


#include	<vxWorks.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<wdLib.h>
#include	<memLib.h>
#include	<string.h>

#include	<alarm.h>
#include	<callback.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<dbCommon.h>
#include	<waveformRecord.h>

/* Create the dset for devWfTestAsyn */
static long init_record();
static long read_wf();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_wf;
	DEVSUPFUN	special_linconv;
}devWfTestAsyn={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	read_wf,
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

static long init_record(pwf)
    struct waveformRecord	*pwf;
{
    struct callback *pcallback;

    /* wf.inp must be a CONSTANT*/
    switch (pwf->inp.type) {
    case (CONSTANT) :
	pcallback = (struct callback *)(calloc(1,sizeof(struct callback)));
	pwf->dpvt = (void *)pcallback;
	callbackSetCallback(myCallback,&pcallback->callback);
        pcallback->precord = (struct dbCommon *)pwf;
	pcallback->wd_id = wdCreate();
	pwf->nord = 0;
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pwf,
		"devWfTestAsyn (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long read_wf(pwf)
    struct waveformRecord	*pwf;
{
    struct callback *pcallback=(struct callback *)(pwf->dpvt);
    int		wait_time;

    /* wf.inp must be a CONSTANT*/
    switch (pwf->inp.type) {
    case (CONSTANT) :
	if(pwf->pact) {
		printf("%s Completed\n",pwf->name);
		return(0); /* don`t convert*/
	} else {
		wait_time = (int)(pwf->disv * vxTicksPerSecond);
		if(wait_time<=0) return(0);
		callbackSetPriority(pwf->prio,&pcallback->callback);
		printf("%s Starting asynchronous processing\n",pwf->name);
		wdStart(pcallback->wd_id,wait_time,(FUNCPTR)callbackRequest,(int)pcallback);
		pwf->pact=TRUE;
		return(0);
	}
    default :
        if(recGblSetSevr(pwf,SOFT_ALARM,INVALID_ALARM)){
		if(pwf->stat!=SOFT_ALARM) {
			recGblRecordError(S_db_badField,(void *)pwf,
			    "devWfTestAsyn (read_wf) Illegal INP field");
		}
	}
    }
    return(0);
}
