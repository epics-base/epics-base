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
 * .01  01-08-92        jba     Added cast in call to wdStart to avoid compile warning msg
 * .02  02-05-92	jba	Changed function arguments from paddr to precord 
 * .03	03-13-92	jba	ANSI C changes
 * .04  04-10-92        jba     pact now used to test for asyn processing, not return value
 *      ...
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
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
	callbackSetCallback(myCallback,pcallback);
	pcallback->precord = (struct dbCommon *)pai;
	pcallback->wd_id = wdCreate();
	pai->val = pai->inp.value.value;
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
		printf("%s Completed\n",pai->name);
		return(2); /* don`t convert*/
	} else {
		wait_time = (int)(pai->disv * vxTicksPerSecond);
		if(wait_time<=0) return(0);
		callbackSetPriority(pai->prio,pcallback);
		printf("%s Starting asynchronous processing\n",pai->name);
		wdStart(pcallback->wd_id,wait_time,callbackRequest,(int)pcallback);
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
