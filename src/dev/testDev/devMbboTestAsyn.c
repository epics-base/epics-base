/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devMbboTestAsyn.c */
/* base/src/dev $Id$ */

/* devMbboTestAsyn.c - Device Support Routines for testing asynchronous processing*/
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
#include	<mbboRecord.h>

/* Create the dset for devMbboTestAsyn */
static long init_record();
static long write_mbbo();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_mbbo;
	DEVSUPFUN	special_linconv;
}devMbboTestAsyn={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	write_mbbo,
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
    
    

static long init_record(pmbbo)
    struct mbboRecord	*pmbbo;
{
    struct callback *pcallback;

    /* mbbo.out must be a CONSTANT*/
    switch (pmbbo->out.type) {
    case (CONSTANT) :
	pcallback = (struct callback *)(calloc(1,sizeof(struct callback)));
	pmbbo->dpvt = (void *)pcallback;
	callbackSetCallback(myCallback,&pcallback->callback);
        pcallback->precord = (struct dbCommon *)pmbbo;
	pcallback->wd_id = wdCreate();
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pmbbo,
	    "devMbboTestAsyn (init_record) Illegal OUT field");
	return(S_db_badField);
    }
    return(2);
}

static long write_mbbo(pmbbo)
    struct mbboRecord	*pmbbo;
{
    struct callback *pcallback=(struct callback *)(pmbbo->dpvt);
    int		wait_time;

    /* mbbo.out must be a CONSTANT*/
    switch (pmbbo->out.type) {
    case (CONSTANT) :
	if(pmbbo->pact) {
		printf("%s Completed\n",pmbbo->name);
		return(0);
	} else {
		wait_time = (int)(pmbbo->disv * vxTicksPerSecond);
		if(wait_time<=0) return(0);
		callbackSetPriority(pmbbo->prio,&pcallback->callback);
		printf("%s Starting asynchronous processing\n",pmbbo->name);
		wdStart(pcallback->wd_id,wait_time,(FUNCPTR)callbackRequest,(int)pcallback);
		pmbbo->pact=TRUE;
		return(0);
	}
    default :
        if(recGblSetSevr(pmbbo,SOFT_ALARM,INVALID_ALARM)){
		if(pmbbo->stat!=SOFT_ALARM) {
			recGblRecordError(S_db_badField,(void *)pmbbo,
			    "devMbboTestAsyn (read_mbbo) Illegal OUT field");
		}
	}
    }
    return(0);
}
