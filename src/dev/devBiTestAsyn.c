/* devBiTestAsyn.c */
/* base/src/dev $Id$ */

/* devBiTestAsyn.c - Device Support Routines for testing asynchronous processing*/
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
 * .01  11-11-91        jba     Moved set of alarm stat and sevr to macros
 * .02  01-08-92        jba     Added cast in call to wdStart to avoid compile warning msg
 * .03  02-05-92	jba	Changed function arguments from paddr to precord 
 * .04	03-13-92	jba	ANSI C changes
 * .05  04-10-92        jba     pact now used to test for asyn processing, not return value
 * .06  04-05-94        mrk	ANSI changes to callback routines
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
	pbi->val = pbi->inp.value.value;
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
