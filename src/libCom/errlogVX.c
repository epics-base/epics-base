/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************
 
(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/
/*
 *      Author:          Marty Kraimer and Jeff Hill
 *      Date:            07JAN1998
 *	NOTE: Original version is adaptation of old version of errPrintfVX.c
*/

#include <vxWorks.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <taskLib.h>
#include <intLib.h>
#include <semLib.h>
#include <vxLib.h>
#include <errnoLib.h>
#include <logLib.h>

#define ERRLOG_INIT
#include "epicsAssert.h"
#include "errMdef.h"
#include "error.h"
#include "ellLib.h"
#include "task_params.h"
#include "errlog.h"


#ifndef LOCAL
#define LOCAL static
#endif /* LOCAL */

#define BUFFER_SIZE 1280
#define MAX_MESSAGE_SIZE 256
#define TRUNCATE_SIZE 80
#define MAX_ALIGNMENT 8

LOCAL void errlogTask(void);

LOCAL char *msgbufGetFree(void);
LOCAL void msgbufSetSize(int size);
LOCAL char * msgbufGetSend(void);
LOCAL void msgbufFreeSend(void);

LOCAL void *pvtCalloc(size_t count,size_t size);
LOCAL void pvtSemTake(SEM_ID semid);

typedef struct listenerNode{
    ELLNODE	node;
    errlogListener listener;
}listenerNode;

/*each message consists of a msgNode immediately followed by the message */
typedef struct msgNode {
    ELLNODE	node;
    char	*message;
    int		length;
} msgNode;

LOCAL struct {
    SEM_ID	errlogTaskWaitForWork;
    SEM_ID	msgQueueLock;
    SEM_ID	listenerLock;
    ELLLIST	listenerList;
    ELLLIST	msgQueue;
    msgNode	*pnextSend;
    int		buffersize;
    int		sevToLog;
    int		toConsole;
    int		missedMessages;
    void	*pbuffer;
}pvtData;

epicsShareFunc int epicsShareAPIV errlogPrintf( const char *pFormat, ...)
{
    va_list	pvar;
    int		nchar;

    if(INT_CONTEXT()) {
	logMsg("errlogPrintf called from interrupt level\n",0,0,0,0,0,0);
	return 0;
    }
    errlogInit(0);
    va_start(pvar, pFormat);
    nchar = errlogVprintf(pFormat,pvar);
    va_end (pvar);
    return(nchar);
}

epicsShareFunc int epicsShareAPIV errlogVprintf(
    const char *pFormat,va_list pvar)
{
    int nchar;
    char *pbuffer;

    if(INT_CONTEXT()) {
	logMsg("errlogVprintf called from interrupt level\n",0,0,0,0,0,0);
	return 0;
    }
    errlogInit(0);
    pbuffer = msgbufGetFree();
    if(!pbuffer) return(0);
    nchar = vsprintf(pbuffer,pFormat,pvar);
    msgbufSetSize(nchar+1);/*include the \0*/
    return nchar;
}

epicsShareFunc int epicsShareAPI errlogMessage(const char *message)
{
    int  status;
    char *pbuffer;

    if(INT_CONTEXT()) {
	status = logMsg ("errlogMessage called from interrupt level %s",
	    message,0,0,0,0,0);
	return 0;
    }
    errlogInit(0);
    pbuffer = msgbufGetFree();
    if(!pbuffer) return(0);
    strcpy(pbuffer,message);
    msgbufSetSize(strlen(message) +1);
    return 0;
}

epicsShareFunc int epicsShareAPIV errlogSevPrintf(
    const errlogSevEnum severity,const char *pFormat, ...)
{
    va_list	pvar;
    int		nchar;

    if(INT_CONTEXT()) {
	logMsg("errlogSevPrintf called from interrupt level\n",0,0,0,0,0,0);
	return 0;
    }
    errlogInit(0);
    if(pvtData.sevToLog>severity) return(0);
    va_start(pvar, pFormat);
    nchar = errlogSevVprintf(severity,pFormat,pvar);
    va_end (pvar);
    return(nchar);
}

epicsShareFunc int epicsShareAPIV errlogSevVprintf(
    const errlogSevEnum severity,const char *pFormat,va_list pvar)
{
    char	*pnext;
    int		nchar;
    int		totalChar=0;

    if(pvtData.sevToLog>severity) return(0);
    if(INT_CONTEXT()) {
	logMsg("errlogSevVprintf called from interrupt level\n",0,0,0,0,0,0);
	return 0;
    }
    errlogInit(0);
    pnext = msgbufGetFree();
    if(!pnext) return(0);
    nchar = sprintf(pnext,"sevr=%s ",errlogGetSevEnumString(severity));
    pnext += nchar; totalChar += nchar;
    nchar = vsprintf(pnext,pFormat,pvar);
    pnext += nchar; totalChar += nchar;
    sprintf(pnext,"\n");
    totalChar += 2; /*include \n and \0*/
    msgbufSetSize(totalChar);
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
    errlogListener listener)
{
    listenerNode *plistenerNode;

    errlogInit(0);
    plistenerNode = pvtCalloc(1,sizeof(listenerNode));
    pvtSemTake(pvtData.listenerLock);
    plistenerNode->listener = listener;
    ellAdd(&pvtData.listenerList,&plistenerNode->node);
    semGive(pvtData.listenerLock);
}
    
epicsShareFunc void epicsShareAPI errlogRemoveListener(
    errlogListener listener)
{
    listenerNode *plistenerNode;

    errlogInit(0);
    pvtSemTake(pvtData.listenerLock);
    plistenerNode = (listenerNode *)ellFirst(&pvtData.listenerList);
    while(plistenerNode) {
	if(plistenerNode->listener==listener) {
	    ellDelete(&pvtData.listenerList,&plistenerNode->node);
	    free((void *)plistenerNode);
	    break;
	}
	plistenerNode = (listenerNode *)ellNext(&plistenerNode->node);
    }
    semGive(pvtData.listenerLock);
    if(!plistenerNode) printf("errlogRemoveListener did not find listener\n");
}

epicsShareFunc void epicsShareAPI eltc(int yesno)
{
    errlogInit(0);
    pvtData.toConsole = yesno;
}

epicsShareFunc void epicsShareAPIV errPrintf(long status, const char *pFileName, 
	int lineno, const char *pformat, ...)
{
    va_list	pvar;
    char	*pnext;
    int		nchar;
    int		totalChar=0;

    if(INT_CONTEXT()) {
	logMsg("errPrintf called from interrupt level\n",0,0,0,0,0,0);
	return;
    }
    errlogInit(0);
    pnext = msgbufGetFree();
    if(!pnext) return;
    if(pFileName){
	nchar = sprintf(pnext,"filename=\"%s\" line number=%d\n",
	    pFileName, lineno);
	pnext += nchar; totalChar += nchar;
    }
    if(status==0) status = MYERRNO;
    if(status>0) {
	int		rtnval;
	char    	name[256];

	rtnval = errSymFind(status,name);
	if(rtnval) {
	    unsigned short	modnum,errnum;

	    modnum = status >> 16; errnum = status & 0xffff;
	    nchar = sprintf(pnext, "status (%hu,%hu) not in symbol table ",
		 modnum, errnum);
	} else {
	    nchar = sprintf(pnext,"%s ",name);
	}
	pnext += nchar; totalChar += nchar;
    }
    va_start (pvar, pformat);
    nchar = vsprintf(pnext,pformat,pvar);
    va_end (pvar);
    if(nchar>0) {
	pnext += nchar;
	totalChar += nchar;
    }
    sprintf(pnext,"\n");
    totalChar += 2; /*include the \n and the \0*/
    msgbufSetSize(totalChar);
}

#define optionsSemM SEM_Q_PRIORITY|SEM_DELETE_SAFE|SEM_INVERSION_SAFE
epicsShareFunc void epicsShareAPI errlogInit(int bufsize)
{
    static int errlogInitFlag=0;
    void	*pbuffer;;

    if(!vxTas(&errlogInitFlag)) return;
    if(bufsize<BUFFER_SIZE) bufsize = BUFFER_SIZE;
    pvtData.buffersize = bufsize;
    ellInit(&pvtData.listenerList);
    ellInit(&pvtData.msgQueue);
    pvtData.toConsole = TRUE;
    if((pvtData.errlogTaskWaitForWork=semBCreate(SEM_Q_FIFO,SEM_EMPTY))==NULL) {
	logMsg("semBCreate failed in errlogInit",0,0,0,0,0,0);
	taskSuspend(0);
    }
    if((pvtData.listenerLock=semMCreate(optionsSemM))==NULL) {
	logMsg("semMCreate failed in errlogInit",0,0,0,0,0,0);
	taskSuspend(0);
    }
    if((pvtData.msgQueueLock=semMCreate(optionsSemM))==NULL) {
	logMsg("semMCreate failed in errlogInit",0,0,0,0,0,0);
	taskSuspend(0);
    }
    /*Allow an extra MAX_MESSAGE_SIZE for extra margain of safety*/
    pbuffer = pvtCalloc(pvtData.buffersize+MAX_MESSAGE_SIZE,sizeof(char));
    pvtData.pbuffer = pbuffer;
    taskSpawn(ERRLOG_NAME,ERRLOG_PRI,ERRLOG_OPT,
	ERRLOG_STACK,(FUNCPTR)errlogTask,
	0,0,0,0,0,0,0,0,0,0);
    /*For now make sure iocLogInit is called*/
    iocLogInit();
}

LOCAL void errlogTask(void)
{
    listenerNode *plistenerNode;

    while(TRUE) {
	char	*pmessage;

	pvtSemTake(pvtData.errlogTaskWaitForWork);
	while((pmessage = msgbufGetSend())) {
	    pvtSemTake(pvtData.listenerLock);
	    if(pvtData.toConsole) printf("%s",pmessage);
	    plistenerNode = (listenerNode *)ellFirst(&pvtData.listenerList);
	    while(plistenerNode) {
		(*plistenerNode->listener)(pmessage);
		plistenerNode = (listenerNode *)ellNext(&plistenerNode->node);
	    }
	    semGive(pvtData.listenerLock);
	    msgbufFreeSend();
	}
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

LOCAL char *msgbufGetFree()
{
    msgNode	*pnextSend;

    pvtSemTake(pvtData.msgQueueLock);
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
    if(pnextSend) return(pnextSend->message);
    ++pvtData.missedMessages;
    semGive(pvtData.msgQueueLock);
    return(0);
}

LOCAL void msgbufSetSize(int size)
{
    char	*pbuffer = (char *)pvtData.pbuffer;
    msgNode	*pnextSend = pvtData.pnextSend;
    char	*message = pnextSend->message;

    if(size>MAX_MESSAGE_SIZE) {
	int excess;
	int nchar;

        excess = size  - (pvtData.buffersize -(message - pbuffer));
	message[TRUNCATE_SIZE] = 0;
	if(excess> 0) {
	    printf("errlog: A message overran buffer by %d. This is VERY bad\n",
		excess);
	    nchar = sprintf(&message[TRUNCATE_SIZE],
	      "\nerrlog = previous message overran buffer. It was truncated."
	      " size = %d excess = %d\n", size,excess);
	} else {
	    nchar = sprintf(&message[TRUNCATE_SIZE],
		"\nerrlog = previous message too long. It was truncated."
		" size=%d It was truncated\n",size);
	}
	pnextSend->length = TRUNCATE_SIZE + nchar +1;
    } else {
        pnextSend->length = size+1;
    }
    ellAdd(&pvtData.msgQueue,&pnextSend->node);
    semGive(pvtData.msgQueueLock);
    semGive(pvtData.errlogTaskWaitForWork);
}

/*errlogTask is the only task that calls msgbufGetSend and msgbufFreeSend*/
/*Thus errlogTask is the ONLY task that removes messages from msgQueue	*/
/*This is why each can lock and unlock msgQueue				*/
/*This is necessary to prevent other tasks from waiting for errlogTask	*/
LOCAL char * msgbufGetSend()
{
    msgNode	*pnextSend;

    pvtSemTake(pvtData.msgQueueLock);
    pnextSend = (msgNode *)ellFirst(&pvtData.msgQueue);
    semGive(pvtData.msgQueueLock);
    if(!pnextSend) return(0);
    return(pnextSend->message);
}

LOCAL void msgbufFreeSend()
{
    msgNode	*pnextSend;

    pvtSemTake(pvtData.msgQueueLock);
    pnextSend = (msgNode *)ellFirst(&pvtData.msgQueue);
    if(!pnextSend) {
	printf("errlog: msgbufFreeSend logic error\n");
	taskSuspend(0);
    }
    ellDelete(&pvtData.msgQueue,&pnextSend->node);
    semGive(pvtData.msgQueueLock);
}

LOCAL void *pvtCalloc(size_t count,size_t size)
{
    size_t *pmem;
    
    pmem = calloc(count,size);
    if(!pmem) {
	printf("calloc failed in errlog\n");
	taskSuspend(0);
    }
    return(pmem);
}

LOCAL void pvtSemTake(SEM_ID semid)
{
    if(semTake(semid,WAIT_FOREVER)!=OK) {
	logMsg("epicsPrint: semTake returned error\n",0,0,0,0,0,0);
	taskSuspend(0);
    }
}
