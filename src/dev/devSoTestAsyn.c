/* devSoTestAsyn.c */
 /* share/src/dev   $Id$ */

/* devSoTestAsyn.c - Device Support Routines for testing asynchronous processing*/
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
#include	<stringoutRecord.h>

/* Create the dset for devSoTestAsyn */
long init_record();
long write_stringout();
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
	void (*callback)();
	int priority;
	struct dbCommon *prec;
	WDOG_ID wd_id;
};
void callbackRequest();

static void myCallback(pcallback)
    struct callback *pcallback;
{
    struct stringoutRecord *pstringout=(struct stringoutRecord *)(pcallback->prec);
    struct rset     *prset=(struct rset *)(pstringout->rset);

    dbScanLock(pstringout);
    (*prset->process)(pstringout->pdba);
    dbScanUnlock(pstringout);
}
    
    

static long init_record(pstringout)
    struct stringoutRecord	*pstringout;
{
    char message[100];
    struct callback *pcallback;

    /* stringout.out must be a CONSTANT*/
    switch (pstringout->out.type) {
    case (CONSTANT) :
	pcallback = (struct callback *)(calloc(1,sizeof(struct callback)));
	pstringout->dpvt = (caddr_t)pcallback;
	pcallback->callback = myCallback;
	pcallback->priority = priorityLow;
        pcallback->prec = (struct dbCommon *)pstringout;
	pcallback->wd_id = wdCreate();
	break;
    default :
	strcpy(message,pstringout->name);
	strcat(message,": devSoTestAsyn (init_record) Illegal OUT field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }
    return(0);
}

static long write_stringout(pstringout)
    struct stringoutRecord	*pstringout;
{
    char message[100];
    long status,options,nRequest;
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
		printf("%s Starting asynchronous processing\n",pstringout->name);
		wdStart(pcallback->wd_id,wait_time,callbackRequest,pcallback);
		return(1);
	}
    default :
        if(recGblSetSevr(pstringout,SOFT_ALARM,VALID_ALARM)){
		if(pstringout->stat!=SOFT_ALARM) {
			strcpy(message,pstringout->name);
			strcat(message,": devSoTestAsyn (read_stringout) Illegal OUT field");
			errMessage(S_db_badField,message);
		}
	}
    }
    return(0);
}
