/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbNotify.c */
/* base/src/db $Id$ */
/*
 *      Author: 	Marty Kraimer
 *      Date:           03-30-95
 * Extracted from dbLink.c
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "dbDefs.h"
#include "ellLib.h"
#include "epicsAssert.h"
#include "epicsEvent.h"
#include "epicsMutex.h"
#include "epicsThread.h"
#include "errlog.h"
#include "errMdef.h"
#include "dbBase.h"
#include "dbStaticLib.h"
#include "dbFldTypes.h"
#include "link.h"
#include "dbCommon.h"
#define epicsExportSharedSymbols
#include "callback.h"
#include "dbAddr.h"
#include "dbScan.h"
#include "dbLock.h"
#include "callback.h"
#include "dbAccessDefs.h"
#include "recGbl.h"
#include "dbNotify.h"
#include "epicsTime.h"
#include "cantProceed.h"

#define STATIC static
/*structure attached to ppnr field of each record*/
typedef struct putNotifyRecord {
    ellCheckNode  waitNode;
    ELLLIST  restartList; /*list of putNotifys to restart*/
    dbCommon *precord;
}putNotifyRecord;

/*putNotify.state values */
typedef enum {
    putNotifyNotActive,
    putNotifyWaitForRestart,
    putNotifyRestartCallbackRequested,
    putNotifyRestartInProgress,
    putNotifyPutInProgress,
    putNotifyUserCallbackRequested,
    putNotifyUserCallbackActive
}putNotifyState;

/* putNotify groups can span locksets if links are dynamically modified*/
/* Thus a global lock is taken while putNotify fields are accessed     */
STATIC epicsMutexId notifyLock = 0;

/*Local routines*/
STATIC void restartCheck(putNotifyRecord *ppnr);
STATIC void notifyCallback(CALLBACK *pcallback);
STATIC void putNotifyPvt(putNotify *ppn,dbCommon *precord);

#define ellSafeAdd(list,listnode) \
{ \
    assert((listnode)->isOnList==0); \
    ellAdd((list),&((listnode)->node)); \
    (listnode)->isOnList=1; \
}

#define ellSafeDelete(list,listnode) \
{ \
    assert((listnode)->isOnList); \
    ellDelete((list),&((listnode)->node)); \
    (listnode)->isOnList=0; \
}

STATIC void restartCheck(putNotifyRecord *ppnr)
{
    dbCommon *precord = ppnr->precord;
    putNotify *pfirst;
    
    assert(precord->ppn);
    pfirst = (putNotify *)ellFirst(&ppnr->restartList);
    if(!pfirst) {
        precord->ppn = 0;
        return;
    }
    assert(pfirst->state==putNotifyWaitForRestart);
    /* remove pfirst from restartList */
    ellSafeDelete(&ppnr->restartList,&pfirst->restartNode);
    /*make pfirst owner of the record*/
    precord->ppn = pfirst;
    /* request callback for pfirst */
    pfirst->state = putNotifyRestartCallbackRequested;
    callbackRequest(&pfirst->callback);
}

STATIC void putNotifyPvt(putNotify *ppn,dbCommon *precord)
{
    long	status=0;
    dbFldDes	*pfldDes=(dbFldDes *)(ppn->paddr->pfldDes);

    if(precord->ppn && ppn->state!=putNotifyRestartCallbackRequested) {
        /*another putNotify owns the record */
        ppn->state = putNotifyWaitForRestart; 
        ellSafeAdd(&precord->ppnr->restartList,&ppn->restartNode);
        epicsMutexUnlock(notifyLock);
        dbScanUnlock(precord);
        return;
    } else if(precord->ppn){
        assert(precord->ppn==ppn);
        assert(ppn->state==putNotifyRestartCallbackRequested);
    }
    if(precord->pact) {
        precord->ppn = ppn;
        ellSafeAdd(&ppn->waitList,&precord->ppnr->waitNode);
        ppn->state = putNotifyRestartInProgress; 
        epicsMutexUnlock(notifyLock);
        dbScanUnlock(precord);
        return;
    }
    status=dbPut(ppn->paddr,ppn->dbrType,ppn->pbuffer,ppn->nRequest);
    ppn->status = (status==0) ? putNotifyOK : putNotifyError;
    /* Check to see if dbProcess should not be called */
    if(!status /*dont process if dbPut returned error */
    &&((ppn->paddr->pfield==(void *)&precord->proc) /*If PROC call dbProcess*/
       || (pfldDes->process_passive && precord->scan==0))) {
        precord->ppn = ppn;
        ellSafeAdd(&ppn->waitList,&precord->ppnr->waitNode);
        ppn->state = putNotifyPutInProgress;
        epicsMutexUnlock(notifyLock);
        dbProcess(precord);
        dbScanUnlock(precord);
        return;
    }
    if(ppn->state==putNotifyRestartCallbackRequested) {
        restartCheck(precord->ppnr);
    }
    ppn->state = putNotifyUserCallbackActive;
    assert(precord->ppn!=ppn);
    epicsMutexUnlock(notifyLock);
    dbScanUnlock(precord);
    (*ppn->userCallback)(ppn);
    epicsMutexMustLock(notifyLock);
    if(ppn->requestCancel) epicsEventSignal(*ppn->pcancelEvent);
    ppn->state = putNotifyNotActive;
    epicsMutexUnlock(notifyLock); 
    return;
}

STATIC void notifyCallback(CALLBACK *pcallback)
{
    putNotify	*ppn=NULL;
    dbCommon	*precord;

    callbackGetUser(ppn,pcallback);
    precord = ppn->paddr->precord;
    dbScanLock(precord);
    epicsMutexMustLock(notifyLock);
    assert(precord->ppnr);
    assert(ppn->state==putNotifyRestartCallbackRequested
          || ppn->state==putNotifyUserCallbackRequested);
    assert(ellCount(&ppn->waitList)==0);
    if(ppn->requestCancel) {
        ppn->status = putNotifyCanceled;
        if(ppn->state==putNotifyRestartCallbackRequested) {
            restartCheck(precord->ppnr);
        }
        ppn->state = putNotifyNotActive;
        epicsMutexUnlock(notifyLock);
        dbScanUnlock(precord);
        assert(ppn->pcancelEvent);
        epicsEventSignal(*ppn->pcancelEvent);
        return;
    }
    if(ppn->state==putNotifyRestartCallbackRequested) {
        putNotifyPvt(ppn,precord);
        return;
    }
    /* All done. Clean up and call userCallback */
    ppn->state = putNotifyUserCallbackActive;
    assert(precord->ppn!=ppn);
    epicsMutexUnlock(notifyLock);
    dbScanUnlock(precord);
    (*ppn->userCallback)(ppn);
    epicsMutexMustLock(notifyLock);
    if(ppn->requestCancel) epicsEventSignal(*ppn->pcancelEvent);
    ppn->state = putNotifyNotActive;
    epicsMutexUnlock(notifyLock); 
}

void epicsShareAPI dbPutNotify(putNotify *ppn)
{
    dbCommon	*precord = ppn->paddr->precord;
    short	dbfType = ppn->paddr->field_type;
    long	status=0;

    assert(precord);
    /*check for putField disabled*/
    if(precord->disp) {
        if((void *)(&precord->disp) != ppn->paddr->pfield) {
           ppn->status = putNotifyPutDisabled;
           (*ppn->userCallback)(ppn);
           return;
        }
    }
    /* Must handle DBF_XXXLINKs as special case.
     * Only dbPutField will change link fields. 
     * Also the record is not processed as a result
    */
    if(dbfType>=DBF_INLINK && dbfType<=DBF_FWDLINK) {
	status=dbPutField(ppn->paddr,ppn->dbrType,ppn->pbuffer,ppn->nRequest);
        ppn->status = (status==0) ? putNotifyOK : putNotifyError;
        (*ppn->userCallback)(ppn);
        return;
    }
    dbScanLock(precord);
    epicsMutexMustLock(notifyLock);
    ppn->status = 0;
    ppn->state = putNotifyNotActive;
    ppn->requestCancel = 0;
    callbackSetCallback(notifyCallback,&ppn->callback);
    callbackSetUser(ppn,&ppn->callback);
    callbackSetPriority(priorityLow,&ppn->callback);
    ellInit(&ppn->waitList);
    if(!precord->ppnr) {/* make sure record has a putNotifyRecord*/
        precord->ppnr = dbCalloc(1,sizeof(putNotifyRecord));
        precord->ppnr->precord = precord;
        ellInit(&precord->ppnr->restartList);
    }
    putNotifyPvt(ppn,precord);
}

void epicsShareAPI dbPutNotifyInit(void)
{
    notifyLock = epicsMutexMustCreate();
}

void epicsShareAPI dbNotifyCancel(putNotify *ppn)
{
    dbCommon	*precord = ppn->paddr->precord;
    putNotifyState state;

    if(ppn->state==putNotifyNotActive) return;
    assert(precord);
    dbScanLock(precord);
    epicsMutexMustLock(notifyLock);
    state = ppn->state;
    /*If callback is scheduled or active wait for it to complete*/
    if(state==putNotifyUserCallbackRequested
    || state==putNotifyRestartCallbackRequested
    || state==putNotifyUserCallbackActive) {
        epicsEventId eventId;
        eventId = epicsEventCreate(epicsEventEmpty);
        ppn->pcancelEvent = &eventId;
        ppn->requestCancel = 1;
        epicsMutexUnlock(notifyLock);
        dbScanUnlock(precord);
        epicsEventWait(eventId);
        dbScanLock(precord);
        epicsMutexMustLock(notifyLock);
        ppn->pcancelEvent = 0;
        epicsEventDestroy(eventId);
        epicsMutexUnlock(notifyLock);
        dbScanUnlock(precord);
        return;
    }
    switch(state) {
    case putNotifyNotActive: break;
    case putNotifyWaitForRestart:
        assert(precord->ppn);
        assert(precord->ppn!=ppn);
        ellSafeDelete(&precord->ppnr->restartList,&ppn->restartNode);
        break;
    case putNotifyRestartInProgress:
    case putNotifyPutInProgress:
        { /*Take all records out of wait list */
            putNotifyRecord *ppnrWait;

            while((ppnrWait = (putNotifyRecord *)ellFirst(&ppn->waitList))){
                ellSafeDelete(&ppn->waitList,&ppnrWait->waitNode);
                restartCheck(ppnrWait);
            }
        }
        if(precord->ppn==ppn) restartCheck(precord->ppnr);
        break;
    default:
        printf("dbNotify: illegal state for notifyCallback\n");
    }
    ppn->state = putNotifyNotActive;
    epicsMutexUnlock(notifyLock);
    dbScanUnlock(precord);
}

void epicsShareAPI dbNotifyCompletion(dbCommon *precord)
{
    putNotify	*ppn = precord->ppn;

    epicsMutexMustLock(notifyLock);
    assert(ppn);
    assert(precord->ppnr);
    if(ppn->state!=putNotifyRestartInProgress
    && ppn->state!=putNotifyPutInProgress) {
        epicsMutexUnlock(notifyLock);
        return;
    }
    ellSafeDelete(&ppn->waitList,&precord->ppnr->waitNode);
    if((ellCount(&ppn->waitList)!=0)) {
        restartCheck(precord->ppnr);
    } else if(ppn->state == putNotifyPutInProgress) {
        ppn->state = putNotifyUserCallbackRequested;
        restartCheck(precord->ppnr);
        callbackRequest(&ppn->callback);
    } else if(ppn->state == putNotifyRestartInProgress) {
        ppn->state = putNotifyRestartCallbackRequested;
        callbackRequest(&ppn->callback);
    } else {
        cantProceed("dbNotifyCompletion illegal state");
    }
    epicsMutexUnlock(notifyLock);
}

void epicsShareAPI dbNotifyAdd(dbCommon *pfrom, dbCommon *pto)
{
    putNotify *ppn = pfrom->ppn;

    if(pto->pact) return; /*if active it will not be processed*/
    epicsMutexMustLock(notifyLock);
    if(!pto->ppnr) {/* make sure record has a putNotifyRecord*/
        pto->ppnr = dbCalloc(1,sizeof(putNotifyRecord));
        pto->ppnr->precord = pto;
        ellInit(&pto->ppnr->restartList);
    }
    assert(ppn);
    if(!(pto->ppn) 
    && (ppn->state==putNotifyPutInProgress)
    && (pto!=ppn->paddr->precord)) {
        pto->ppn = pfrom->ppn;
        ellSafeAdd(&pfrom->ppn->waitList,&pto->ppnr->waitNode);
    }
    epicsMutexUnlock(notifyLock);
}

typedef struct tpnInfo {
    epicsEventId callbackDone;
    putNotify    *ppn;
}tpnInfo;

STATIC void dbtpnCallback(putNotify *ppn)
{
    putNotifyStatus status = ppn->status;
    tpnInfo         *ptpnInfo = (tpnInfo *)ppn->usrPvt;

    if(status==0)
	printf("dbtpnCallback: success record=%s\n",ppn->paddr->precord->name);
    else
        printf("%s dbtpnCallback putNotify.status %d\n",ppn->paddr->precord->name,(int)status);
    epicsEventSignal(ptpnInfo->callbackDone);
}

static void tpnThread(void *pvt)
{
    tpnInfo   *ptpnInfo = (tpnInfo *)pvt;
    putNotify *ppn = (putNotify *)ptpnInfo->ppn;

    dbPutNotify(ppn);
    epicsEventWait(ptpnInfo->callbackDone);
    dbNotifyCancel(ppn);
    epicsEventDestroy(ptpnInfo->callbackDone);
    free((void *)ppn->paddr);
    free(ppn);
    free(ptpnInfo);
}

long epicsShareAPI dbtpn(char	*pname,char *pvalue)
{
    long	status;
    tpnInfo     *ptpnInfo;
    DBADDR	*pdbaddr=NULL;
    putNotify	*ppn=NULL;
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
    ppn = dbCalloc(1,sizeof(putNotify));
    ppn->paddr = pdbaddr;
    ppn->pbuffer = psavevalue;
    ppn->nRequest = 1;
    ppn->dbrType = DBR_STRING;
    ppn->userCallback = dbtpnCallback;
    ptpnInfo = dbCalloc(1,sizeof(tpnInfo));
    ptpnInfo->ppn = ppn;
    ptpnInfo->callbackDone = epicsEventCreate(epicsEventEmpty);
    ppn->usrPvt = ptpnInfo;
    epicsThreadCreate("dbtpn",epicsThreadPriorityHigh,
        epicsThreadGetStackSize(epicsThreadStackMedium),
        tpnThread,ptpnInfo);
    return(0);
}

int epicsShareAPI dbNotifyDump(void)
{
    epicsMutexLockStatus lockStatus;
    dbRecordType *pdbRecordType;
    dbRecordNode *pdbRecordNode;
    dbCommon     *precord;
    putNotify    *ppn;
    putNotify    *ppnRestart;
    putNotifyRecord *ppnrWait;
    int itry;
   
    
    for(itry=0; itry<100; itry++) {
        lockStatus = epicsMutexTryLock(notifyLock);
        if(lockStatus==epicsMutexLockOK) break;
        epicsThreadSleep(.05);
    }
    for(pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
    pdbRecordType;
    pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
        for (pdbRecordNode=(dbRecordNode *)ellFirst(&pdbRecordType->recList);
        pdbRecordNode;
        pdbRecordNode = (dbRecordNode *)ellNext(&pdbRecordNode->node)) {
            precord = pdbRecordNode->precord;
            if(!precord->ppn) continue;
            if(!precord->ppnr) continue;
            if(precord->ppn->paddr->precord != precord) continue;
            ppn = precord->ppn;
            printf("%s state %d ppn %p\n  waitList\n",
                precord->name,ppn->state,(void*)ppn);
            ppnrWait = (putNotifyRecord *)ellFirst(&ppn->waitList);
            while(ppnrWait) {
                printf("    %s pact %d\n",
                    ppnrWait->precord->name,ppnrWait->precord->pact);
                ppnrWait = (putNotifyRecord *)ellNext(&ppnrWait->waitNode.node);
            }
            printf("  restartList\n");
            ppnRestart = (putNotify *)ellFirst(&precord->ppnr->restartList);
            while(ppnRestart) {
                printf("    %p\n", (void *)ppnRestart);
                ppnRestart = (putNotify *)ellNext(&ppnRestart->restartNode.node);
            }
        }
    }
    if(lockStatus==epicsMutexLockOK) epicsMutexUnlock(notifyLock);
    return(0);
}
