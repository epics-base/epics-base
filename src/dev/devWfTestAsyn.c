/* devWfTestAsyn.c */
/* share/src/dev $Id$ */

/* devWfTestAsyn.c - Device Support Routines for testing asynchronous processing*/
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
 * .01  mm-dd-yy        iii     Comment
 * .02  mm-dd-yy        iii     Comment
 *      ...
 */


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<wdLib.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<devSup.h>
#include	<link.h>
#include	<waveformRecord.h>

/* Create the dset for devWfTestAsyn */
long init_record();
long read_wf();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_wf;
	DEVSUPFUN	special_linconv;
}devWfTestAsyn={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	read_wf,
	NULL};

/* control block for callback*/
struct callback {
	void (*callback)();
	int priority;
	struct dbAddr dbAddr;
	WDOG_ID wd_id;
	void (*process)();
};
void callbackRequest();

static void myCallback(pcallback)
    struct callback *pcallback;
{
    struct wfRecord *pwf=(struct wfRecord *)(pcallback->dbAddr.precord);

    dbScanLock(pwf);
    (pcallback->process)(&pcallback->dbAddr);
    dbScanUnlock(pwf);
}
static long init_record(pwf,process)
    struct waveformRecord	*pwf;
    void (*process)();
{
    char message[100];
    struct callback *pcallback;

    /* wf.inp must be a CONSTANT*/
    switch (pwf->inp.type) {
    case (CONSTANT) :
	pcallback = (struct callback *)(calloc(1,sizeof(struct callback)));
	pwf->dpvt = (caddr_t)pcallback;
	pcallback->callback = myCallback;
	pcallback->priority = priorityLow;
	if(dbNameToAddr(pwf->name,&(pcallback->dbAddr))) {
		logMsg("dbNameToAddr failed in init_record for devWfTestAsyn\n");
		exit(1);
	}
	pcallback->wd_id = wdCreate();
	pcallback->process = process;
	pwf->nord = 0;
	break;
    default :
	strcpy(message,pwf->name);
	strcat(message,": devWfTestAsyn (init_record) Illegal INP field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }
    return(0);
}

static long read_wf(pwf)
    struct waveformRecord	*pwf;
{
    char message[100];
    long status,options,nRequest;
    struct callback *pcallback=(struct callback *)(pwf->dpvt);
    int		wait_time;

    /* wf.inp must be a CONSTANT*/
    switch (pwf->inp.type) {
    case (CONSTANT) :
	if(pwf->pact) {
		printf("%s Completed\n",pwf->name);
		return(0); /* don`t convert*/
	} else {
		wait_time = (int)(pwf->disv * vxTicksPerSecond);
		if(wait_time<=0) return(0);
		printf("%s Starting asynchronous processing\n",pwf->name);
		wdStart(pcallback->wd_id,wait_time,callbackRequest,pcallback);
		return(1);
	}
    default :
	if(pwf->nsev<VALID_ALARM) {
		pwf->nsev = VALID_ALARM;
		pwf->nsta = SOFT_ALARM;
		if(pwf->stat!=SOFT_ALARM) {
			strcpy(message,pwf->name);
			strcat(message,": devWfTestAsyn (read_wf) Illegal INP field");
			errMessage(S_db_badField,message);
		}
	}
    }
    return(0);
}
