/* devAiTestAsyn.c */
/* share/src/dev $Id$ */

/* devAiTestAsyn.c - Device Support Routines for testing asynchronous processing*/
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            6-1-90
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 *      ...
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<wdLib.h>

#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<aiRecord.h>

/* Create the dset for devAiTestAsyn */
long init_record();
long read_ai();
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
	void (*callback)();
	int priority;
	struct dbCommon *prec;
	WDOG_ID wd_id;
};

void callbackRequest();

static void myCallback(pcallback)
    struct callback *pcallback;
{
    struct aiRecord *pai=(struct aiRecord *)(pcallback->prec);
    struct rset     *prset=(struct rset *)(pai->rset);

    dbScanLock(pai);
    (*prset->process)(pai->pdba);
    dbScanUnlock(pai);
}
    
    

static long init_record(pai)
    struct aiRecord	*pai;
{
    char message[100];
    struct callback *pcallback;

    /* ai.inp must be a CONSTANT*/
    switch (pai->inp.type) {
    case (CONSTANT) :
	pcallback = (struct callback *)(calloc(1,sizeof(struct callback)));
	pai->dpvt = (caddr_t)pcallback;
	pcallback->callback = myCallback;
	pcallback->priority = priorityLow;
	pcallback->prec = (struct dbCommon *)pai;
	pcallback->wd_id = wdCreate();
	pai->val = pai->inp.value.value;
	pai->udf = FALSE;
	break;
    default :
	strcpy(message,pai->name);
	strcat(message,": devAiTestAsyn (init_record) Illegal INP field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }
    return(0);
}

static long read_ai(pai)
    struct aiRecord	*pai;
{
    char message[100];
    long status,options,nRequest;
    struct callback *pcallback=(struct callback *)(pai->dpvt);
    int		wait_time;

    /* ai.inp must be a CONSTANT*/
    switch (pai->inp.type) {
    case (CONSTANT) :
	if(pai->pact) {
		printf("%s Completed\n",pai->name);
		return(2); /* don`t convert*/
	} else {
		wait_time = (int)(pai->disv * vxTicksPerSecond);
		if(wait_time<=0) return(0);
		printf("%s Starting asynchronous processing\n",pai->name);
		wdStart(pcallback->wd_id,wait_time,callbackRequest,pcallback);
		return(1);
	}
    default :
        if(recGblSetSevr(pai,SOFT_ALARM,VALID_ALARM)){
		if(pai->stat!=SOFT_ALARM) {
			strcpy(message,pai->name);
			strcat(message,": devAiTestAsyn (read_ai) Illegal INP field");
			errMessage(S_db_badField,message);
		}
	}
    }
    return(0);
}
