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

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "osiSem.h"
#include "errlog.h"
#include "errMdef.h"
#include "ellLib.h"
#include "tsStamp.h"
#include "dbBase.h"
#include "dbStaticLib.h"
#include "dbFldTypes.h"
#include "link.h"
#include "dbCommon.h"
#include "dbScan.h"
#include "dbLock.h"
#include "callback.h"
#include "dbAddr.h"
#include "dbAccess.h"
#include "recGbl.h"
#define epicsExportSharedSymbols
#include "dbNotify.h"

/*NODE structure attached to ppnn field of each record in list*/
typedef struct pnWaitNode {
	ELLNODE		node;
	struct dbCommon	*precord;
} PNWAITNODE;

/*Local routines*/
static void restartAdd(PUTNOTIFY *ppnto, PUTNOTIFY *ppnfrom);
static void waitAdd(struct dbCommon *precord,PUTNOTIFY *ppn);
static long putNotify(PUTNOTIFY *ppn);
static void notifyCallback(CALLBACK *pcallback);
static void notifyCancel(PUTNOTIFY *ppn);
static void issueCallback(PUTNOTIFY *ppn);

/*callbackState values */
typedef enum {
	callbackNotActive,callbackActive,callbackCanceled}callbackState;

/* This module provides code to handle put notify. If a put causes a record to
 * be processed, then a user supplied callback is called when that record
 * and all records processed because of that record complete processing.
 * For asynchronous records completion means completion of the asyn phase.
 *
 * User code calls dbPutNotify and dbNotifyCancel.
 *
 * The other global routines (dbNotifyAdd and dbNotifyCompletion) are called by:
 *
 *  dbAccess.c
 *	dbScanPassive and dbScanLink
 *		call dbNotifyAdd just before calling dbProcess
 *	dbProcess
 *		Calls dbNotifyCompletion if record is disabled.
 *	recGbl
 *		recGblFwdLink calls dbNotifyCompletion
 */

/* Two fields in dbCommon are used for put notify.
 *	ppn
 *		If a record is part of a put notify group,
 *		This field is tha address of the associated PUTNOTIFY.
 *		As soon as a record completes processing the field is set NULL
 *	ppnn
 *		Address of an PNWAITNODE for keeping list of records in a
 *		put notify group. Once an PNWAITNODE is allocated for a record,
 *		it is never freed under the assumption that once a record
 *		becomes part of a put notify group it is likely to become
 *		one again in the future.
 */

static void restartAdd(PUTNOTIFY *ppnto, PUTNOTIFY *ppnfrom)
{
    ppnfrom->restartNode.ppn = ppnfrom;
    ppnfrom->restartNode.ppnrestartList = ppnto;
    ppnfrom->restart = TRUE;
    ellAdd(&ppnto->restartList,&ppnfrom->restartNode.node);
}

static void waitAdd(struct dbCommon *precord,PUTNOTIFY *ppn)
{
    PNWAITNODE	*ppnnode;

    if(!precord->ppnn) precord->ppnn = dbCalloc(1,sizeof(PNWAITNODE));
    ppnnode = precord->ppnn;
    ppnnode->precord = precord;
    precord->ppn = ppn;
    ellAdd(&ppn->waitList,&ppnnode->node);
}



long dbPutNotify(PUTNOTIFY *ppn)
{
    long	status;
    DBADDR	*paddr = ppn->paddr;
    dbCommon	*precord = (dbCommon *)paddr->precord;
    short	dbfType = paddr->field_type;

    if(dbfType>=DBF_INLINK && dbfType<=DBF_FWDLINK) {
	/*Initialize everything in PUTNOTIFY except restart list and node*/
        if(!ppn->callback) ppn->callback = dbCalloc(1,sizeof(CALLBACK));
	callbackSetCallback(notifyCallback,ppn->callback);
	callbackSetUser(ppn,ppn->callback);
	callbackSetPriority(priorityLow,ppn->callback);
	ppn->status = 0;
	ppn->restart = FALSE;
	ppn->callbackState = callbackNotActive;
	status=dbPutField(paddr,ppn->dbrType,ppn->pbuffer,ppn->nRequest);
	ppn->status = status;
	issueCallback(ppn);
	return(S_db_Pending);
    }
    dbScanLock(precord);
    ellInit(&ppn->restartList);
    memset(&ppn->restartNode,'\0',sizeof(PNRESTARTNODE));
    status = putNotify(ppn);
    dbScanUnlock(precord);
    return(status);
}

static long putNotify(PUTNOTIFY *ppn)
{
    struct dbAddr *paddr = ppn->paddr;
    short	dbrType = ppn->dbrType;
    void	*pbuffer = ppn->pbuffer;
    long	nRequest = ppn->nRequest;
    long	status=0;
    dbFldDes	*pfldDes=(dbFldDes *)(paddr->pfldDes);
    struct dbCommon *precord = (struct dbCommon *)(paddr->precord);

    if(precord->ppn == ppn) {
	return(S_db_Blocked);
    }
    /*Initialize everything in PUTNOTIFY except restart list and node*/
    if(!ppn->callback) ppn->callback = dbCalloc(1,sizeof(CALLBACK));
    callbackSetCallback(notifyCallback,ppn->callback);
    callbackSetUser(ppn,ppn->callback);
    callbackSetPriority(priorityLow,ppn->callback);
    ppn->status = 0;
    ppn->restart = FALSE;
    ppn->callbackState = callbackNotActive;
    ellInit(&ppn->waitList);
    /*check for putField disabled*/
    if(precord->disp) {
	if((void *)(&precord->disp) != paddr->pfield) {
	    ppn->status = S_db_putDisabled;
	    issueCallback(ppn);
	    goto ret_pending;
	}
    }
    if(precord->ppn) {/*Another put notify is in progress*/
	restartAdd(precord->ppn,ppn);
	goto ret_pending;
    }
    if(precord->pact) {/*blocked wait for dbNotifyCompletion*/
	precord->ppn = ppn;
	waitAdd(precord,ppn);
	restartAdd(precord->ppn,ppn);
	goto ret_pending;
    }
    status=dbPut(paddr,dbrType,pbuffer,nRequest);
    if(status) {
	ppn->status = status;
	issueCallback(ppn);
	goto ret_pending;
    } 
    /*check for no processing required*/
    if(!(paddr->pfield==(void *)&precord->proc)
    && (!pfldDes->process_passive || precord->scan!=0)) {
	issueCallback(ppn);
	goto ret_pending;
    }
    /*Add record to waitlist*/
    waitAdd(precord,ppn);
    status=dbProcess(precord);
    if(status!=0) {
	ppn->status = status;
    }
ret_pending:
    return(S_db_Pending);
}

static void notifyCallback(CALLBACK *pcallback)
{
    PUTNOTIFY		*ppn=NULL;
    struct dbCommon	*precord;

    callbackGetUser(ppn,pcallback);
    precord = ppn->paddr->precord;
    dbScanLock(precord);
    if(ppn->callbackState==callbackCanceled) {
	dbScanUnlock(precord);
	ppn->restart = FALSE;
	ppn->callbackState = callbackNotActive;
	semBinaryGive((semBinaryId)ppn->waitForCallback);
	return;
    }
    if(ppn->callbackState==callbackActive) {
	if(ppn->restart) {
	    putNotify(ppn);
	    dbScanUnlock(precord);
	} else {
	    dbScanUnlock(precord);
	    (ppn->userCallback)(ppn);
	}
    }
}

static void issueCallback(PUTNOTIFY *ppn)
{
    notifyCancel(ppn);
    ppn->callbackState = callbackActive;
    callbackRequest(ppn->callback);
}

void epicsShareAPI dbNotifyCancel(PUTNOTIFY *ppn)
{
    struct dbCommon *precord = ppn->paddr->precord;;

    dbScanLock(precord);
    notifyCancel(ppn);
    if(ppn->callbackState == callbackActive) {
	ppn->waitForCallback = (void *)semBinaryMustCreate(semFull);
	ppn->callbackState = callbackCanceled;
	dbScanUnlock(precord);
	if(semBinaryTakeTimeout((semBinaryId)ppn->waitForCallback,10.0)!=semTakeOK) {
	    errlogPrintf("dbNotifyCancel had semTake timeout\n");
	    ppn->callbackState = callbackNotActive;
	}
	semBinaryDestroy((semBinaryId)ppn->waitForCallback);
    } else {
	dbScanUnlock(precord);
    }
}


static void notifyCancel(PUTNOTIFY *ppn)
{
    PNWAITNODE		*ppnnode;
    struct dbCommon	*precord;
    PNRESTARTNODE	*pfirst;

    /*Remove everything on waitList*/
    while((ppnnode = (PNWAITNODE *)ellLast(&ppn->waitList))) {
	precord = ppnnode->precord;
	precord->ppn = NULL;
	ellDelete(&ppn->waitList,&ppnnode->node);
    }
    /*If on restartList remove it*/
    if(ppn->restartNode.ppnrestartList) 
	ellDelete(&ppn->restartNode.ppnrestartList->restartList,
		&ppn->restartNode.node);
    /*If this ppn has a restartList move it */
    if((pfirst = (PNRESTARTNODE *)ellFirst(&ppn->restartList))) {
	PNRESTARTNODE	*pnext;
	PUTNOTIFY	*pfirstppn;

	pfirstppn = pfirst->ppn;
	ellConcat(&pfirstppn->restartList,&ppn->restartList);
	pnext = (PNRESTARTNODE *)ellFirst(&pfirstppn->restartList);
	while(pnext) {
	    pnext->ppnrestartList = pfirstppn;
	    pnext = (PNRESTARTNODE *)ellNext(&pnext->node);
	}
	pfirstppn->restart = TRUE;
	pfirstppn->callbackState = callbackActive;
	callbackRequest(pfirstppn->callback);
    }
    memset(&ppn->restartNode,'\0',sizeof(PNRESTARTNODE));
}

void epicsShareAPI dbNotifyCompletion(struct dbCommon *precord)
{
    PUTNOTIFY	*ppn = precord->ppn;
    PNWAITNODE	*ppnnode = precord->ppnn;;

    ellDelete(&ppn->waitList,&ppnnode->node);
    precord->ppn = NULL;
    if((ppn->status!=0) || (ellCount(&ppn->waitList)==0)) 
	issueCallback(ppn);
}

void epicsShareAPI dbNotifyAdd(struct dbCommon *pfrom, struct dbCommon *pto)
{
    PUTNOTIFY *pfromppn = pfrom->ppn;
    PUTNOTIFY *ppn=NULL;

    if(pto->ppn == pfrom->ppn) return; /*Aready in same set*/
    if (pto->ppn) { /*already being used. Abandon request*/
	notifyCancel(pfrom->ppn);
	restartAdd(pto->ppn,pfromppn);
	return;
    } else {
	/*If already active must redo*/
	if(pto->pact) {
	    ppn = pfrom->ppn;
	    notifyCancel(pfrom->ppn);
	    waitAdd(pto,ppn);
	    restartAdd(pto->ppn,ppn);
	} else {
	    waitAdd(pto,pfrom->ppn);
	}
    }
}

static void dbtpnCallback(PUTNOTIFY *ppn)
{
    DBADDR	*pdbaddr = ppn->paddr;
    long	status = ppn->status;

    if(status==S_db_Blocked)
	printf("dbtpnCallback: blocked record=%s\n",ppn->paddr->precord->name);
    else if(status==0)
	printf("dbtpnCallback: success record=%s\n",ppn->paddr->precord->name);
    else
	recGblRecordError(status,pdbaddr->precord,"dbtpnCallback");
    free((void *)pdbaddr);
    free(ppn);
}

long dbtpn(char	*pname,char *pvalue)
{
    long	status;
    DBADDR	*pdbaddr=NULL;
    PUTNOTIFY	*ppn=NULL;
    char	*psavevalue;
    int		len;

    len = strlen(pvalue);
    /*allocate space for value immediately following DBADDR*/
    pdbaddr = dbCalloc(1,sizeof(DBADDR) + len+1);
    psavevalue = (char *)(pdbaddr + 1);
    strcpy(psavevalue,pvalue);
    status = dbNameToAddr(pname,pdbaddr);
    if(status) {
	errMessage(status, "dbtpn: dbNameToAddr");
	free((void *)pdbaddr);
	return(-1);
    }
    ppn = dbCalloc(1,sizeof(PUTNOTIFY));
    ppn->paddr = pdbaddr;
    ppn->pbuffer = psavevalue;
    ppn->nRequest = 1;
    ppn->dbrType = DBR_STRING;
    ppn->userCallback = dbtpnCallback;
    status = dbPutNotify(ppn);
    if(status==S_db_Pending) {
	printf("dbtpn: Pending nwaiting=%d\n",ellCount(&ppn->waitList));
	return(0);
    }
    if(status==S_db_Blocked) {
	printf("dbtpn: blocked record=%s\n",pname);
    } else if(status) {
    	errMessage(status, "dbtpn");
    }
    return(0);
}
