/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devBoTestAsyn.c */
/* base/src/dev $Id$ */

/* devBoTestAsyn.c - Device Support Routines for testing asynchronous processing*/
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
#include	<cvtTable.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<dbCommon.h>
#include	<boRecord.h>

/* Create the dset for devBoTestAsyn */
static long init_record();
static long write_bo();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_bo;
	DEVSUPFUN	special_linconv;
}devBoTestAsyn={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	write_bo,
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


static long init_record(pbo)
    struct boRecord	*pbo;
{
    struct callback *pcallback;

    /* bo.out must be a CONSTANT*/
    switch (pbo->out.type) {
    case (CONSTANT) :
	pcallback = (struct callback *)(calloc(1,sizeof(struct callback)));
	pbo->dpvt = (void *)pcallback;
	callbackSetCallback(myCallback,&pcallback->callback);
	pcallback->precord = (struct dbCommon *)pbo;
	pcallback->wd_id = wdCreate();
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pbo,
		"devBoTestAsyn (init_record) Illegal OUT field");
	return(S_db_badField);
    }
    return(2);
}

static long write_bo(pbo)
    struct boRecord	*pbo;
{
    struct callback *pcallback=(struct callback *)(pbo->dpvt);
    int		wait_time;

    /* bo.out must be a CONSTANT*/
    switch (pbo->out.type) {
    case (CONSTANT) :
	if(pbo->pact) {
		printf("%s Completed\n",pbo->name);
		return(0);
	} else {
		wait_time = (int)(pbo->disv * vxTicksPerSecond);
		if(wait_time<=0) return(0);
		callbackSetPriority(pbo->prio,&pcallback->callback);
		printf("%s Starting asynchronous processing\n",pbo->name);
		wdStart(pcallback->wd_id,wait_time,(FUNCPTR)callbackRequest,(int)pcallback);
		pbo->pact=TRUE;
		return(0);
	}
    default :
        if(recGblSetSevr(pbo,SOFT_ALARM,INVALID_ALARM)){
		if(pbo->stat!=SOFT_ALARM) {
			recGblRecordError(S_db_badField,(void *)pbo,
			    "devBoTestAsyn (read_bo) Illegal OUT field");
		}
	}
    }
    return(0);
}
