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
#include	<recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<dbCommon.h>
#include	<biRecord.h>

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
    

static long init_record(pbi)
    struct biRecord	*pbi;
{
    struct callback *pcallback;

    /* bi.inp must be a CONSTANT*/
    switch (pbi->inp.type) {
    case (CONSTANT) :
	pcallback = (struct callback *)(calloc(1,sizeof(struct callback)));
	pbi->dpvt = (void *)pcallback;
	callbackSetCallback(myCallback,&pcallback->callback);
	pcallback->precord = (struct dbCommon *)pbi;
	pcallback->wd_id = wdCreate();
	if(recGblInitConstantLink(&pbi->inp,DBF_ENUM,&pbi->val))
	    pbi->udf = FALSE;
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pbi,
		"devBiTestAsyn (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long read_bi(pbi)
    struct biRecord	*pbi;
{
    struct callback *pcallback=(struct callback *)(pbi->dpvt);
    int		wait_time;

    /* bi.inp must be a CONSTANT*/
    switch (pbi->inp.type) {
    case (CONSTANT) :
	if(pbi->pact) {
		printf("%s Completed\n",pbi->name);
		return(2); /* don't convert */
	} else {
		wait_time = (int)(pbi->disv * vxTicksPerSecond);
		if(wait_time<=0) return(0);
		callbackSetPriority(pbi->prio,&pcallback->callback);
		printf("%s Starting asynchronous processing\n",pbi->name);
		wdStart(pcallback->wd_id,wait_time,(FUNCPTR)callbackRequest,(int)pcallback);
		pbi->pact=TRUE;	
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
