/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*asTrapWrite.c */
/* Author:  Marty Kraimer Date:    07NOV2000 */

/* Matthias Clausen and Vladis Korobov at DESY
 * implemented the first logging of Channel Access Puts
 * This implementation uses many ideas from their implementation
*/

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "ellLib.h"
#include "freeList.h"
#include "epicsStdioRedirect.h"
#include "cantProceed.h"
#include "epicsMutex.h"
#include "ellLib.h"
#define epicsExportSharedSymbols
#include "asLib.h"
#include "asTrapWrite.h"

typedef struct listenerPvt {
    ELLNODE node;
    struct listener *plistener;
    void *userPvt;
}listenerPvt;

typedef struct listener{
    ELLNODE node;
    asTrapWriteListener func;
}listener;

typedef struct writeMessage {
    ELLNODE node;
    asTrapWriteMessage message;
    ELLLIST listenerPvtList;
}writeMessage;
    

typedef struct asTrapWritePvt
{
    ELLLIST listenerList;
    ELLLIST writeMessageList;
    void *freeListWriteMessage;
    void *freeListListenerPvt;
    epicsMutexId lock;
}asTrapWritePvt;

static asTrapWritePvt *pasTrapWritePvt = 0;

static void asTrapWriteInit(void)
{
    pasTrapWritePvt = callocMustSucceed(1,sizeof(asTrapWritePvt),"asTrapWriteInit");
    ellInit(&pasTrapWritePvt->listenerList);
    ellInit(&pasTrapWritePvt->writeMessageList);
    freeListInitPvt(
        &pasTrapWritePvt->freeListWriteMessage,sizeof(writeMessage),20);
    freeListInitPvt(
        &pasTrapWritePvt->freeListListenerPvt,sizeof(listenerPvt),20);
    pasTrapWritePvt->lock = epicsMutexMustCreate();
}

asTrapWriteId epicsShareAPI asTrapWriteRegisterListener(
    asTrapWriteListener func)
{
    listener *plistener;
    if(pasTrapWritePvt==0) asTrapWriteInit();
    plistener = callocMustSucceed(1,sizeof(listener),"asTrapWriteRegisterListener");
    plistener->func = func; 
    epicsMutexMustLock(pasTrapWritePvt->lock);
    ellAdd(&pasTrapWritePvt->listenerList,&plistener->node);
    epicsMutexUnlock(pasTrapWritePvt->lock);
    return((asTrapWriteId)plistener);
}

void epicsShareAPI asTrapWriteUnregisterListener(asTrapWriteId id)
{
    listener *plistener = (listener *)id;
    writeMessage *pwriteMessage;

    if(pasTrapWritePvt==0) return;
    epicsMutexMustLock(pasTrapWritePvt->lock);
    pwriteMessage = (writeMessage *)ellFirst(&pasTrapWritePvt->writeMessageList);
    while(pwriteMessage) {
        listenerPvt *plistenerPvt
             = (listenerPvt *)ellFirst(&pwriteMessage->listenerPvtList);
        while(plistenerPvt) {
            listenerPvt *pnext
                 = (listenerPvt *)ellNext(&plistenerPvt->node);
            if(plistenerPvt->plistener == plistener) {
                ellDelete(&pwriteMessage->listenerPvtList,&plistenerPvt->node);
                freeListFree(pasTrapWritePvt->freeListListenerPvt,(void *)plistenerPvt);
            }
            plistenerPvt = pnext;
        }
        pwriteMessage = (writeMessage *)ellNext(&pwriteMessage->node);
    }
    ellDelete(&pasTrapWritePvt->listenerList,&plistener->node);
    free((void *)plistener);
    epicsMutexUnlock(pasTrapWritePvt->lock);
}

void * epicsShareAPI asTrapWriteBeforeWrite(
    const char *userid,const char *hostid,void *addr)
{
    writeMessage *pwriteMessage;
    listener *plistener;
    listenerPvt *plistenerPvt;

    if(pasTrapWritePvt==0) return(0);
    if(ellCount(&pasTrapWritePvt->listenerList)<=0) return 0;
    pwriteMessage = (writeMessage *)freeListCalloc(
        pasTrapWritePvt->freeListWriteMessage);
    pwriteMessage->message.userid = userid;
    pwriteMessage->message.hostid = hostid;
    pwriteMessage->message.serverSpecific = addr;
    ellInit(&pwriteMessage->listenerPvtList);
    epicsMutexMustLock(pasTrapWritePvt->lock);
    ellAdd(&pasTrapWritePvt->writeMessageList,&pwriteMessage->node);
    plistener = (listener *)ellFirst(&pasTrapWritePvt->listenerList);
    while(plistener) {
        plistenerPvt = (listenerPvt *)freeListCalloc(
            pasTrapWritePvt->freeListListenerPvt);
        plistenerPvt->plistener = plistener;
        pwriteMessage->message.userPvt = 0;
        (*plistener->func)(&pwriteMessage->message,0);
        plistenerPvt->userPvt = pwriteMessage->message.userPvt;
        ellAdd(&pwriteMessage->listenerPvtList,&plistenerPvt->node);
        plistener = (listener *)ellNext(&plistener->node);
    }
    epicsMutexUnlock(pasTrapWritePvt->lock);
    return((void *)pwriteMessage);
}

void epicsShareAPI asTrapWriteAfterWrite(void *pvt)
{
    writeMessage *pwriteMessage = (writeMessage *)pvt;
    listenerPvt *plistenerPvt;

    if(pwriteMessage==0 || pasTrapWritePvt==0) return;
    epicsMutexMustLock(pasTrapWritePvt->lock);
    plistenerPvt = (listenerPvt *)ellFirst(&pwriteMessage->listenerPvtList);
    while(plistenerPvt) {
        listenerPvt *pnext = (listenerPvt *)ellNext(&plistenerPvt->node);
        listener *plistener;
        plistener = plistenerPvt->plistener;
        pwriteMessage->message.userPvt = plistenerPvt->userPvt;
        (*plistener->func)(&pwriteMessage->message,1);
        ellDelete(&pwriteMessage->listenerPvtList,&plistenerPvt->node);
        freeListFree(pasTrapWritePvt->freeListListenerPvt,(void *)plistenerPvt);
        plistenerPvt = pnext;
    }
    ellDelete(&pasTrapWritePvt->writeMessageList,&pwriteMessage->node);
    freeListFree(pasTrapWritePvt->freeListWriteMessage,(void *)pwriteMessage);
    epicsMutexUnlock(pasTrapWritePvt->lock);
}
