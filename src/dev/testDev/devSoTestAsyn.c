/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devSoTestAsyn.c */
/* base/src/dev $Id$ */

/* devSoTestAsyn.c - Device Support Routines for testing asynchronous processing*/
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
#include	<cvtTable.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<dbCommon.h>
#include	<stringoutRecord.h>

/* Create the dset for devSoTestAsyn */
static long init_record();
static long write_stringout();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_stringout;
	DEVSUPFUN	special_linconv;
}devSoTestAsyn={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	write_stringout,
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
    

static long init_record(pstringout)
    struct stringoutRecord	*pstringout;
{
    struct callback *pcallback;

    /* stringout.out must be a CONSTANT*/
    switch (pstringout->out.type) {
    case (CONSTANT) :
	pcallback = (struct callback *)(calloc(1,sizeof(struct callback)));
	pstringout->dpvt = (void *)pcallback;
	callbackSetCallback(myCallback,&pcallback->callback);
        pcallback->precord = (struct dbCommon *)pstringout;
	pcallback->wd_id = wdCreate();
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pstringout,
		"devSoTestAsyn (init_record) Illegal OUT field");
	return(S_db_badField);
    }
    return(0);
}

static long write_stringout(pstringout)
    struct stringoutRecord	*pstringout;
{
    struct callback *pcallback=(struct callback *)(pstringout->dpvt);
    int		wait_time;

    /* stringout.out must be a CONSTANT*/
    switch (pstringout->out.type) {
    case (CONSTANT) :
	if(pstringout->pact) {
		printf("%s Completed\n",pstringout->name);
		return(0); /* don`t convert*/
	} else {
		wait_time = (int)(pstringout->disv * vxTicksPerSecond);
		if(wait_time<=0) return(0);
		callbackSetPriority(pstringout->prio,&pcallback->callback);
		printf("%s Starting asynchronous processing\n",pstringout->name);
		wdStart(pcallback->wd_id,wait_time,(FUNCPTR)callbackRequest,(int)pcallback);
		pstringout->pact=TRUE;
		return(0);
	}
    default :
        if(recGblSetSevr(pstringout,SOFT_ALARM,INVALID_ALARM)){
		if(pstringout->stat!=SOFT_ALARM) {
			recGblRecordError(S_db_badField,(void *)pstringout,
			    "devSoTestAsyn (read_stringout) Illegal OUT field");
		}
	}
    }
    return(0);
}
