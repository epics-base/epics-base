/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* dbNotify.c */
/*
 *      Author: 	Marty Kraimer
 *                      Andrew Johnson <anj@aps.anl.gov>
 *
 * Extracted from dbLink.c
 */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "cantProceed.h"
#include "dbDefs.h"
#include "ellLib.h"
#include "epicsAssert.h"
#include "epicsEvent.h"
#include "epicsMutex.h"
#include "epicsString.h"
#include "epicsThread.h"
#include "epicsTime.h"
#include "errlog.h"
#include "errMdef.h"

#define epicsExportSharedSymbols
#include "callback.h"
#include "dbAccessDefs.h"
#include "dbBase.h"
#include "dbChannel.h"
#include "dbCommon.h"
#include "dbFldTypes.h"
#include "dbLock.h"
#include "dbNotify.h"
#include "dbScan.h"
#include "dbStaticLib.h"
#include "link.h"
#include "recGbl.h"

/*notify state values */
typedef enum {
    notifyNotActive,
    notifyWaitForRestart,
    notifyRestartCallbackRequested,
    notifyRestartInProgress,
    notifyProcessInProgress,
    notifyUserCallbackRequested,
    notifyUserCallbackActive
} notifyState;

/*structure attached to ppnr field of each record*/
typedef struct processNotifyRecord {
    ellCheckNode waitNode;
    ELLLIST  restartList; /*list of processNotifys to restart*/
    dbCommon *precord;
} processNotifyRecord;

#define MAGIC 0xfedc0123
typedef struct notifyPvt {
    ELLNODE      node;   /*For free list*/
    long         magic;
    short        state;
    CALLBACK     callback;
    ELLLIST      waitList; /*list of records for current processNotify*/
    short        cancelWait;
    short        userCallbackWait;
    epicsEventId cancelEvent;
    epicsEventId userCallbackEvent;
} notifyPvt;

/* processNotify groups can span locksets if links are dynamically modified*/
/* Thus a global lock is taken while processNotify fields are accessed     */
typedef struct notifyGlobal {
    epicsMutexId lock;
    ELLLIST      freeList;
} notifyGlobal;

static notifyGlobal *pnotifyGlobal = 0;

/*Local routines*/
static void notifyInit(processNotify *ppn);
static void notifyCleanup(processNotify *ppn);
static void restartCheck(processNotifyRecord *ppnr);
static void callDone(dbCommon *precord,processNotify *ppn);
static void processNotifyCommon(processNotify *ppn,dbCommon *precord);
static void notifyCallback(CALLBACK *pcallback);

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

static void notifyFree(void *raw)
{
    notifyPvt *pnotifyPvt = raw;
    assert(pnotifyPvt->magic==MAGIC);
    epicsEventDestroy(pnotifyPvt->cancelEvent);
    epicsEventDestroy(pnotifyPvt->userCallbackEvent);
    free(pnotifyPvt);
}

static void notifyInit(processNotify *ppn)
{
    notifyPvt *pnotifyPvt;

    pnotifyPvt = (notifyPvt *) ellFirst(&pnotifyGlobal->freeList);
    if (pnotifyPvt) {
        ellDelete(&pnotifyGlobal->freeList, &pnotifyPvt->node);
    } else {
        pnotifyPvt = dbCalloc(1,sizeof(notifyPvt));
        pnotifyPvt->cancelEvent = epicsEventCreate(epicsEventEmpty);
        pnotifyPvt->userCallbackEvent = epicsEventCreate(epicsEventEmpty);
        pnotifyPvt->magic = MAGIC;
        pnotifyPvt->state = notifyNotActive;
    }
    pnotifyPvt->state = notifyNotActive;
    callbackSetCallback(notifyCallback,&pnotifyPvt->callback);
    callbackSetUser(ppn,&pnotifyPvt->callback);
    callbackSetPriority(priorityLow,&pnotifyPvt->callback);
    ellInit(&pnotifyPvt->waitList);
    ppn->status = notifyOK;
    ppn->wasProcessed = 0;
    pnotifyPvt->state = notifyNotActive;
    pnotifyPvt->cancelWait = pnotifyPvt->userCallbackWait = 0;
    ppn->pnotifyPvt = pnotifyPvt;
}

static void notifyCleanup(processNotify *ppn)
{
    notifyPvt *pnotifyPvt = (notifyPvt *) ppn->pnotifyPvt;

    pnotifyPvt->state = notifyNotActive;
    ellAdd(&pnotifyGlobal->freeList, &pnotifyPvt->node);
    ppn->pnotifyPvt = 0;
}

static void restartCheck(processNotifyRecord *ppnr)
{
    dbCommon *precord = ppnr->precord;
    processNotify *pfirst;
    notifyPvt *pnotifyPvt;
    
    assert(precord->ppn);
    pfirst = (processNotify *) ellFirst(&ppnr->restartList);
    if (!pfirst) {
        precord->ppn = 0;
        return;
    }
    pnotifyPvt = (notifyPvt *) pfirst->pnotifyPvt;
    assert(pnotifyPvt->state == notifyWaitForRestart);
    /* remove pfirst from restartList */
    ellSafeDelete(&ppnr->restartList, &pfirst->restartNode);
    /*make pfirst owner of the record*/
    precord->ppn = pfirst;
    /* request callback for pfirst */
    pnotifyPvt->state = notifyRestartCallbackRequested;
    callbackRequest(&pnotifyPvt->callback);
}

static void callDone(dbCommon *precord, processNotify *ppn)
{
    notifyPvt *pnotifyPvt = (notifyPvt *) ppn->pnotifyPvt;

    epicsMutexUnlock(pnotifyGlobal->lock);
    if (ppn->requestType == processGetRequest ||
        ppn->requestType == putProcessGetRequest) {
        ppn->getCallback(ppn, getFieldType);
    }
    dbScanUnlock(precord);
    ppn->doneCallback(ppn);
    epicsMutexMustLock(pnotifyGlobal->lock);
    if (pnotifyPvt->cancelWait && pnotifyPvt->userCallbackWait) {
        errlogPrintf("%s processNotify: both cancelWait and userCallbackWait true."
               "This is illegal\n", precord->name);
        pnotifyPvt->cancelWait = pnotifyPvt->userCallbackWait = 0;
    }
    if (!pnotifyPvt->cancelWait && !pnotifyPvt->userCallbackWait) {
        notifyCleanup(ppn);
        epicsMutexUnlock(pnotifyGlobal->lock); 
        return;
    }
    if (pnotifyPvt->cancelWait) {
        pnotifyPvt->cancelWait = 0;
        epicsEventSignal(pnotifyPvt->cancelEvent);
        epicsMutexUnlock(pnotifyGlobal->lock);
        return;
    }
    assert(pnotifyPvt->userCallbackWait);
    pnotifyPvt->userCallbackWait = 0;
    epicsEventSignal(pnotifyPvt->userCallbackEvent);
    epicsMutexUnlock(pnotifyGlobal->lock);
    return;
}

static void processNotifyCommon(processNotify *ppn,dbCommon *precord)
{
    notifyPvt *pnotifyPvt = (notifyPvt *) ppn->pnotifyPvt;
    int didPut = 0;
    int doProcess = 0;

    if (precord->ppn &&
        pnotifyPvt->state != notifyRestartCallbackRequested) {
        /* Another processNotify owns the record */
        pnotifyPvt->state = notifyWaitForRestart; 
        ellSafeAdd(&precord->ppnr->restartList, &ppn->restartNode);
        epicsMutexUnlock(pnotifyGlobal->lock);
        dbScanUnlock(precord);
        return;
    } else if (precord->ppn) {
        assert(precord->ppn == ppn);
        assert(pnotifyPvt->state == notifyRestartCallbackRequested);
    }
    if (precord->pact) {
        precord->ppn = ppn;
        ellSafeAdd(&pnotifyPvt->waitList, &precord->ppnr->waitNode);
        pnotifyPvt->state = notifyRestartInProgress; 
        epicsMutexUnlock(pnotifyGlobal->lock);
        dbScanUnlock(precord);
        return;
    }
    if (ppn->requestType == putProcessRequest ||
        ppn->requestType == putProcessGetRequest) {
        /* Check if puts disabled */
        if (precord->disp && (dbChannelField(ppn->chan) != (void *) &precord->disp)) {
            ppn->putCallback(ppn, putDisabledType);
        } else {
            didPut = ppn->putCallback(ppn, putType);
        }
    }
    /* Check if dbProcess should be called */
    if (didPut &&
        ((dbChannelField(ppn->chan) == (void *) &precord->proc) ||
        (dbChannelFldDes(ppn->chan)->process_passive && precord->scan == 0)))
       doProcess = 1;
    else
        if (ppn->requestType == processGetRequest &&
            precord->scan == 0)
            doProcess = 1;

    if (doProcess) {
        ppn->wasProcessed = 1;
        precord->ppn = ppn;
        ellSafeAdd(&pnotifyPvt->waitList, &precord->ppnr->waitNode);
        pnotifyPvt->state = notifyProcessInProgress;
        epicsMutexUnlock(pnotifyGlobal->lock);
        dbProcess(precord);
        dbScanUnlock(precord);
        return;
    }
    if (pnotifyPvt->state == notifyRestartCallbackRequested) {
        restartCheck(precord->ppnr);
    }
    pnotifyPvt->state = notifyUserCallbackActive;
    assert(precord->ppn!=ppn);
    callDone(precord, ppn);
}

static void notifyCallback(CALLBACK *pcallback)
{
    processNotify *ppn = NULL;
    dbCommon  *precord;
    notifyPvt *pnotifyPvt;

    callbackGetUser(ppn,pcallback);
    pnotifyPvt = (notifyPvt *) ppn->pnotifyPvt;
    precord = dbChannelRecord(ppn->chan);
    dbScanLock(precord);
    epicsMutexMustLock(pnotifyGlobal->lock);
    assert(precord->ppnr);
    assert(pnotifyPvt->state == notifyRestartCallbackRequested ||
           pnotifyPvt->state == notifyUserCallbackRequested);
    assert(ellCount(&pnotifyPvt->waitList) == 0);
    if (pnotifyPvt->cancelWait) {
        if (pnotifyPvt->state == notifyRestartCallbackRequested) {
            restartCheck(precord->ppnr);
        }
        epicsEventSignal(pnotifyPvt->cancelEvent);
        epicsMutexUnlock(pnotifyGlobal->lock);
        dbScanUnlock(precord);
        return;
    }
    if(pnotifyPvt->state == notifyRestartCallbackRequested) {
        processNotifyCommon(ppn, precord);
        return;
    }
    /* All done. Clean up and call userCallback */
    pnotifyPvt->state = notifyUserCallbackActive;
    assert(precord->ppn!=ppn);
    callDone(precord, ppn);
}

void dbProcessNotifyExit(void)
{
    ellFree2(&pnotifyGlobal->freeList, &notifyFree);
    epicsMutexDestroy(pnotifyGlobal->lock);
    free(pnotifyGlobal);
    pnotifyGlobal = NULL;
}

void dbProcessNotifyInit(void)
{
    if (pnotifyGlobal)
        return;
    pnotifyGlobal = dbCalloc(1,sizeof(notifyGlobal));
    pnotifyGlobal->lock = epicsMutexMustCreate();
    ellInit(&pnotifyGlobal->freeList);
}

void dbProcessNotify(processNotify *ppn)
{
    struct dbChannel *chan = ppn->chan;
    dbCommon *precord = dbChannelRecord(chan);
    short dbfType = dbChannelFieldType(chan);
    notifyPvt *pnotifyPvt;

    /* Must handle DBF_XXXLINKs as special case.
     * Only dbPutField will change link fields.
     * Also the record is not processed as a result
    */
    ppn->status = notifyOK;
    ppn->wasProcessed = 0;
    if (dbfType>=DBF_INLINK && dbfType<=DBF_FWDLINK) {
        if (ppn->requestType == putProcessRequest ||
            ppn->requestType == putProcessGetRequest) {
            /* Check if puts disabled */
            if (precord->disp && (dbChannelField(ppn->chan) != (void *) &precord->disp)) {
                ppn->putCallback(ppn, putDisabledType);
            } else {
                ppn->putCallback(ppn, putFieldType);
            }
        }
        if (ppn->requestType == processGetRequest ||
            ppn->requestType == putProcessGetRequest) {
                ppn->getCallback(ppn, getFieldType);
            
        }
        ppn->doneCallback(ppn);
        return;
    }
    dbScanLock(precord);
    epicsMutexMustLock(pnotifyGlobal->lock);
    pnotifyPvt = (notifyPvt *) ppn->pnotifyPvt;
    if (pnotifyPvt && (pnotifyPvt->magic != MAGIC)) {
        printf("dbPutNotify:pnotifyPvt was not initialized\n");
        pnotifyPvt = 0;
    }
    if (pnotifyPvt) {
        assert(pnotifyPvt->state == notifyUserCallbackActive);
        pnotifyPvt->userCallbackWait = 1;
        epicsMutexUnlock(pnotifyGlobal->lock);
        dbScanUnlock(precord);
        epicsEventWait(pnotifyPvt->userCallbackEvent);
        dbScanLock(precord);
        epicsMutexMustLock(pnotifyGlobal->lock);
        notifyCleanup(ppn);
    }
    pnotifyPvt = (notifyPvt *) ppn->pnotifyPvt;
    assert(!pnotifyPvt);
    notifyInit(ppn);
    pnotifyPvt = (notifyPvt *) ppn->pnotifyPvt;
    if (!precord->ppnr) {
        /* make sure record has a processNotifyRecord*/
        precord->ppnr = dbCalloc(1, sizeof(processNotifyRecord));
        precord->ppnr->precord = precord;
        ellInit(&precord->ppnr->restartList);
    }
    processNotifyCommon(ppn, precord);
}

void dbNotifyCancel(processNotify *ppn)
{
    dbCommon *precord = dbChannelRecord(ppn->chan);
    notifyState state;
    notifyPvt *pnotifyPvt;

    dbScanLock(precord);
    epicsMutexMustLock(pnotifyGlobal->lock);
    ppn->status = notifyCanceled;
    pnotifyPvt = (notifyPvt *) ppn->pnotifyPvt;
    if (!pnotifyPvt || pnotifyPvt->state == notifyNotActive) {
        epicsMutexUnlock(pnotifyGlobal->lock);
        dbScanUnlock(precord);
        return;
    }

    state = pnotifyPvt->state;
    switch (state) {
    case notifyUserCallbackRequested:
    case notifyRestartCallbackRequested:
    case notifyUserCallbackActive:
        /* Callback is scheduled or active, wait for it to complete */
        pnotifyPvt->cancelWait = 1;
        epicsMutexUnlock(pnotifyGlobal->lock);
        dbScanUnlock(precord);
        epicsEventWait(pnotifyPvt->cancelEvent);
        epicsMutexMustLock(pnotifyGlobal->lock);
        notifyCleanup(ppn);
        epicsMutexUnlock(pnotifyGlobal->lock);
        return;
    case notifyNotActive:
    	break;
    case notifyWaitForRestart:
        assert(precord->ppn);
        assert(precord->ppn!=ppn);
        ellSafeDelete(&precord->ppnr->restartList,&ppn->restartNode);
        break;
    case notifyRestartInProgress:
    case notifyProcessInProgress:
        { /*Take all records out of wait list */
            processNotifyRecord *ppnrWait;

            while ((ppnrWait = (processNotifyRecord *)
	                       ellFirst(&pnotifyPvt->waitList))) {
                ellSafeDelete(&pnotifyPvt->waitList, &ppnrWait->waitNode);
                restartCheck(ppnrWait);
            }
        }
        if (precord->ppn == ppn)
            restartCheck(precord->ppnr);
        break;
    default:
        printf("dbNotify: illegal state for notifyCallback\n");
    }
    pnotifyPvt->state = notifyNotActive;
    notifyCleanup(ppn);
    epicsMutexUnlock(pnotifyGlobal->lock);
    dbScanUnlock(precord);
}

void dbNotifyCompletion(dbCommon *precord)
{
    processNotify *ppn = precord->ppn;
    notifyPvt *pnotifyPvt;

    epicsMutexMustLock(pnotifyGlobal->lock);
    assert(ppn);
    assert(precord->ppnr);
    pnotifyPvt = (notifyPvt *) ppn->pnotifyPvt;
    if (pnotifyPvt->state != notifyRestartInProgress &&
        pnotifyPvt->state != notifyProcessInProgress) {
        epicsMutexUnlock(pnotifyGlobal->lock);
        return;
    }
    ellSafeDelete(&pnotifyPvt->waitList, &precord->ppnr->waitNode);
    if ((ellCount(&pnotifyPvt->waitList) != 0)) {
        restartCheck(precord->ppnr);
    }
    else if (pnotifyPvt->state == notifyProcessInProgress) {
        pnotifyPvt->state = notifyUserCallbackRequested;
        restartCheck(precord->ppnr);
        callbackRequest(&pnotifyPvt->callback);
    }
    else if(pnotifyPvt->state == notifyRestartInProgress) {
        pnotifyPvt->state = notifyRestartCallbackRequested;
        callbackRequest(&pnotifyPvt->callback);
    } else {
        cantProceed("dbNotifyCompletion illegal state");
    }
    epicsMutexUnlock(pnotifyGlobal->lock);
}

void dbNotifyAdd(dbCommon *pfrom, dbCommon *pto)
{
    processNotify *ppn = pfrom->ppn;
    notifyPvt *pnotifyPvt;

    if (pto->pact)
        return; /*if active it will not be processed*/
    epicsMutexMustLock(pnotifyGlobal->lock);
    if (!pto->ppnr) {/* make sure record has a processNotifyRecord*/
        pto->ppnr = dbCalloc(1, sizeof(processNotifyRecord));
        pto->ppnr->precord = pto;
        ellInit(&pto->ppnr->restartList);
    }
    assert(ppn);
    pnotifyPvt = (notifyPvt *) ppn->pnotifyPvt;
    if (!pto->ppn &&
        (pnotifyPvt->state == notifyProcessInProgress) &&
        (pto != dbChannelRecord(ppn->chan))) {
        notifyPvt *pnotifyPvt;
        pto->ppn = pfrom->ppn;
        pnotifyPvt = (notifyPvt *) pfrom->ppn->pnotifyPvt;
        ellSafeAdd(&pnotifyPvt->waitList, &pto->ppnr->waitNode);
    }
    epicsMutexUnlock(pnotifyGlobal->lock);
}

typedef struct tpnInfo {
    epicsEventId callbackDone;
    processNotify *ppn;
    char buffer[80];
} tpnInfo;

static int putCallback(processNotify *ppn, notifyPutType type)
{
    tpnInfo *ptpnInfo = (tpnInfo *) ppn->usrPvt;
    int status = 0;

    if (ppn->status == notifyCanceled)
        return 0;
    ppn->status = notifyOK;
    switch (type) {
    case putDisabledType:
        ppn->status = notifyError;
        return 0;
    case putFieldType:
        status = dbChannelPutField(ppn->chan, DBR_STRING, ptpnInfo->buffer, 1);
        break;
    case putType:
        status = dbChannelPut(ppn->chan, DBR_STRING, ptpnInfo->buffer, 1);
        break;
    }
    if (status)
        ppn->status = notifyError;
    return 1;
}

static void getCallback(processNotify *ppn,notifyGetType type)
{
    tpnInfo *ptpnInfo = (tpnInfo *)ppn->usrPvt;
    int status = 0;
    long no_elements = 1;
    long options = 0;

    if(ppn->status==notifyCanceled) {
        printf("dbtpn:getCallback notifyCanceled\n");
        return;
    }
    switch(type) {
    case getFieldType:
        status = dbChannelGetField(ppn->chan, DBR_STRING, ptpnInfo->buffer,
            &options, &no_elements, 0);
        break;
    case getType:
        status = dbChannelGet(ppn->chan, DBR_STRING, ptpnInfo->buffer,
            &options, &no_elements, 0);
        break;
    }
    if (status) {
        ppn->status = notifyError;
        printf("dbtpn:getCallback error\n");
    } else {
        printf("dbtpn:getCallback value %s\n", ptpnInfo->buffer);
    }
}

static void doneCallback(processNotify *ppn)
{
    notifyStatus status = ppn->status;
    tpnInfo *ptpnInfo = (tpnInfo *) ppn->usrPvt;
    const char *pname = dbChannelRecord(ppn->chan)->name;

    if (status == 0)
        printf("dbtpnCallback: success record=%s\n", pname);
    else
        printf("%s dbtpnCallback processNotify.status %d\n",
	    pname, (int) status);
    epicsEventSignal(ptpnInfo->callbackDone);
}

static void tpnThread(void *pvt)
{
    tpnInfo *ptpnInfo = (tpnInfo *) pvt;
    processNotify *ppn = (processNotify *) ptpnInfo->ppn;

    dbProcessNotify(ppn);
    epicsEventWait(ptpnInfo->callbackDone);
    dbNotifyCancel(ppn);
    epicsEventDestroy(ptpnInfo->callbackDone);
    dbChannelDelete(ppn->chan);
    free(ppn);
    free(ptpnInfo);
}

long dbtpn(char *pname, char *pvalue)
{
    struct dbChannel *chan;
    tpnInfo *ptpnInfo;
    processNotify *ppn=NULL;

    chan = dbChannelCreate(pname);
    if (!chan) {
        printf("dbtpn: No such channel");
        return -1;
    }
    
    ppn = dbCalloc(1, sizeof(processNotify));
    ppn->requestType = pvalue ? putProcessRequest : processGetRequest;
    ppn->chan = chan;
    ppn->putCallback = putCallback;
    ppn->getCallback = getCallback;
    ppn->doneCallback = doneCallback;

    ptpnInfo = dbCalloc(1, sizeof(tpnInfo));
    ptpnInfo->ppn = ppn;
    ptpnInfo->callbackDone = epicsEventCreate(epicsEventEmpty);
    strncpy(ptpnInfo->buffer, pvalue, 80);
    ptpnInfo->buffer[79] = 0;

    ppn->usrPvt = ptpnInfo;
    epicsThreadCreate("dbtpn", epicsThreadPriorityHigh,
        epicsThreadGetStackSize(epicsThreadStackMedium), tpnThread, ptpnInfo);
    return 0;
}

int dbNotifyDump(void)
{
    epicsMutexLockStatus lockStatus;
    dbRecordType *pdbRecordType;
    processNotify *ppnRestart;
    processNotifyRecord *ppnr;
    int itry;

    for (itry = 0; itry < 100; itry++) {
        lockStatus = epicsMutexTryLock(pnotifyGlobal->lock);
        if (lockStatus == epicsMutexLockOK)
            break;
        epicsThreadSleep(.05);
    }

    for (pdbRecordType = (dbRecordType *) ellFirst(&pdbbase->recordTypeList);
         pdbRecordType;
         pdbRecordType = (dbRecordType *) ellNext(&pdbRecordType->node)) {
        dbRecordNode *pdbRecordNode;

        for (pdbRecordNode = (dbRecordNode *) ellFirst(&pdbRecordType->recList);
             pdbRecordNode;
             pdbRecordNode = (dbRecordNode *) ellNext(&pdbRecordNode->node)) {
            dbCommon *precord = pdbRecordNode->precord;
            processNotify *ppn;
            notifyPvt *pnotifyPvt;

            if (!precord->name[0] || pdbRecordNode->flags & DBRN_FLAGS_ISALIAS)
                continue;
            ppn = precord->ppn;
            if (!ppn || !precord->ppnr)
                continue;
            if (dbChannelRecord(precord->ppn->chan) != precord)
                continue;

            pnotifyPvt = (notifyPvt *) ppn->pnotifyPvt;
            printf("%s state %d ppn %p\n  waitList\n",
                precord->name, pnotifyPvt->state, (void*) ppn);
            ppnr = (processNotifyRecord *) ellFirst(&pnotifyPvt->waitList);
            while (ppnr) {
                printf("    %s pact %d\n",
                    ppnr->precord->name, ppnr->precord->pact);
                ppnr = (processNotifyRecord *) ellNext(&ppnr->waitNode.node);
            }
            ppnr = precord->ppnr;
            if (ppnr)  {
                ppnRestart = (processNotify *)ellFirst(
                    &precord->ppnr->restartList);
                if (ppnRestart)
		    printf("%s restartList\n", precord->name);
                while (ppnRestart) {
                    printf("    %s\n", dbChannelRecord(ppnRestart->chan)->name);
                    ppnRestart = (processNotify *) ellNext(
                        &ppnRestart->restartNode.node);
                }
            }
        }
    }
    if (lockStatus == epicsMutexLockOK)
        epicsMutexUnlock(pnotifyGlobal->lock);
    return 0;
}

