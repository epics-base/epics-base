/* devSiTestAsyn.c */
/* base/src/dev $Id$ */

/* devSiTestAsyn.c - Device Support Routines for testing asynchronous processing*/
/*
 *      Author:          Janet Anderson
 *      Date:            5-1-91
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
#include	<rec/dbCommon.h>
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
	if (pstringin->inp.value.value!=0.0) {
        	sprintf(pstringin->val,"%-14.7g",pstringin->inp.value.value);
		pstringin->udf = FALSE;
		}
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
