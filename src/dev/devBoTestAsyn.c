/* devBoTestAsyn.c */
/* share/src/dev $Id$ */

/* devBoTestAsyn.c - Device Support Routines for testing asynchronous processing*/
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
 *      ...
 */



#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<wdLib.h>

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
long init_record();
long write_bo();
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
    char message[100];
    struct callback *pcallback;

    /* bo.out must be a CONSTANT*/
    switch (pbo->out.type) {
    case (CONSTANT) :
	pcallback = (struct callback *)(calloc(1,sizeof(struct callback)));
	pbo->dpvt = (void *)pcallback;
	callbackSetCallback(myCallback,pcallback);
	pcallback->precord = (struct dbCommon *)pbo;
	pcallback->wd_id = wdCreate();
	break;
    default :
	strcpy(message,pbo->name);
	strcat(message,": devBoTestAsyn (init_record) Illegal OUT field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }
    return(2);
}

static long write_bo(pbo)
    struct boRecord	*pbo;
{
    char message[100];
    long status,options,nRequest;
    struct callback *pcallback=(struct callback *)(pbo->dpvt);
    int		wait_time;

    /* bo.out must be a CONSTANT*/
    switch (pbo->out.type) {
    case (CONSTANT) :
	if(pbo->pact) {
		printf("%s Completed\n",pbo->name);
		return(0); /* don`t convert*/
	} else {
		wait_time = (int)(pbo->disv * vxTicksPerSecond);
		if(wait_time<=0) return(0);
		callbackSetPriority(pbo->prio,pcallback);
		printf("%s Starting asynchronous processing\n",pbo->name);
		wdStart(pcallback->wd_id,wait_time,callbackRequest,(int)pcallback);
		return(1);
	}
    default :
        if(recGblSetSevr(pbo,SOFT_ALARM,VALID_ALARM)){
		if(pbo->stat!=SOFT_ALARM) {
			strcpy(message,pbo->name);
			strcat(message,": devBoTestAsyn (read_bo) Illegal OUT field");
			errMessage(S_db_badField,message);
		}
	}
    }
    return(0);
}
