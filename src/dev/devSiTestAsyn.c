/* devSiTestAsyn.c */
 /* share/src/dev   $Id$ */

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
 *      ...
 */


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<wdLib.h>

#include	<alarm.h>
#include	<callback.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<stringinRecord.h>

/* Create the dset for devSiTestAsyn */
long init_record();
long read_stringin();
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
	void (*callback)();
	int priority;
	struct dbCommon *prec;
	WDOG_ID wd_id;
};

void callbackRequest();

static void myCallback(pcallback)
    struct callback *pcallback;
{
    struct stringinRecord *pstringin=(struct stringinRecord *)(pcallback->prec);
    struct rset     *prset=(struct rset *)(pstringin->rset);

    dbScanLock((struct dbCommon *)pstringin);
    (*prset->process)(pstringin);
    dbScanUnlock((struct dbCommon *)pstringin);
}
    
    

static long init_record(pstringin)
    struct stringinRecord	*pstringin;
{
    char message[100];
    struct callback *pcallback;

    /* stringin.inp must be a CONSTANT*/
    switch (pstringin->inp.type) {
    case (CONSTANT) :
	pcallback = (struct callback *)(calloc(1,sizeof(struct callback)));
	pstringin->dpvt = (caddr_t)pcallback;
	pcallback->callback = myCallback;
	pcallback->priority = priorityLow;
        pcallback->prec = (struct dbCommon *)pstringin;
	pcallback->wd_id = wdCreate();
	if (pstringin->inp.value.value!=0.0) {
        	sprintf(pstringin->val,"%-14.7g",pstringin->inp.value.value);
		pstringin->udf = FALSE;
		}
	break;
    default :
	strcpy(message,pstringin->name);
	strcat(message,": devSiTestAsyn (init_record) Illegal INP field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }
    return(0);
}

static long read_stringin(pstringin)
    struct stringinRecord	*pstringin;
{
    char message[100];
    long status,options,nRequest;
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
		printf("%s Starting asynchronous processing\n",pstringin->name);
		wdStart(pcallback->wd_id,wait_time,callbackRequest,(int)pcallback);
		return(1);
	}
    default :
        if(recGblSetSevr(pstringin,SOFT_ALARM,VALID_ALARM)){
		if(pstringin->stat!=SOFT_ALARM) {
			strcpy(message,pstringin->name);
			strcat(message,": devSiTestAsyn (read_stringin) Illegal INP field");
			errMessage(S_db_badField,message);
		}
	}
    }
    return(0);
}
