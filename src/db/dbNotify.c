/* dbNotify.c */
/* base/src/db $Id$ */
/*
 *      Author: 	Marty Kraimer
 *      Date:           03-30-95
 * Extracted from dbLink.c
*/
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO

This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/

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
#include "callback.h"
#include "errlog.h"
#include "errMdef.h"
#include "dbBase.h"
#include "dbStaticLib.h"
#include "dbFldTypes.h"
#include "link.h"
#include "dbCommon.h"
#define epicsExportSharedSymbols
#include "dbAddr.h"
#include "dbScan.h"
#include "dbLock.h"
#include "callback.h"
#include "dbAccessDefs.h"
#include "recGbl.h"
#include "dbNotify.h"
#include "epicsTime.h"

#define STATIC static
#define STATIC
/*structure attached to ppnr field of each record*/
typedef struct putNotifyRecord {
    ellCheckNode  waitNode;
    ELLLIST  restartList; /*list of putNotifys to restart*/
    ELLLIST  waitList; /*list of records for current putNotify*/
    dbCommon *precord;
}putNotifyRecord;

/*putNotify.state values */
typedef enum {
    putNotifyNotActive,
    putNotifyWaitForRestart,
    putNotifyRestart,
    putNotifyPutInProgress,
    putNotifyUserCallbackRequested,
    putNotifyUserCallbackActive
}putNotifyState;

/* putNotify groups can span locksets if links are dynamically modified*/
/* Thus a global lock is taken while putNotify fields are accessed     */
STATIC epicsMutexId notifyLock = 0;

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

/*Local routines*/
STATIC void restartCheck(putNotifyRecord *ppnr);
STATIC void waitAdd(dbCommon *precord,putNotify *ppn);
STATIC void notifyCallback(CALLBACK *pcallback);

STATIC void restartCheck(putNotifyRecord *ppnr)
{
    dbCommon *precord = ppnr->precord;
    putNotify *pfirst;
    
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
    pfirst->state = putNotifyRestart;
    callbackRequest(&pfirst->callback);
}

STATIC void waitAdd(dbCommon *precord,putNotify *ppn)
{
    putNotifyRecord *ppnrOwner = ppn->paddr->precord->ppnr;
    putNotifyRecord *ppnrThis;

    if(!precord->ppnr) {/* make sure record has a putNotifyRecord*/
        precord->ppnr = dbCalloc(1,sizeof(putNotifyRecord));
        precord->ppnr->precord = precord;
        ellInit(&precord->ppnr->restartList);
        ellInit(&precord->ppnr->waitList);
    }
    ppnrThis = precord->ppnr;
    precord->ppn = ppn;
    ellSafeAdd(&ppnrOwner->waitList,&ppnrThis->waitNode);
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
    assert(precord->ppn==ppn);
    assert(ppn->state==putNotifyRestart
          || ppn->state==putNotifyUserCallbackRequested);
    if(ppn->state==putNotifyRestart) {
        if(++ppn->ntimesActive <= 2) {
            short dbrType = ppn->dbrType;
            void *pbuffer = ppn->pbuffer;
            long nRequest = ppn->nRequest;
            long status=0;
            dbFldDes *pfldDes=(dbFldDes *)(ppn->paddr->pfldDes);

            if(precord->pact) {
                /* a non putNotify processed the record */
                /* try again when record finishes processing*/
                waitAdd(precord,ppn);
                epicsMutexUnlock(notifyLock);
                dbScanUnlock(precord);
                return;
            }
            status=dbPut(ppn->paddr,dbrType,pbuffer,nRequest);
            ppn->status = status;
            /* Check to see if dbProcess should not be called */
            if(!status /*dont process if dbPut returned error */
            &&((ppn->paddr->pfield==(void *)&precord->proc)
               || (pfldDes->process_passive && precord->scan==0))) {
                waitAdd(precord,ppn);
                ppn->state = putNotifyPutInProgress;
                epicsMutexUnlock(notifyLock);
                dbProcess(precord);
                dbScanUnlock(precord);
                return;
            }
            /*All done. Just fall through and call userCallback*/
        } else {
            /* have already restarted three times. Give up*/
            ppn->status = S_db_Blocked;
        }
    }
    /* All done. Clean up and call userCallback */
    restartCheck(precord->ppnr);
    assert(precord->ppn!=ppn);
    ppn->state = putNotifyUserCallbackActive;
    epicsMutexUnlock(notifyLock);
    dbScanUnlock(precord);
epicsMutexMustLock(notifyLock);
    (*ppn->userCallback)(ppn);
    /*This ppn no longer owns record. No need to dbScanLock*/
#ifdef 0
    epicsMutexMustLock(notifyLock);
#endif
    ppn->state = putNotifyNotActive;
    epicsMutexUnlock(notifyLock);
}

long epicsShareAPI dbPutNotify(putNotify *ppn)
{
    dbCommon	*precord = ppn->paddr->precord;
    short	dbfType = ppn->paddr->field_type;
    long	status=0;
    short	dbrType = ppn->dbrType;
    void	*pbuffer = ppn->pbuffer;
    long	nRequest = ppn->nRequest;
    dbFldDes	*pfldDes=(dbFldDes *)(ppn->paddr->pfldDes);

    assert(precord);
    assert(precord->ppn!=ppn);
    /* Must handle DBF_XXXLINKs as special case.
     * Only dbPutField will change link fields. 
     * Also the record is not processed as a result
    */
    if(dbfType>=DBF_INLINK && dbfType<=DBF_FWDLINK) {
	status=dbPutField(ppn->paddr,ppn->dbrType,ppn->pbuffer,ppn->nRequest);
	ppn->status = status;
        (*ppn->userCallback)(ppn);
        return(S_db_Pending);
    }
    dbScanLock(precord);
    epicsMutexMustLock(notifyLock);
    ppn->status = 0;
    ppn->state = putNotifyNotActive;
    ppn->ntimesActive = 0;
    callbackSetCallback(notifyCallback,&ppn->callback);
    callbackSetUser(ppn,&ppn->callback);
    callbackSetPriority(priorityLow,&ppn->callback);
    if(!precord->ppnr) {/* make sure record has a putNotifyRecord*/
        precord->ppnr = dbCalloc(1,sizeof(putNotifyRecord));
        precord->ppnr->precord = precord;
        ellInit(&precord->ppnr->restartList);
        ellInit(&precord->ppnr->waitList);
    }
    if(precord->ppn) {
        assert(precord->ppn!=ppn);
        /*another putNotify owns the record */
        ppn->state = putNotifyWaitForRestart; 
        ellSafeAdd(&precord->ppnr->restartList,&ppn->restartNode);
        epicsMutexUnlock(notifyLock);
        dbScanUnlock(precord);
        return(S_db_Pending);
    }
    if(precord->pact) {
        waitAdd(precord,ppn);
        ppn->state = putNotifyRestart; 
        epicsMutexUnlock(notifyLock);
        dbScanUnlock(precord);
        return(S_db_Pending);
    }
    status=dbPut(ppn->paddr,dbrType,pbuffer,nRequest);
    ppn->status = status;
    /* Check to see if dbProcess should not be called */
    if(!status /*dont process if dbPut returned error */
    &&((ppn->paddr->pfield==(void *)&precord->proc) /*If PROC call dbProcess*/
       || (pfldDes->process_passive && precord->scan==0))) {
        waitAdd(precord,ppn);
        ppn->state = putNotifyPutInProgress;
        epicsMutexUnlock(notifyLock);
        dbProcess(precord);
        dbScanUnlock(precord);
        return(S_db_Pending);
    }
    epicsMutexUnlock(notifyLock);
    dbScanUnlock(precord);
    (*ppn->userCallback)(ppn);
    return(S_db_Pending);
}

void epicsShareAPI dbPutNotifyInit(void)
{
    notifyLock = epicsMutexMustCreate();
}

void epicsShareAPI dbNotifyCancel(putNotify *ppn)
{
    dbCommon	*precord = ppn->paddr->precord;
    putNotifyState state;

    assert(precord);
    dbScanLock(precord);
    epicsMutexMustLock(notifyLock);
    state = ppn->state;
    /*If callback is scheduled or active wait for it to complete*/
    while(state==putNotifyUserCallbackRequested
    || state==putNotifyUserCallbackActive
    || state==putNotifyRestart) {
        epicsMutexUnlock(notifyLock);
        dbScanUnlock(precord);
        epicsThreadSleep(.1);
        dbScanLock(precord);
        epicsMutexMustLock(notifyLock);
        state = ppn->state;
    }
    switch(state) {
    case putNotifyNotActive: break;
    case putNotifyWaitForRestart:
        assert(precord->ppn);
        assert(precord->ppn!=ppn);
        ellSafeDelete(&precord->ppnr->restartList,&ppn->restartNode);
        break;
    case putNotifyPutInProgress:
        { /*Take all records out of wait list */
            putNotifyRecord *ppnrWait;

            assert(precord->ppn==ppn);
            while((ppnrWait = (putNotifyRecord *)ellFirst(&precord->ppnr->waitList))){
                ellSafeDelete(&precord->ppnr->waitList,&ppnrWait->waitNode);
                if(ppnrWait->precord!=precord) restartCheck(ppnrWait);
            }
            restartCheck(precord->ppnr);
        }
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
    putNotify	*ppnOwner = precord->ppn;
    putNotifyRecord *ppnrOwner = ppnOwner->paddr->precord->ppnr;
    putNotifyRecord *ppnrThis = precord->ppnr;

    epicsMutexMustLock(notifyLock);
    assert(ppnOwner);
    assert(ppnrThis);
    ellSafeDelete(&ppnrOwner->waitList,&ppnrThis->waitNode);
    if(precord!=ppnrOwner->precord) restartCheck(ppnrThis);
    if((ellCount(&ppnrOwner->waitList)==0)) {
        switch(ppnOwner->state) {
        case putNotifyPutInProgress:
            ppnOwner->state = putNotifyUserCallbackRequested;
            break;
        case putNotifyRestart:
            break;
        default:
            printf("%s dbNotifyCompletion illegal state %d\n",
                precord->name,ppnOwner->state);
            return;
        }
        callbackRequest(&ppnOwner->callback);
    }
    epicsMutexUnlock(notifyLock);
}

void epicsShareAPI dbNotifyAdd(dbCommon *pfrom, dbCommon *pto)
{
    if(pto->pact) return; /*if active it will not be processed*/
    epicsMutexMustLock(notifyLock);
    /* if pto->ppn then either another putNotify owns record or */
    /* there is an infinite look of record processing. In either*/
    /* case dont add the record                                 */
    if(!pto->ppn) waitAdd(pto,pfrom->ppn);
    epicsMutexUnlock(notifyLock);
}

STATIC void dbtpnCallback(putNotify *ppn)
{
    long	status = ppn->status;

    if(status==S_db_Blocked)
	printf("dbtpnCallback: blocked record=%s\n",ppn->paddr->precord->name);
    else if(status==0)
	printf("dbtpnCallback: success record=%s\n",ppn->paddr->precord->name);
    else
	recGblRecordError(status,ppn->paddr->precord,"dbtpnCallback");
    free((void *)ppn->paddr);
    free(ppn);
}

long epicsShareAPI dbtpn(char	*pname,char *pvalue)
{
    long	status;
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
    status = dbPutNotify(ppn);
    if(status==S_db_Pending) {
	printf("dbtpn: Pending nwaiting=%d\n",
            ellCount(&ppn->paddr->precord->ppnr->waitList));
	return(0);
    }
    if(status==S_db_Blocked) {
	printf("dbtpn: blocked record=%s\n",pname);
    } else if(status) {
    	errMessage(status, "dbtpn");
    }
    return(0);
}

int epicsShareAPI dbNotifyDump(void)
{
    epicsMutexLockStatus lockStatus;
    dbRecordType *pdbRecordType;
    dbRecordNode *pdbRecordNode;
    dbCommon     *precord;
    putNotify    *ppnOwner;
    putNotifyRecord *ppnrOwner;
    putNotify    *ppnRestart;
    putNotifyRecord *ppnrWait;
   
    
    lockStatus = epicsMutexLockWithTimeout(notifyLock,2.0);
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
            ppnOwner = precord->ppn;
            ppnrOwner = precord->ppnr;
            printf("%s state %d\n  waitList\n",
                ppnrOwner->precord->name,ppnOwner->state);
            ppnrWait = (putNotifyRecord *)ellFirst(&ppnrOwner->waitList);
            while(ppnrWait) {
                printf("    %s pact %d\n",
                    ppnrWait->precord->name,ppnrWait->precord->pact);
                ppnrWait = (putNotifyRecord *)ellNext(&ppnrWait->waitNode.node);
            }
            printf("  restartList\n");
            ppnRestart = (putNotify *)ellFirst(&ppnrOwner->restartList);
            while(ppnrWait) {
                printf("    %s\n", ppnRestart->paddr->precord->name);
                ppnRestart = (putNotify *)ellNext(&ppnRestart->restartNode.node);
            }
        }
    }
    if(lockStatus==epicsMutexLockOK) epicsMutexUnlock(notifyLock);
    return(0);
}
