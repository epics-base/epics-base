/* devMbboDirectTestAsyn.c */
/* base/src/dev $Id$ */

/* devMbboDirectTestAsyn.c - Device Support for testing asynch processing */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Matthew Needes
 *      Date:            10-08-93
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
 *  (modification log for devMbboTestAsyn applies)
 *  .01  10-08-93   mcn    device support for MbboDirect records
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
#include	<rec/dbCommon.h>
#include	<mbboDirectRecord.h>

/* Create the dset for devMbboDirectTestAsyn */
static long init_record();
static long write_mbbo();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_mbbo;
	DEVSUPFUN	special_linconv;
}devMbboDirectTestAsyn={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	write_mbbo,
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

static long init_record(pmbbo)
    struct mbboDirectRecord	*pmbbo;
{
    struct callback *pcallback;

    /* mbbo.out must be a CONSTANT*/
    switch (pmbbo->out.type) {
    case (CONSTANT) :
	pcallback = (struct callback *)(calloc(1,sizeof(struct callback)));
	pmbbo->dpvt = (void *)pcallback;
	callbackSetCallback(myCallback,pcallback);
        pcallback->precord = (struct dbCommon *)pmbbo;
	pcallback->wd_id = wdCreate();
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pmbbo,
	    "devMbboDirectTestAsyn (init_record) Illegal OUT field");
	return(S_db_badField);
    }
    return(2);
}

static long write_mbbo(pmbbo)
    struct mbboDirectRecord	*pmbbo;
{
    struct callback *pcallback=(struct callback *)(pmbbo->dpvt);
    int		wait_time;

    /* mbbo.out must be a CONSTANT*/
    switch (pmbbo->out.type) {
    case (CONSTANT) :
	if(pmbbo->pact) {
		printf("%s Completed\n",pmbbo->name);
		return(0);
	} else {
		wait_time = (int)(pmbbo->disv * vxTicksPerSecond);
		if(wait_time<=0) return(0);
		callbackSetPriority(pmbbo->prio,pcallback);
		printf("%s Starting asynchronous processing\n",pmbbo->name);
		wdStart(pcallback->wd_id,wait_time,(FUNCPTR)callbackRequest,(int)pcallback);
		pmbbo->pact=TRUE;
		return(0);
	}
    default :
        if(recGblSetSevr(pmbbo,SOFT_ALARM,INVALID_ALARM)){
		if(pmbbo->stat!=SOFT_ALARM) {
			recGblRecordError(S_db_badField,(void *)pmbbo,
			    "devMbboDirectTestAsyn (read_mbbo) Illegal OUT field");
		}
	}
    }
    return(0);
}
