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
#include	<mbbiRecord.h>

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
    

static long init_record(pmbbi)
    struct mbbiRecord	*pmbbi;
{
    struct callback *pcallback;

    /* mbbi.inp must be a CONSTANT*/
    switch (pmbbi->inp.type) {
    case (CONSTANT) :
	pcallback = (struct callback *)(calloc(1,sizeof(struct callback)));
	pmbbi->dpvt = (void *)pcallback;
	callbackSetCallback(myCallback,&pcallback->callback);
        pcallback->precord = (struct dbCommon *)pmbbi;
	pcallback->wd_id = wdCreate();
	if(recGblInitConstantLink(&pmbbi->inp,DBF_ENUM,&pmbbi->val))
	    pmbbi->udf = FALSE;
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pmbbi,
		"devMbbiTestAsyn (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long read_mbbi(pmbbi)
    struct mbbiRecord	*pmbbi;
{
    struct callback *pcallback=(struct callback *)(pmbbi->dpvt);
    int		wait_time;

    /* mbbi.inp must be a CONSTANT*/
    switch (pmbbi->inp.type) {
    case (CONSTANT) :
	if(pmbbi->pact) {
		printf("%s Completed\n",pmbbi->name);
		return(2); /* don't convert */
	} else {
		wait_time = (int)(pmbbi->disv * vxTicksPerSecond);
		if(wait_time<=0) return(0);
		callbackSetPriority(pmbbi->prio,&pcallback->callback);
		printf("%s Starting asynchronous processing\n",pmbbi->name);
		wdStart(pcallback->wd_id,wait_time,(FUNCPTR)callbackRequest,(int)pcallback);
		pmbbi->pact=TRUE;
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
