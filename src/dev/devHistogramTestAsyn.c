/* devHistogramTestAsyn.c */
/* share/src/dev $Id$    */

/* devHistogramTestAsyn.c - Device Support Routines for testing asynchronous processing*/
/*
 *      Author:		Janet Anderson
 *      Date:		07/02/91
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
#include	<cvtTable.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<histogramRecord.h>

/* Create the dset for devHistogramTestAsyn */
long init_record();
long read_histogram();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_histogram;
	DEVSUPFUN	special_linconv;
}devHistogramTestAsyn={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	read_histogram,
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
    struct histogramRecord *phistogram=(struct histogramRecord *)(pcallback->dbAddr.precord);

    dbScanLock(phistogram);
    (pcallback->process)(&pcallback->dbAddr);
    dbScanUnlock(phistogram);
}
    
    

static long init_record(phistogram,process)
    struct histogramRecord	*phistogram;
    void (*process)();
{
    char message[100];
    struct callback *pcallback;

    /* histogram.svl must be a CONSTANT*/
    switch (phistogram->svl.type) {
    case (CONSTANT) :
	pcallback = (struct callback *)(calloc(1,sizeof(struct callback)));
	phistogram->dpvt = (caddr_t)pcallback;
	pcallback->callback = myCallback;
	pcallback->priority = priorityLow;
	if(dbNameToAddr(phistogram->name,&(pcallback->dbAddr))) {
		logMsg("dbNameToAddr failed in init_record for devHistogramTestAsyn\n");
		exit(1);
	}
	pcallback->wd_id = wdCreate();
	pcallback->process = process;
	phistogram->sgnl = phistogram->svl.value.value;
	break;
    default :
	strcpy(message,phistogram->name);
	strcat(message,": devHistogramTestAsyn (init_record) Illegal SVL field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }
    return(0);
}

static long read_histogram(phistogram)
    struct histogramRecord	*phistogram;
{
    char message[100];
    long status,options,nRequest;
    struct callback *pcallback=(struct callback *)(phistogram->dpvt);
    int		wait_time;

    /* histogram.svl must be a CONSTANT*/
    switch (phistogram->svl.type) {
    case (CONSTANT) :
	if(phistogram->pact) {
		printf("%s Completed\n",phistogram->name);
		return(0); /*add count*/
	} else {
		wait_time = (int)(phistogram->disv * vxTicksPerSecond);
		if(wait_time<=0) return(0);
		printf("%s Starting asynchronous processing\n",phistogram->name);
		wdStart(pcallback->wd_id,wait_time,callbackRequest,pcallback);
		return(1);
	}
    default :
	if(phistogram->nsev<VALID_ALARM) {
		phistogram->nsev = VALID_ALARM;
		phistogram->nsta = SOFT_ALARM;
		if(phistogram->stat!=SOFT_ALARM) {
			strcpy(message,phistogram->name);
			strcat(message,": devHistogramTestAsyn (read_histogram) Illegal SVL field");
			errMessage(S_db_badField,message);
		}
	}
    }
    return(0);
}
