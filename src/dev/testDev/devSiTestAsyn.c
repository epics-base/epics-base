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
#include	<stringinRecord.h>

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
    

static long init_record(pstringin)
    struct stringinRecord	*pstringin;
{
    struct callback *pcallback;

    /* stringin.inp must be a CONSTANT*/
    switch (pstringin->inp.type) {
    case (CONSTANT) :
	pcallback = (struct callback *)(calloc(1,sizeof(struct callback)));
	pstringin->dpvt = (void *)pcallback;
	callbackSetCallback(myCallback,&pcallback->callback);
        pcallback->precord = (struct dbCommon *)pstringin;
	pcallback->wd_id = wdCreate();
	if(recGblInitConstantLink(&pstringin->inp,DBF_STRING,pstringin->val))
	    pstringin->udf = FALSE;
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pstringin,
		"devSiTestAsyn (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long read_stringin(pstringin)
    struct stringinRecord	*pstringin;
{
    struct callback *pcallback=(struct callback *)(pstringin->dpvt);
    int		wait_time;

    /* stringin.inp must be a CONSTANT*/
    switch (pstringin->inp.type) {
    case (CONSTANT) :
	if(pstringin->pact) {
		printf("%s Completed\n",pstringin->name);
		return(0);
	} else {
		wait_time = (int)(pstringin->disv * vxTicksPerSecond);
		if(wait_time<=0) return(0);
		callbackSetPriority(pstringin->prio,&pcallback->callback);
		printf("%s Starting asynchronous processing\n",pstringin->name);
		wdStart(pcallback->wd_id,wait_time,(FUNCPTR)callbackRequest,(int)pcallback);
		pstringin->pact=TRUE;
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
