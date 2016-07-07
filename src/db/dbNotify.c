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

/*structure attached to ppnr field of each record*/
typedef struct putNotifyRecord {
    ellCheckNode  waitNode;
    ELLLIST  restartList; /*list of putNotifys to restart*/
    dbCommon *precord;
}putNotifyRecord;

#define MAGIC 0xfedc0123
typedef struct putNotifyPvt {
    ELLNODE      node;   /*For free list*/
    long         magic;
    short        state;
    CALLBACK     callback;
    ELLLIST      waitList; /*list of records for current putNotify*/
    short        cancelWait;
    short        userCallbackWait;
    epicsEventId cancelEvent;
    epicsEventId userCallbackEvent;
}putNotifyPvt;

/* putNotify groups can span locksets if links are dynamically modified*/
/* Thus a global lock is taken while putNotify fields are accessed     */
typedef struct notifyGlobal {
    epicsMutexId lock;
    ELLLIST      freeList;
}notifyGlobal;

static notifyGlobal *pnotifyGlobal = 0;

/*Local routines*/
static void putNotifyInit(putNotify *ppn);
static void putNotifyCleanup(putNotify *ppn);
static void restartCheck(putNotifyRecord *ppnr);
static void callUser(dbCommon *precord,putNotify *ppn);
static void notifyCallback(CALLBACK *pcallback);
static void putNotifyCommon(putNotify *ppn,dbCommon *precord);

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

static void putNotifyInit(putNotify *ppn)
{
    putNotifyPvt *pputNotifyPvt;

    pputNotifyPvt = (putNotifyPvt *)ellFirst(&pnotifyGlobal->freeList);
    if(pputNotifyPvt) {
        ellDelete(&pnotifyGlobal->freeList,&pputNotifyPvt->node);
    } else {
        pputNotifyPvt = dbCalloc(1,sizeof(putNotifyPvt));
        pputNotifyPvt->cancelEvent = epicsEventCreate(epicsEventEmpty);
        pputNotifyPvt->userCallbackEvent = epicsEventCreate(epicsEventEmpty);
        pputNotifyPvt->magic = MAGIC;
        pputNotifyPvt->state = putNotifyNotActive;
    }
    pputNotifyPvt->state = putNotifyNotActive;
    callbackSetCallback(notifyCallback,&pputNotifyPvt->callback);
    callbackSetUser(ppn,&pputNotifyPvt->callback);
    callbackSetPriority(priorityLow,&pputNotifyPvt->callback);
    ellInit(&pputNotifyPvt->waitList);
    ppn->status = 0;
    pputNotifyPvt->state = putNotifyNotActive;
    pputNotifyPvt->cancelWait = pputNotifyPvt->userCallbackWait = 0;
    ppn->pputNotifyPvt = pputNotifyPvt;
}

static void putNotifyCleanup(putNotify *ppn)
{
    putNotifyPvt *pputNotifyPvt = (putNotifyPvt *)ppn->pputNotifyPvt;

    pputNotifyPvt->state = putNotifyNotActive;
    ellAdd(&pnotifyGlobal->freeList,&pputNotifyPvt->node);
    ppn->pputNotifyPvt = 0;
}

static void restartCheck(putNotifyRecord *ppnr)
{
    dbCommon *precord = ppnr->precord;
    putNotify *pfirst;
    putNotifyPvt *pputNotifyPvt;
    
    assert(precord->ppn);
    pfirst = (putNotify *)ellFirst(&ppnr->restartList);
    if(!pfirst) {
        precord->ppn = 0;
        return;
    }
    pputNotifyPvt = (putNotifyPvt *)pfirst->pputNotifyPvt;
    assert(pputNotifyPvt->state==putNotifyWaitForRestart);
    /* remove pfirst from restartList */
    ellSafeDelete(&ppnr->restartList,&pfirst->restartNode);
    /*make pfirst owner of the record*/
    precord->ppn = pfirst;
    /* request callback for pfirst */
    pputNotifyPvt->state = putNotifyRestartCallbackRequested;
    callbackRequest(&pputNotifyPvt->callback);
}

static void callUser(dbCommon *precord,putNotify *ppn)
{
    putNotifyPvt *pputNotifyPvt = (putNotifyPvt *)ppn->pputNotifyPvt;

    epicsMutexUnlock(pnotifyGlobal->lock);
    dbScanUnlock(precord);
    (*ppn->userCallback)(ppn);
    epicsMutexMustLock(pnotifyGlobal->lock);
    if(pputNotifyPvt->cancelWait && pputNotifyPvt->userCallbackWait) {
        errlogPrintf("%s putNotify: both cancelWait and userCallbackWait true."
               "This is illegal\n",precord->name);
        pputNotifyPvt->cancelWait = pputNotifyPvt->userCallbackWait = 0;
    }
    if(!pputNotifyPvt->cancelWait && !pputNotifyPvt->userCallbackWait) {
        putNotifyCleanup(ppn);
        epicsMutexUnlock(pnotifyGlobal->lock); 
        return;
    }
    if(pputNotifyPvt->cancelWait) {
        pputNotifyPvt->cancelWait = 0;
        epicsEventSignal(pputNotifyPvt->cancelEvent);
        epicsMutexUnlock(pnotifyGlobal->lock);
        return;
    }
    assert(pputNotifyPvt->userCallbackWait);
    pputNotifyPvt->userCallbackWait = 0;
    epicsEventSignal(pputNotifyPvt->userCallbackEvent);
    epicsMutexUnlock(pnotifyGlobal->lock);
    return;
}

static void putNotifyCommon(putNotify *ppn,dbCommon *precord)
{
    long	status=0;
    dbFldDes	*pfldDes = ppn->paddr->pfldDes;
    putNotifyPvt *pputNotifyPvt = (putNotifyPvt *)ppn->pputNotifyPvt;

    if(precord->ppn && pputNotifyPvt->state!=putNotifyRestartCallbackRequested)
    { /*another putNotify owns the record */
        pputNotifyPvt->state = putNotifyWaitForRestart; 
        ellSafeAdd(&precord->ppnr->restartList,&ppn->restartNode);
        epicsMutexUnlock(pnotifyGlobal->lock);
        dbScanUnlock(precord);
        return;
    } else if(precord->ppn){
        assert(precord->ppn==ppn);
        assert(pputNotifyPvt->state==putNotifyRestartCallbackRequested);
    }
    if(precord->pact) {
        precord->ppn = ppn;
        ellSafeAdd(&pputNotifyPvt->waitList,&precord->ppnr->waitNode);
        pputNotifyPvt->state = putNotifyRestartInProgress; 
        epicsMutexUnlock(pnotifyGlobal->lock);
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
        ellSafeAdd(&pputNotifyPvt->waitList,&precord->ppnr->waitNode);
        pputNotifyPvt->state = putNotifyPutInProgress;
        epicsMutexUnlock(pnotifyGlobal->lock);
        dbProcess(precord);
        dbScanUnlock(precord);
        return;
    }
    if(pputNotifyPvt->state==putNotifyRestartCallbackRequested) {
        restartCheck(precord->ppnr);
    }
    pputNotifyPvt->state = putNotifyUserCallbackActive;
    assert(precord->ppn!=ppn);
    callUser(precord,ppn);
}

static void notifyCallback(CALLBACK *pcallback)
{
    putNotify	*ppn=NULL;
    dbCommon	*precord;
    putNotifyPvt *pputNotifyPvt;

    callbackGetUser(ppn,pcallback);
    pputNotifyPvt = (putNotifyPvt *)ppn->pputNotifyPvt;
    precord = ppn->paddr->precord;
    dbScanLock(precord);
    epicsMutexMustLock(pnotifyGlobal->lock);
    assert(precord->ppnr);
    assert(pputNotifyPvt->state==putNotifyRestartCallbackRequested
          || pputNotifyPvt->state==putNotifyUserCallbackRequested);
    assert(ellCount(&pputNotifyPvt->waitList)==0);
    if(pputNotifyPvt->cancelWait) {
        if(pputNotifyPvt->state==putNotifyRestartCallbackRequested) {
            restartCheck(precord->ppnr);
        }
        epicsEventSignal(pputNotifyPvt->cancelEvent);
        epicsMutexUnlock(pnotifyGlobal->lock);
        dbScanUnlock(precord);
        return;
    }
    if(pputNotifyPvt->state==putNotifyRestartCallbackRequested) {
        putNotifyCommon(ppn,precord);
        return;
    }
    /* All done. Clean up and call userCallback */
    pputNotifyPvt->state = putNotifyUserCallbackActive;
    assert(precord->ppn!=ppn);
    callUser(precord,ppn);
}

void epicsShareAPI dbPutNotifyInit(void)
{
    if(pnotifyGlobal) return;
    pnotifyGlobal = dbCalloc(1,sizeof(notifyGlobal));
    pnotifyGlobal->lock = epicsMutexMustCreate();
    ellInit(&pnotifyGlobal->freeList);
}

void epicsShareAPI dbPutNotify(putNotify *ppn)
{
    dbCommon	*precord = ppn->paddr->precord;
    short	dbfType = ppn->paddr->field_type;
    long	status=0;
    putNotifyPvt *pputNotifyPvt;

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
    epicsMutexMustLock(pnotifyGlobal->lock);
    pputNotifyPvt = (putNotifyPvt *)ppn->pputNotifyPvt;
    if(pputNotifyPvt && (pputNotifyPvt->magic!=MAGIC)) {
        printf("dbPutNotify:pputNotifyPvt was not initialized\n");
        pputNotifyPvt = 0;
    }
    if(pputNotifyPvt) {
        assert(pputNotifyPvt->state==putNotifyUserCallbackActive);
        pputNotifyPvt->userCallbackWait = 1;
        epicsMutexUnlock(pnotifyGlobal->lock);
        dbScanUnlock(precord);
        epicsEventWait(pputNotifyPvt->userCallbackEvent);
        dbScanLock(precord);
        epicsMutexMustLock(pnotifyGlobal->lock);
        putNotifyCleanup(ppn);
    }
    pputNotifyPvt = (putNotifyPvt *)ppn->pputNotifyPvt;
    assert(!pputNotifyPvt);
    putNotifyInit(ppn);
    pputNotifyPvt = (putNotifyPvt *)ppn->pputNotifyPvt;
    if(!precord->ppnr) {/* make sure record has a putNotifyRecord*/
        precord->ppnr = dbCalloc(1,sizeof(putNotifyRecord));
        precord->ppnr->precord = precord;
        ellInit(&precord->ppnr->restartList);
    }
    putNotifyCommon(ppn,precord);
}

void epicsShareAPI dbNotifyCancel(putNotify *ppn)
{
    dbCommon	*precord = ppn->paddr->precord;
    putNotifyState state;
    putNotifyPvt *pputNotifyPvt;

    assert(precord);
    dbScanLock(precord);
    epicsMutexMustLock(pnotifyGlobal->lock);
    pputNotifyPvt = (putNotifyPvt *)ppn->pputNotifyPvt;
    if(!pputNotifyPvt || pputNotifyPvt->state==putNotifyNotActive) {
        epicsMutexUnlock(pnotifyGlobal->lock);
        dbScanUnlock(precord);
        return;
    }
    state = pputNotifyPvt->state;
    /*If callback is scheduled or active wait for it to complete*/
    if(state==putNotifyUserCallbackRequested
    || state==putNotifyRestartCallbackRequested
    || state==putNotifyUserCallbackActive) {
        pputNotifyPvt->cancelWait = 1;
        epicsMutexUnlock(pnotifyGlobal->lock);
        dbScanUnlock(precord);
        epicsEventWait(pputNotifyPvt->cancelEvent);
        epicsMutexMustLock(pnotifyGlobal->lock);
        putNotifyCleanup(ppn);
        epicsMutexUnlock(pnotifyGlobal->lock);
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

            while((ppnrWait = (putNotifyRecord *)ellFirst(&pputNotifyPvt->waitList))){
                ellSafeDelete(&pputNotifyPvt->waitList,&ppnrWait->waitNode);
                restartCheck(ppnrWait);
            }
        }
        if(precord->ppn==ppn) restartCheck(precord->ppnr);
        break;
    default:
        printf("dbNotify: illegal state for notifyCallback\n");
    }
    pputNotifyPvt->state = putNotifyNotActive;
    putNotifyCleanup(ppn);
    epicsMutexUnlock(pnotifyGlobal->lock);
    dbScanUnlock(precord);
}

void epicsShareAPI dbNotifyCompletion(dbCommon *precord)
{
    putNotify	*ppn = precord->ppn;
    putNotifyPvt *pputNotifyPvt;

    epicsMutexMustLock(pnotifyGlobal->lock);
    assert(ppn);
    assert(precord->ppnr);
    pputNotifyPvt = (putNotifyPvt *)ppn->pputNotifyPvt;
    if(pputNotifyPvt->state!=putNotifyRestartInProgress
    && pputNotifyPvt->state!=putNotifyPutInProgress) {
        epicsMutexUnlock(pnotifyGlobal->lock);
        return;
    }
    ellSafeDelete(&pputNotifyPvt->waitList,&precord->ppnr->waitNode);
    if((ellCount(&pputNotifyPvt->waitList)!=0)) {
        restartCheck(precord->ppnr);
    } else if(pputNotifyPvt->state == putNotifyPutInProgress) {
        pputNotifyPvt->state = putNotifyUserCallbackRequested;
        restartCheck(precord->ppnr);
        callbackRequest(&pputNotifyPvt->callback);
    } else if(pputNotifyPvt->state == putNotifyRestartInProgress) {
        pputNotifyPvt->state = putNotifyRestartCallbackRequested;
        callbackRequest(&pputNotifyPvt->callback);
    } else {
        cantProceed("dbNotifyCompletion illegal state");
    }
    epicsMutexUnlock(pnotifyGlobal->lock);
}

void epicsShareAPI dbNotifyAdd(dbCommon *pfrom, dbCommon *pto)
{
    putNotify *ppn = pfrom->ppn;
    putNotifyPvt *pputNotifyPvt;

    if(pto->pact) return; /*if active it will not be processed*/
    epicsMutexMustLock(pnotifyGlobal->lock);
    if(!pto->ppnr) {/* make sure record has a putNotifyRecord*/
        pto->ppnr = dbCalloc(1,sizeof(putNotifyRecord));
        pto->ppnr->precord = pto;
        ellInit(&pto->ppnr->restartList);
    }
    assert(ppn);
    pputNotifyPvt = (putNotifyPvt *)ppn->pputNotifyPvt;
    if(!(pto->ppn) 
    && (pputNotifyPvt->state==putNotifyPutInProgress)
    && (pto!=ppn->paddr->precord)) {
        putNotifyPvt *pputNotifyPvt;
        pto->ppn = pfrom->ppn;
        pputNotifyPvt = (putNotifyPvt *)pfrom->ppn->pputNotifyPvt;
        ellSafeAdd(&pputNotifyPvt->waitList,&pto->ppnr->waitNode);
    }
    epicsMutexUnlock(pnotifyGlobal->lock);
}

typedef struct tpnInfo {
    epicsEventId callbackDone;
    putNotify    *ppn;
}tpnInfo;

static void dbtpnCallback(putNotify *ppn)
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
        lockStatus = epicsMutexTryLock(pnotifyGlobal->lock);
        if(lockStatus==epicsMutexLockOK) break;
        epicsThreadSleep(.05);
    }
    for(pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
    pdbRecordType;
    pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
        for (pdbRecordNode=(dbRecordNode *)ellFirst(&pdbRecordType->recList);
        pdbRecordNode;
        pdbRecordNode = (dbRecordNode *)ellNext(&pdbRecordNode->node)) {
            putNotifyPvt *pputNotifyPvt;
            precord = pdbRecordNode->precord;
            if (!precord->name[0] ||
                pdbRecordNode->flags & DBRN_FLAGS_ISALIAS)
                continue;
            if(!precord->ppn) continue;
            if(!precord->ppnr) continue;
            if(precord->ppn->paddr->precord != precord) continue;
            ppn = precord->ppn;
            pputNotifyPvt = (putNotifyPvt *)ppn->pputNotifyPvt;
            printf("%s state %d ppn %p\n  waitList\n",
                precord->name,pputNotifyPvt->state,(void*)ppn);
            ppnrWait = (putNotifyRecord *)ellFirst(&pputNotifyPvt->waitList);
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
    if(lockStatus==epicsMutexLockOK) epicsMutexUnlock(pnotifyGlobal->lock);
    return(0);
}
