/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *      Author:          Marty Kraimer and Jeff Hill
 *      Date:            07JAN1998
 *	NOTE: Original version is adaptation of old version of errPrintfVX.c
*/

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#define epicsExportSharedSymbols
#define ERRLOG_INIT
#include "dbDefs.h"
#include "epicsThread.h"
#include "epicsMutex.h"
#include "epicsEvent.h"
#include "epicsInterrupt.h"
#include "epicsAssert.h"
#include "errMdef.h"
#include "error.h"
#include "ellLib.h"
#include "errlog.h"
#include "logClient.h"
#include "epicsStdio.h"


#ifndef LOCAL
#define LOCAL static
#endif /* LOCAL */

#define BUFFER_SIZE 1280
#define MAX_MESSAGE_SIZE 256
#define MAX_ALIGNMENT 8

/*Declare storage for errVerbose */
epicsShareDef int errVerbose=0;

LOCAL void errlogThread(void);

LOCAL char *msgbufGetFree(int noConsoleMessage);
LOCAL void msgbufSetSize(int size); /* Send 'size' chars plus trailing '\0' */
LOCAL char * msgbufGetSend(int *noConsoleMessage);
LOCAL void msgbufFreeSend(void);

LOCAL void *pvtCalloc(size_t count,size_t size);

typedef struct listenerNode{
    ELLNODE	node;
    errlogListener listener;
    void *pPrivate;
}listenerNode;

/*each message consists of a msgNode immediately followed by the message */
typedef struct msgNode {
    ELLNODE	node;
    char	*message;
    int		length;
    int		noConsoleMessage;
} msgNode;

LOCAL struct {
    epicsEventId waitForWork; /*errlogThread waits for this*/
    epicsMutexId msgQueueLock;
    epicsMutexId listenerLock;
    epicsEventId waitForFlush; /*errlogFlush waits for this*/
    epicsEventId flush; /*errlogFlush sets errlogThread does a Try*/
    epicsMutexId flushLock;
    ELLLIST      listenerList;
    ELLLIST      msgQueue;
    msgNode      *pnextSend;
    int	         errlogInitFailed;
    int	         buffersize;
    int	         sevToLog;
    int	         toConsole;
    int	         missedMessages;
    void         *pbuffer;
}pvtData;


/*
 * vsnprintf with truncation message
 */
LOCAL int tvsnPrint(char *str, size_t size, const char *format, va_list ap)
{
    int nchar;
    static const char tmsg[] = "<<TRUNCATED>>\n";

    nchar = epicsVsnprintf(str, size, format?format:"", ap);
    if(nchar >= size) {
        if (size > sizeof tmsg)
            strcpy(str+size-sizeof tmsg, tmsg);
        nchar = size - 1;
    }
    return nchar;
}

epicsShareFunc int errlogPrintf( const char *pFormat, ...)
{
    va_list	pvar;
    int		nchar;

    if(epicsInterruptIsInterruptContext()) {
	epicsInterruptContextMessage
            ("errlogPrintf called from interrupt level\n");
	return 0;
    }
    errlogInit(0);
    va_start(pvar, pFormat);
    nchar = errlogVprintf(pFormat,pvar);
    va_end (pvar);
    return(nchar);
}

epicsShareFunc int errlogVprintf(
    const char *pFormat,va_list pvar)
{
    int nchar;
    char *pbuffer;
    int isOkToBlock = epicsThreadIsOkToBlock(epicsThreadGetIdSelf());

    if(epicsInterruptIsInterruptContext()) {
	epicsInterruptContextMessage
            ("errlogVprintf called from interrupt level\n");
	return 0;
    }
    errlogInit(0);
    pbuffer = msgbufGetFree(!isOkToBlock);
    if(!pbuffer) return(0);
    nchar = tvsnPrint(pbuffer,MAX_MESSAGE_SIZE,pFormat?pFormat:"",pvar);
    msgbufSetSize(nchar);
    if(isOkToBlock) printf("%s",pbuffer);
    return nchar;
}

epicsShareFunc int epicsShareAPI errlogMessage(const char *message)
{
    errlogPrintf("%s", message);
    return 0;
}

epicsShareFunc int errlogPrintfNoConsole( const char *pFormat, ...)
{
    va_list	pvar;
    int		nchar;

    if(epicsInterruptIsInterruptContext()) {
	epicsInterruptContextMessage
            ("errlogPrintf called from interrupt level\n");
	return 0;
    }
    errlogInit(0);
    va_start(pvar, pFormat);
    nchar = errlogVprintfNoConsole(pFormat,pvar);
    va_end (pvar);
    return(nchar);
}

epicsShareFunc int errlogVprintfNoConsole(
    const char *pFormat,va_list pvar)
{
    int nchar;
    char *pbuffer;

    if(epicsInterruptIsInterruptContext()) {
	epicsInterruptContextMessage
            ("errlogVprintf called from interrupt level\n");
	return 0;
    }
    errlogInit(0);
    pbuffer = msgbufGetFree(1);
    if(!pbuffer) return(0);
    nchar = tvsnPrint(pbuffer,MAX_MESSAGE_SIZE,pFormat?pFormat:"",pvar);
    msgbufSetSize(nchar);
    return nchar;
}

epicsShareFunc int errlogSevPrintf(
    const errlogSevEnum severity,const char *pFormat, ...)
{
    va_list	pvar;
    int		nchar;

    if(epicsInterruptIsInterruptContext()) {
	epicsInterruptContextMessage
            ("errlogSevPrintf called from interrupt level\n");
	return 0;
    }
    errlogInit(0);
    if(pvtData.sevToLog>severity) return(0);
    va_start(pvar, pFormat);
    nchar = errlogSevVprintf(severity,pFormat,pvar);
    va_end (pvar);
    return(nchar);
}

epicsShareFunc int errlogSevVprintf(
    const errlogSevEnum severity,const char *pFormat,va_list pvar)
{
    char *pnext;
    int	 nchar;
    int	 totalChar=0;
    int  isOkToBlock = epicsThreadIsOkToBlock(epicsThreadGetIdSelf());
    char *pmessage;

    if(pvtData.sevToLog>severity) return(0);
    if(epicsInterruptIsInterruptContext()) {
	epicsInterruptContextMessage
            ("errlogSevVprintf called from interrupt level\n");
	return 0;
    }
    errlogInit(0);
    pnext = msgbufGetFree(!isOkToBlock);
    pmessage = pnext;
    if(!pnext) return(0);
    nchar = sprintf(pnext,"sevr=%s ",errlogGetSevEnumString(severity));
    pnext += nchar; totalChar += nchar;
    nchar = tvsnPrint(pnext,MAX_MESSAGE_SIZE-totalChar-1,pFormat,pvar);
    pnext += nchar; totalChar += nchar;
    if(pnext[-1] != '\n') {
        strcpy(pnext,"\n");
        totalChar++;
    }
    msgbufSetSize(totalChar);
    if(isOkToBlock) printf("%s",pmessage);
    return(nchar);
}

epicsShareFunc char * epicsShareAPI errlogGetSevEnumString(
    const errlogSevEnum severity)
{
    static char unknown[] = "unknown";

    errlogInit(0);
    if(severity<0 || severity>3) return(unknown);
    return(errlogSevEnumString[severity]);
}

epicsShareFunc void epicsShareAPI errlogSetSevToLog(
    const errlogSevEnum severity )
{
    errlogInit(0);
    pvtData.sevToLog = severity;
}

epicsShareFunc errlogSevEnum epicsShareAPI errlogGetSevToLog()
{
    errlogInit(0);
    return(pvtData.sevToLog);
}

epicsShareFunc void epicsShareAPI errlogAddListener(
    errlogListener listener, void *pPrivate)
{
    listenerNode *plistenerNode;

    errlogInit(0);
    plistenerNode = pvtCalloc(1,sizeof(listenerNode));
    epicsMutexMustLock(pvtData.listenerLock);
    plistenerNode->listener = listener;
    plistenerNode->pPrivate = pPrivate;
    ellAdd(&pvtData.listenerList,&plistenerNode->node);
    epicsMutexUnlock(pvtData.listenerLock);
}
    
epicsShareFunc void epicsShareAPI errlogRemoveListener(
    errlogListener listener)
{
    listenerNode *plistenerNode;

    errlogInit(0);
    epicsMutexMustLock(pvtData.listenerLock);
    plistenerNode = (listenerNode *)ellFirst(&pvtData.listenerList);
    while(plistenerNode) {
	if(plistenerNode->listener==listener) {
	    ellDelete(&pvtData.listenerList,&plistenerNode->node);
	    free((void *)plistenerNode);
	    break;
	}
	plistenerNode = (listenerNode *)ellNext(&plistenerNode->node);
    }
    epicsMutexUnlock(pvtData.listenerLock);
    if(!plistenerNode) printf("errlogRemoveListener did not find listener\n");
}

epicsShareFunc int epicsShareAPI eltc(int yesno)
{
    errlogInit(0);
    pvtData.toConsole = yesno;
    return(0);
}

epicsShareFunc void errPrintf(long status, const char *pFileName, 
                                             int lineno, const char *pformat, ...)
{
    va_list pvar;
    char    *pnext;
    int     nchar;
    int     totalChar=0;
    int     isOkToBlock = epicsThreadIsOkToBlock(epicsThreadGetIdSelf());
    char    *pmessage;

    if(epicsInterruptIsInterruptContext()) {
        epicsInterruptContextMessage("errPrintf called from interrupt level\n");
        return;
    }
    errlogInit(0);
    pnext = msgbufGetFree(!isOkToBlock);
    pmessage = pnext;
    if(!pnext) return;
    if(pFileName){
        nchar = sprintf(pnext,"filename=\"%s\" line number=%d\n",
            pFileName, lineno);
        pnext += nchar; totalChar += nchar;
    }
    if(status==0) status = errno;
    if(status>0) {
         char    name[256];

        errSymLookup(status,name,sizeof(name));
        nchar = sprintf(pnext,"%s ",name);
        pnext += nchar; totalChar += nchar;
    }
    va_start (pvar, pformat);
    nchar = tvsnPrint(pnext,MAX_MESSAGE_SIZE,pformat,pvar);
    va_end (pvar);
    if(nchar>0) {
        pnext += nchar;
        totalChar += nchar;
    }
    strcpy(pnext,"\n");
    totalChar++ ; /*include the \n */
    msgbufSetSize(totalChar);
    if(isOkToBlock) printf("%s",pmessage);
}

static void errlogInitPvt(void *arg)
{
    int bufsize = *(int *)arg;
    void	*pbuffer;
    epicsThreadId tid;

    pvtData.errlogInitFailed = TRUE;
    if(bufsize<BUFFER_SIZE) bufsize = BUFFER_SIZE;
    pvtData.buffersize = bufsize;
    ellInit(&pvtData.listenerList);
    ellInit(&pvtData.msgQueue);
    pvtData.toConsole = TRUE;
    pvtData.waitForWork = epicsEventMustCreate(epicsEventEmpty);
    pvtData.listenerLock = epicsMutexMustCreate();
    pvtData.msgQueueLock = epicsMutexMustCreate();
    pvtData.waitForFlush = epicsEventMustCreate(epicsEventEmpty);
    pvtData.flush = epicsEventMustCreate(epicsEventEmpty);
    pvtData.flushLock = epicsMutexMustCreate();
    pbuffer = pvtCalloc(pvtData.buffersize,sizeof(char));
    pvtData.pbuffer = pbuffer;
    tid = epicsThreadCreate("errlog",epicsThreadPriorityLow,
        epicsThreadGetStackSize(epicsThreadStackSmall),
        (EPICSTHREADFUNC)errlogThread,0);
    if(tid) {
        pvtData.errlogInitFailed = FALSE;
    }
}

epicsShareFunc int epicsShareAPI errlogInit(int bufsize)
{
    static epicsThreadOnceId errlogOnceFlag=EPICS_THREAD_ONCE_INIT;

    epicsThreadOnce(&errlogOnceFlag,errlogInitPvt,(void *)&bufsize);
    if(pvtData.errlogInitFailed) {
        fprintf(stderr,"errlogInit failed\n");
        exit(1);
    }
    return(0);
}

epicsShareFunc void epicsShareAPI errlogFlush(void)
{
    int count;
    errlogInit(0);
   /*If nothing in queue dont wake up errlogThread*/
    epicsMutexMustLock(pvtData.msgQueueLock);
    count = ellCount(&pvtData.msgQueue);
    epicsMutexUnlock(pvtData.msgQueueLock);
    if(count<=0) return;
    /*must let errlogThread empty queue*/
    epicsMutexMustLock(pvtData.flushLock);
    epicsEventSignal(pvtData.flush);
    epicsEventSignal(pvtData.waitForWork);
    epicsEventMustWait(pvtData.waitForFlush);
    epicsMutexUnlock(pvtData.flushLock);
}

LOCAL void errlogThread(void)
{
    listenerNode *plistenerNode;
    int noConsoleMessage;

    while(TRUE) {
	char	*pmessage;

	epicsEventMustWait(pvtData.waitForWork);
	while((pmessage = msgbufGetSend(&noConsoleMessage))) {
	    epicsMutexMustLock(pvtData.listenerLock);
	    if(pvtData.toConsole && !noConsoleMessage) printf("%s",pmessage);
	    plistenerNode = (listenerNode *)ellFirst(&pvtData.listenerList);
	    while(plistenerNode) {
		(*plistenerNode->listener)(plistenerNode->pPrivate, pmessage);
		plistenerNode = (listenerNode *)ellNext(&plistenerNode->node);
	    }
            epicsMutexUnlock(pvtData.listenerLock);
	    msgbufFreeSend();
	}
        if(epicsEventTryWait(pvtData.flush)!=epicsEventWaitOK) continue;
        epicsThreadSleep(.2); /*just wait an extra .2 seconds*/
        epicsEventSignal(pvtData.waitForFlush);
    }
}

LOCAL msgNode *msgbufGetNode()
{
    char	*pbuffer = (char *)pvtData.pbuffer;
    msgNode	*pnextSend = 0;

    if(ellCount(&pvtData.msgQueue) == 0 ) {
	pnextSend = (msgNode *)pbuffer;
    } else {
	int	needed;
	int	remaining;
        msgNode	*plast;

	plast = (msgNode *)ellLast(&pvtData.msgQueue);
	needed = MAX_MESSAGE_SIZE + sizeof(msgNode) + MAX_ALIGNMENT;
	remaining = pvtData.buffersize
	    - ((plast->message - pbuffer) + plast->length);
	if(needed < remaining) {
	    int	length,adjust;

	    length = plast->length;
	    /*Make length a multiple of MAX_ALIGNMENT*/
	    adjust = length%MAX_ALIGNMENT;
	    if(adjust) length += (MAX_ALIGNMENT-adjust);
	    pnextSend = (msgNode *)(plast->message + length);
	}
    }
    if(!pnextSend) return(0);
    pnextSend->message = (char *)pnextSend + sizeof(msgNode);
    pnextSend->length = 0;
    return(pnextSend);
}

LOCAL char *msgbufGetFree(int noConsoleMessage)
{
    msgNode	*pnextSend;

    epicsMutexMustLock(pvtData.msgQueueLock);
    if((ellCount(&pvtData.msgQueue) == 0) && pvtData.missedMessages) {
	int	nchar;

	pnextSend = msgbufGetNode();
	nchar = sprintf(pnextSend->message,"errlog = %d messages were discarded\n",
		pvtData.missedMessages);
	pnextSend->length = nchar + 1;
	pvtData.missedMessages = 0;
	ellAdd(&pvtData.msgQueue,&pnextSend->node);
    }
    pvtData.pnextSend = pnextSend = msgbufGetNode();
    if(pnextSend) {
        pnextSend->noConsoleMessage = noConsoleMessage;
        pnextSend->length = 0;
        return(pnextSend->message);
    }
    ++pvtData.missedMessages;
    epicsMutexUnlock(pvtData.msgQueueLock);
    return(0);
}

LOCAL void msgbufSetSize(int size)
{
    msgNode	*pnextSend = pvtData.pnextSend;

    pnextSend->length = size+1;
    ellAdd(&pvtData.msgQueue,&pnextSend->node);
    epicsMutexUnlock(pvtData.msgQueueLock);
    epicsEventSignal(pvtData.waitForWork);
}

/*errlogThread is the only task that calls msgbufGetSend and msgbufFreeSend*/
/*Thus errlogThread is the ONLY task that removes messages from msgQueue	*/
/*This is why each can lock and unlock msgQueue				*/
/*This is necessary to prevent other tasks from waiting for errlogThread	*/
LOCAL char * msgbufGetSend(int *noConsoleMessage)
{
    msgNode	*pnextSend;

    epicsMutexMustLock(pvtData.msgQueueLock);
    pnextSend = (msgNode *)ellFirst(&pvtData.msgQueue);
    epicsMutexUnlock(pvtData.msgQueueLock);
    if(!pnextSend) return(0);
    *noConsoleMessage = pnextSend->noConsoleMessage;
    return(pnextSend->message);
}

LOCAL void msgbufFreeSend()
{
    msgNode	*pnextSend;

    epicsMutexMustLock(pvtData.msgQueueLock);
    pnextSend = (msgNode *)ellFirst(&pvtData.msgQueue);
    if(!pnextSend) {
	printf("errlog: msgbufFreeSend logic error\n");
	epicsThreadSuspendSelf();
    }
    ellDelete(&pvtData.msgQueue,&pnextSend->node);
    epicsMutexUnlock(pvtData.msgQueueLock);
}

LOCAL void *pvtCalloc(size_t count,size_t size)
{
    size_t *pmem;
    
    pmem = calloc(count,size);
    if(!pmem) {
	printf("calloc failed in errlog\n");
	epicsThreadSuspendSelf();
    }
    return(pmem);
}
