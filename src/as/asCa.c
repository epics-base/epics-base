/* share/src/as/asCa.c	*/
/* share/src/as $Id$ */
/*
 *      Author:  Marty Kraimer
 *      Date:    10-15-93
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
 * .01  03-22-94	mrk	Initial Implementation
 */

#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#ifdef vxWorks
#include <taskLib.h>
#endif
#include <dbDefs.h>
#include <asLib.h>
#include <cadef.h>
#include <caerr.h>
#include <caeventmask.h>
#include <calink.h>
#include <task_params.h>

static ASBASE *pasbase=NULL;
static int taskid;

typedef struct {
    struct dbr_sts_double rtndata;
    chid		mychid;
    evid		myevid;
    int			gotEvent;
} CAPVT;

void connectCallback(struct connection_handler_args cha)
{
    chid		chid=cha.chid;
    ASGINP		*pasginp = (ASGINP *)ca_puser(chid);
    ASG			*pasg = (ASG *)pasginp->pasg;;
    int			inpOk=TRUE;
    CAPVT		*pcapvt;
    enum channel_state	state;

    pasginp = (ASGINP *)ellFirst(&pasg->inpList);
    while(pasginp) {
	pcapvt = pasginp->capvt;
	if(ca_state(pcapvt->chid)!=cs_conn) {
	    inpOk = FALSE;
	    pcapvt->gotEvent = FALSE;
	}
	pasginp = (ASGINP *)ellNext((ELLNODE *)pasginp);
    }
    if(!inpOk && pasg->inpOk) {
	pasg->inpOk = FALSE;
	asComputeAsg(pasg);
    }
}

void eventCallback(struct event_handler_args eha)
{
    ASGINP		*pasginp = (ASGINP *)eha.usr;
    CAPVT		*pcapvt = (CAPVT *)pasginp->capvt;
    ASG			*pasg = (ASG *)pasginp->pasg;;
    int			inpOk=TRUE;
    enum channel_state	state;
    struct dbr_sts_double *pdata = eha.dbr;

    
    pcapvt->rtndata = *pdata; /*structure copy*/
    pcapvt->gotEvent = TRUE;
    pasg->pavalue[pasginp->inpIndex] = pdata->value;
    pasginp = (ASGINP *)ellFirst(&pasg->inpList);
    while(pasginp) {
	pcapvt = pasginp->capvt;
	if(ca_state(pcapvt->chid)!=cs_conn) {
	    inpOk = FALSE;
	    pcapvt->gotEvent = FALSE;
	}
	pasginp = (ASGINP *)ellNext((ELLNODE *)pasginp);
    }
    if(!inpOk && pasg->inpOk) {
	pasg->inpOk = FALSE;
	asComputeAsg(pasg);
    }
}

static asCaInit(
    ASG		*pasg;
    ASGINP	*pasginp;
    CAINP	*pcapvt;

    SEVCHK(ca_task_initialize(),"ca_task_initialize");
    pasg = (ASG *)ellFirst(&pasbase->asgList);
    while(pasg) {
	pasg->inpOk = FALSE;
	pasginp = (ASGINP *)ellFirst(&pasg->inpList);
	while(pasginp) {
	    pcapvt = pasginp->capvt = asCalloc(1,sizeof(CAPVT));
	    SEVCHK(ca_build_and_connect(pasginp->inp,TYPENOTCONN,0,
		&pcapvt->mychid,0,connectCallback,pasginp),
		"ca_build_and_connect");
	    SEVCHK(ca_add_event(DBR_STATUS_DOUBLE,pcapvt->mychid,
		eventCallback,pasginp,&pcapvt->myevid),
		"ca_add_event");
	    pasginp = (ASGINP *)ellNext((ELLNODE *)pasginp);
	}
	pasg = (ASG *)ellNext((ELLNODE *)pasg);
    }
    SEVCHK(ca_pend_event(0.0),"ca_pend_event");
}
    

void asCaStart(ASBASE *ptemp)
{
    pasbase = ptemp;
    taskid = taskSpawn("asCa",CA_CLIENT_PRI,VX_FP_TASK,CA_CLIENT_STACK,
	(FUNCPTR)asCaInit,0,0,0,0,0,0,0,0,0,0);
}

static asCaStop(void)
{
    taskDelete(taskid);
    
