/* dbNotify.c */
/* base/src/db $Id$ */
/*
 *      Author: 	Marty Kraimer
 *      Date:           03-30-95
 * Extracted from dbLink.c
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
 * .01  03-30-95	mrk	Extracted from dbLink.c
 */

#include	<vxWorks.h>
#include	<stdlib.h>
#include	<stdarg.h>
#include	<stdio.h>
#include	<taskLib.h>

#include	<dbDefs.h>
#include	<fast_lock.h>
#include	<dbBase.h>
#include	<dbAccess.h>
#include	<dbStaticLib.h>
#include	<dbScan.h>
#include	<dbCommon.h>
#include	<errMdef.h>

static void notifyCallback(CALLBACK *pcallback)
{
    PUTNOTIFY	*ppn=NULL;
    long	status;

    callbackGetUser(ppn,pcallback);
    if(ppn->cmd==notifyCmdRepeat) {
	status = dbPutNotify(ppn);
    } else if(ppn->cmd==notifyCmdCallUser) {
	ppn->cmd = notifyUserCalled;
	(ppn->userCallback)(ppn);
    } else {/*illegal request*/
	recGblRecordError(-1,ppn->paddr->precord,
		"dbNotifyCompletion: illegal callback request");
    }
}

static void notifyCancel(PUTNOTIFY *ppn)
{
    struct dbCommon *precord = ppn->list;

    while(precord) {
	void	*pnext;

	if(precord->rpro) {
	    precord->rpro = FALSE;
	    scanOnce(precord);
	}
	precord->ppn = NULL;
	pnext = precord->ppnn;
	precord->ppnn = NULL;
	precord = pnext;
    }
    ppn->list = NULL;
}

void dbNotifyCancel(PUTNOTIFY *ppn)
{
    struct dbCommon *precord = ppn->list;

    if(!precord) return;
    dbScanLock(precord);
    notifyCancel(ppn);
    if(ppn->cmd!=notifyCmdNull && ppn->cmd!=notifyUserCalled) {
	/*Bad lets try one time to  wait*/
	dbScanUnlock(precord);
	taskDelay(10);
	dbScanLock(precord);
    }
    if(ppn->cmd!=notifyCmdNull && ppn->cmd!=notifyUserCalled) {
	epicsPrintf("dbNotifyCancel called while callback requested"
		" but not called\n");
    }
    dbScanUnlock(precord);
}

static void issueCallback(PUTNOTIFY *ppn, notifyCmd cmd)
{
    if(ppn->cmd!=notifyCmdNull) return;
    ppn->cmd = cmd;
    notifyCancel(ppn);
    callbackRequest(&ppn->callback);
}

long dbPutNotify(PUTNOTIFY *ppn)
{
    struct dbAddr *paddr = ppn->paddr;
    short         dbrType = ppn->dbrType;
    void          *pbuffer = ppn->pbuffer;
    long          nRequest = ppn->nRequest;
    long	  status=0;
    struct fldDes *pfldDes=(struct fldDes *)(paddr->pfldDes);
    struct dbCommon *precord = (struct dbCommon *)(paddr->precord);

    callbackSetCallback(notifyCallback,&ppn->callback);
    callbackSetUser(ppn,&ppn->callback);
    callbackSetPriority(priorityLow,&ppn->callback);
    /*check for putField disabled*/
    if(precord->disp) {
	if((void *)(&precord->disp) != paddr->pfield) {
	    ppn->status = S_db_putDisabled;
	    issueCallback(ppn,notifyCmdCallUser);
	    return(S_db_putDisabled);
	}
    }
    ppn->status = 0;
    ppn->cmd = notifyCmdNull;
    ppn->nwaiting = 1;
    ppn->rescan = FALSE;
    dbScanLock(precord);
    status=dbPut(paddr,dbrType,pbuffer,nRequest);
    ppn->status = status;
    if(status==0){
       	if((paddr->pfield==(void *)&precord->proc)
	||(pfldDes->process_passive && precord->scan==0)) {
	    if(precord->ppn) {
		/*record already has attached ppn. Blocked*/

		ppn->status = status = S_db_Blocked;
    		dbScanUnlock(precord);
		return(status);
	    }
	    precord->ppn = ppn;
	    precord->ppnn = NULL;
	    ppn->list = precord;
	    if(precord->pact) {/*blocked wait for dbNotifyCompletion*/
		ppn->rescan = TRUE;
    		dbScanUnlock(precord);
		return(S_db_Pending);
	    }
	    status=dbProcess(precord);
	    if(status!=0) {
		ppn->status = status;
	        issueCallback(ppn,notifyCmdCallUser);
	    }
	} else { /*Make callback immediately*/
	    issueCallback(ppn,notifyCmdCallUser);
	}
    }
    dbScanUnlock(precord);
    return(S_db_Pending);
}

void dbNotifyCompletion(PUTNOTIFY *ppn)
{

    if(ppn->status!=0) {
	issueCallback(ppn,notifyCmdCallUser);
	return;
    }
    /*decrement number of records being waited on*/
    if(ppn->nwaiting<=0) {
	recGblRecordError(-1,ppn->paddr->precord,"dbNotifyCompletion: nwaiting<-0 LOGIC");
	return;
    }
    if(--ppn->nwaiting == 0) {/*original request completed*/
	if(ppn->rescan) {
	    issueCallback(ppn,notifyCmdRepeat);
	} else {
	    issueCallback(ppn,notifyCmdCallUser);
	}
    }
}

/*Remove all nonactive records from put notify list*/
void cleanPpList(PUTNOTIFY *ppn)
{
    struct dbCommon 	*precord = ppn->list;
    struct dbCommon 	*pnext;
    struct dbCommon 	*pprev=NULL;

    while(precord) {
	pnext = precord->ppnn;
	if(!precord->pact) {
	    if(!pprev) ppn->list = pnext; else pprev->ppnn = pnext;
	    precord->ppn = NULL;
	    precord->ppnn = NULL;
	} else {
	    pprev = precord;
	}
	precord = pnext;
    }
}

void dbNotifyAdd(struct dbCommon *pfrom, struct dbCommon *pto)
{
    PUTNOTIFY *ppn = pfrom->ppn;

    if(pto->ppn) cleanPpList(pto->ppn); /* clean list before giving up*/
    if (pto->ppn) { /*already being used. Abandon request*/
	    ppn->status = S_db_Blocked;
	    dbNotifyCompletion(ppn);
    } else {
	    ppn->nwaiting++;
	    pto->ppn = pfrom->ppn;
	    pto->ppnn = ppn->list;
	    ppn->list = pto;
	    /*If already active must redo*/
	    if(pto->pact) ppn->rescan = TRUE;
    }
}
