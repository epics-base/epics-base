/* share/src/as/asCa.c	*/
/* share/src/as $Id$ */
/* Author:  Marty Kraimer Date:    10-15-93 */
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
AND IN ALL SOURCE LISTINGS OF THE CODE.
 
(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
Argonne National Laboratory (ANL), with facilities in the States of 
Illinois and Idaho, is owned by the United States Government, and
operated by the University of Chicago under provision of a contract
with the Department of Energy.

Portions of this material resulted from work developed under a U.S.
Government contract and are subject to the following license:  For
a period of five years from March 30, 1993, the Government is
granted for itself and others acting on its behalf a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, and perform
publicly and display publicly.  With the approval of DOE, this
period may be renewed for two additional five year periods. 
Following the expiration of this period or periods, the Government
is granted for itself and others acting on its behalf, a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, distribute copies
to the public, perform publicly and display publicly, and to permit
others to do so.

*****************************************************************
                                DISCLAIMER
*****************************************************************

NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
OWNED RIGHTS.  

*****************************************************************
LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY (708-252-2000).
 *
 * Modification Log:
 * -----------------
 * .01  03-22-94	mrk	Initial Implementation
 */

/*This module is separate from asDbLib because CA uses old database access*/
#include <vxWorks.h>
#include <taskLib.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <dbDefs.h>
#include <taskwd.h>
#include <asLib.h>
#include <cadef.h>
#include <caerr.h>
#include <caeventmask.h>
#include <calink.h>
#include <task_params.h>
#include <alarm.h>

static int taskid=0;
static int caInitializing=FALSE;
extern ASBASE *pasbase;

typedef struct {
    struct dbr_sts_double rtndata;
    chid		chid;
    evid		evid;
} CAPVT;

static void connectCallback(struct connection_handler_args cha)
{
    chid		chid = cha.chid;
    ASGINP		*pasginp;
    ASG			*pasg;
    CAPVT		*pcapvt;
    enum channel_state	state;
    int			Ilocked=FALSE;

    if(!caInitializing) {
	FASTLOCK(&asLock);
	Ilocked = TRUE;
    }
    pasginp = (ASGINP *)ca_puser(chid);
    pasg = (ASG *)pasginp->pasg;
    pcapvt = pasginp->capvt;
    if(ca_state(chid)!=cs_conn) {
	pasg->inpBad |= (1<<pasginp->inpIndex);
	if(!caInitializing) asComputeAsg(pasg);
    } /*eventCallback will set inpBad false*/
    if(Ilocked) FASTUNLOCK(&asLock);
}

static void eventCallback(struct event_handler_args eha)
{
    ASGINP		*pasginp;
    CAPVT		*pcapvt;
    ASG			*pasg;
    int			inpOk=TRUE;
    enum channel_state	state;
    struct dbr_sts_double *pdata = eha.dbr;
    int			Ilocked=FALSE;

    
    if(!caInitializing) {
	FASTLOCK(&asLock);
	Ilocked = TRUE;
    }
    pasginp = (ASGINP *)eha.usr;
    pcapvt = (CAPVT *)pasginp->capvt;
    pasg = (ASG *)pasginp->pasg;
    pcapvt->rtndata = *pdata; /*structure copy*/
    if(pdata->severity==INVALID_ALARM) {
	pasg->inpBad |= (1<<pasginp->inpIndex);
    } else {
	pasg->inpBad &= ~((1<<pasginp->inpIndex));
        pasg->pavalue[pasginp->inpIndex] = pdata->value;
    }
    pasg->inpChanged |= (1<<pasginp->inpIndex);
    if(!caInitializing) asComputeAsg(pasg);
    if(Ilocked) FASTUNLOCK(&asLock);
}

static void asCaTask(void)
{
    ASG		*pasg;
    ASGINP	*pasginp;
    CAPVT	*pcapvt;

    taskwdInsert(taskIdSelf(),NULL,NULL);
    SEVCHK(ca_task_initialize(),"ca_task_initialize");
    caInitializing = TRUE;
    pasg = (ASG *)ellFirst(&pasbase->asgList);
    while(pasg) {
	pasginp = (ASGINP *)ellFirst(&pasg->inpList);
	while(pasginp) {
	    pasg->inpBad |= (1<<pasginp->inpIndex);
	    pcapvt = pasginp->capvt = asCalloc(1,sizeof(CAPVT));
	    SEVCHK(ca_build_and_connect(pasginp->inp,TYPENOTCONN,0,
		&pcapvt->chid,0,connectCallback,pasginp),
		"ca_build_and_connect");
	    SEVCHK(ca_add_event(DBR_STS_DOUBLE,pcapvt->chid,
		eventCallback,pasginp,&pcapvt->evid),
		"ca_add_event");
	    pasginp = (ASGINP *)ellNext((ELLNODE *)pasginp);
	}
	pasg = (ASG *)ellNext((ELLNODE *)pasg);
    }
    FASTLOCK(&asLock);
    asComputeAllAsg();
    caInitializing = FALSE;
    FASTUNLOCK(&asLock);
    SEVCHK(ca_pend_event(0.0),"ca_pend_event");
    exit(-1);
}
    
void asCaStart(void)
{
    taskid = taskSpawn("asCa",CA_CLIENT_PRI-1,VX_FP_TASK,CA_CLIENT_STACK,
	(FUNCPTR)asCaTask,0,0,0,0,0,0,0,0,0,0);
    if(taskid==ERROR) {
	errMessage(0,"asCaStart: taskSpawn Failure\n");
    } else {
	taskDelay(1);
    }
}

void asCaStop(void)
{
    ASG		*pasg;
    ASGINP	*pasginp;
    CAPVT	*pcapvt;
    STATUS	status;

    if(taskid==0 || taskid==ERROR) return;
    taskwdRemove(taskid);
    status = taskDelete(taskid);
    if(status!=OK) errMessage(0,"asCaStop: taskDelete Failure\n");
    while(taskIdVerify(taskid)==OK) {
	taskDelay(5);
    }
    pasg = (ASG *)ellFirst(&pasbase->asgList);
    while(pasg) {
	pasginp = (ASGINP *)ellFirst(&pasg->inpList);
	while(pasginp) {
	    free(pasginp->capvt);
	    pasginp = (ASGINP *)ellNext((ELLNODE *)pasginp);
	}
	pasg = (ASG *)ellNext((ELLNODE *)pasg);
    }
}
