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
 * .01  11-11-91        jba     Moved set of alarm stat and sevr to macros
 * .02  01-08-92        jba     Added cast in call to wdStart to avoid compile warning msg
 * .03  02-05-92	jba	Changed function arguments from paddr to precord 
 * .04	03-13-92	jba	ANSI C changes
 * .05  04-10-92        jba     pact now used to test for asyn processing, not return value
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
#include	<histogramRecord.h>

/* Create the dset for devHistogramTestAsyn */
static long init_record();
static long read_histogram();
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

static long init_record(phistogram)
    struct histogramRecord	*phistogram;
{
    struct callback *pcallback;

    /* histogram.svl must be a CONSTANT*/
    switch (phistogram->svl.type) {
    case (CONSTANT) :
	pcallback = (struct callback *)(calloc(1,sizeof(struct callback)));
	phistogram->dpvt = (void *)pcallback;
	callbackSetCallback(myCallback,pcallback);
        pcallback->precord = (struct dbCommon *)phistogram;
	pcallback->wd_id = wdCreate();
	phistogram->sgnl = phistogram->svl.value.value;
	break;
    default :
	recGblRecordError(S_db_badField,(void *)phistogram,
	    "devHistogramTestAsyn (init_record) Illegal SVL field");
	return(S_db_badField);
    }
    return(0);
}

static long read_histogram(phistogram)
    struct histogramRecord	*phistogram;
{
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
		callbackSetPriority(phistogram->prio,pcallback);
		printf("%s Starting asynchronous processing\n",phistogram->name);
		wdStart(pcallback->wd_id,wait_time,(FUNCPTR)callbackRequest,(int)pcallback);
		phistogram->pact=TRUE;
		return(0);
	}
    default :
        if(recGblSetSevr(phistogram,SOFT_ALARM,INVALID_ALARM)){
		if(phistogram->stat!=SOFT_ALARM) {
			recGblRecordError(S_db_badField,(void *)phistogram,
			    "devHistogramTestAsyn (read_histogram) Illegal SVL field");
		}
	}
    }
    return(0);
}
