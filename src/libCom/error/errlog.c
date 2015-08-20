/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *      Original Author: Marty Kraimer
 *      Date:            07JAN1998
 */

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#define epicsExportSharedSymbols
#define ERRLOG_INIT
#include "adjustment.h"
#include "dbDefs.h"
#include "epicsThread.h"
#include "cantProceed.h"
#include "epicsMutex.h"
#include "epicsEvent.h"
#include "epicsInterrupt.h"
#include "errMdef.h"
#include "error.h"
#include "ellLib.h"
#include "errlog.h"
#include "epicsStdio.h"
#include "epicsExit.h"


#define BUFFER_SIZE 1280
#define MAX_MESSAGE_SIZE 256

/*Declare storage for errVerbose */
epicsShareDef int errVerbose = 0;

static void errlogExitHandler(void *);
static void errlogThread(void);

static char *msgbufGetFree(int noConsoleMessage);
static void msgbufSetSize(int size); /* Send 'size' chars plus trailing '\0' */
static char *msgbufGetSend(int *noConsoleMessage);
static void msgbufFreeSend(void);

typedef struct listenerNode{
    ELLNODE node;
    errlogListener listener;
    void *pPrivate;
} listenerNode;

/*each message consists of a msgNode immediately followed by the message */
typedef struct msgNode {
    ELLNODE node;
    char *message;
    int length;
    int noConsoleMessage;
} msgNode;

static struct {
    epicsEventId waitForWork; /*errlogThread waits for this*/
    epicsMutexId msgQueueLock;
    epicsMutexId listenerLock;
    epicsEventId waitForFlush; /*errlogFlush waits for this*/
    epicsEventId flush; /*errlogFlush sets errlogThread does a Try*/
    epicsMutexId flushLock;
    epicsEventId waitForExit; /*errlogExitHandler waits for this*/
    int          atExit;      /*TRUE when errlogExitHandler is active*/
    ELLLIST      listenerList;
    ELLLIST      msgQueue;
    msgNode      *pnextSend;
    int          errlogInitFailed;
    int          buffersize;
    int          maxMsgSize;
    int          msgNeeded;
    int          sevToLog;
    int          toConsole;
    FILE         *console;
    int          missedMessages;
    char         *pbuffer;
} pvtData;


/*
 * vsnprintf with truncation message
 */
static int tvsnPrint(char *str, size_t size, const char *format, va_list ap)
{
    static const char tmsg[] = "<<TRUNCATED>>\n";
    int nchar = epicsVsnprintf(str, size, format ? format : "", ap);

    if (nchar >= size) {
        if (size > sizeof tmsg)
            strcpy(str + size - sizeof tmsg, tmsg);
        nchar = size - 1;
    }
    return nchar;
}

epicsShareFunc int errlogPrintf(const char *pFormat, ...)
{
    va_list pvar;
    char *pbuffer;
    int nchar;
    int isOkToBlock;

    if (epicsInterruptIsInterruptContext()) {
        epicsInterruptContextMessage
            ("errlogPrintf called from interrupt level\n");
        return 0;
    }

    errlogInit(0);
    isOkToBlock = epicsThreadIsOkToBlock();

    if (pvtData.atExit || (isOkToBlock && pvtData.toConsole)) {
        FILE *console = pvtData.console ? pvtData.console : stderr;

        va_start(pvar, pFormat);
        nchar = vfprintf(console, pFormat, pvar);
        va_end (pvar);
        fflush(console);
    }

    if (pvtData.atExit)
        return nchar;

    pbuffer = msgbufGetFree(isOkToBlock);
    if (!pbuffer)
        return 0;

    va_start(pvar, pFormat);
    nchar = tvsnPrint(pbuffer, pvtData.maxMsgSize, pFormat?pFormat:"", pvar);
    va_end(pvar);
    msgbufSetSize(nchar);
    return nchar;
}

epicsShareFunc int errlogVprintf(
    const char *pFormat,va_list pvar)
{
    int nchar;
    char *pbuffer;
    int isOkToBlock;
    FILE *console;

    if (epicsInterruptIsInterruptContext()) {
        epicsInterruptContextMessage
            ("errlogVprintf called from interrupt level\n");
        return 0;
    }

    errlogInit(0);
    if (pvtData.atExit)
        return 0;
    isOkToBlock = epicsThreadIsOkToBlock();

    pbuffer = msgbufGetFree(isOkToBlock);
    if (!pbuffer) {
        console = pvtData.console ? pvtData.console : stderr;
        vfprintf(console, pFormat, pvar);
        fflush(console);
        return 0;
    }

    nchar = tvsnPrint(pbuffer, pvtData.maxMsgSize, pFormat?pFormat:"", pvar);
    if (pvtData.atExit || (isOkToBlock && pvtData.toConsole)) {
        console = pvtData.console ? pvtData.console : stderr;
        fprintf(console, "%s", pbuffer);
        fflush(console);
    }
    msgbufSetSize(nchar);
    return nchar;
}

epicsShareFunc int epicsShareAPI errlogMessage(const char *message)
{
    errlogPrintf("%s", message);
    return 0;
}

epicsShareFunc int errlogPrintfNoConsole( const char *pFormat, ...)
{
    va_list pvar;
    int nchar;

    if (epicsInterruptIsInterruptContext()) {
        epicsInterruptContextMessage
            ("errlogPrintfNoConsole called from interrupt level\n");
        return 0;
    }

    errlogInit(0);
    va_start(pvar, pFormat);
    nchar = errlogVprintfNoConsole(pFormat, pvar);
    va_end(pvar);
    return nchar;
}

epicsShareFunc int errlogVprintfNoConsole(
    const char *pFormat,va_list pvar)
{
    int nchar;
    char *pbuffer;

    if (epicsInterruptIsInterruptContext()) {
        epicsInterruptContextMessage
            ("errlogVprintfNoConsole called from interrupt level\n");
        return 0;
    }

    errlogInit(0);
    if (pvtData.atExit)
        return 0;

    pbuffer = msgbufGetFree(1);
    if (!pbuffer)
        return 0;

    nchar = tvsnPrint(pbuffer, pvtData.maxMsgSize, pFormat?pFormat:"", pvar);
    msgbufSetSize(nchar);
    return nchar;
}


epicsShareFunc int errlogSevPrintf(
    const errlogSevEnum severity,const char *pFormat, ...)
{
    va_list pvar;
    int nchar;
    int isOkToBlock;

    if (epicsInterruptIsInterruptContext()) {
        epicsInterruptContextMessage
            ("errlogSevPrintf called from interrupt level\n");
        return 0;
    }

    errlogInit(0);
    if (pvtData.sevToLog > severity)
        return 0;

    isOkToBlock = epicsThreadIsOkToBlock();
    if (pvtData.atExit || (isOkToBlock && pvtData.toConsole)) {
        FILE *console = pvtData.console ? pvtData.console : stderr;

        fprintf(console, "sevr=%s ", errlogGetSevEnumString(severity));
        va_start(pvar, pFormat);
        vfprintf(console, pFormat, pvar);
        va_end(pvar);
        fflush(console);
    }

    va_start(pvar, pFormat);
    nchar = errlogSevVprintf(severity, pFormat, pvar);
    va_end(pvar);
    return nchar;
}

epicsShareFunc int errlogSevVprintf(
    const errlogSevEnum severity,const char *pFormat,va_list pvar)
{
    char *pnext;
    int nchar;
    int totalChar = 0;
    int isOkToBlock;

    if (epicsInterruptIsInterruptContext()) {
        epicsInterruptContextMessage
            ("errlogSevVprintf called from interrupt level\n");
        return 0;
    }

    errlogInit(0);
    if (pvtData.atExit)
        return 0;

    isOkToBlock = epicsThreadIsOkToBlock();
    pnext = msgbufGetFree(isOkToBlock);
    if (!pnext)
        return 0;

    nchar = sprintf(pnext, "sevr=%s ", errlogGetSevEnumString(severity));
    pnext += nchar; totalChar += nchar;
    nchar = tvsnPrint(pnext, pvtData.maxMsgSize - totalChar - 1, pFormat, pvar);
    pnext += nchar; totalChar += nchar;
    if (pnext[-1] != '\n') {
        strcpy(pnext,"\n");
        totalChar++;
    }
    msgbufSetSize(totalChar);
    return nchar;
}


epicsShareFunc char * epicsShareAPI errlogGetSevEnumString(
    const errlogSevEnum severity)
{
    errlogInit(0);
    if (severity > 3)
        return "unknown";
    return errlogSevEnumString[severity];
}

epicsShareFunc void epicsShareAPI errlogSetSevToLog(
    const errlogSevEnum severity)
{
    errlogInit(0);
    pvtData.sevToLog = severity;
}

epicsShareFunc errlogSevEnum epicsShareAPI errlogGetSevToLog(void)
{
    errlogInit(0);
    return pvtData.sevToLog;
}

epicsShareFunc void epicsShareAPI errlogAddListener(
    errlogListener listener, void *pPrivate)
{
    listenerNode *plistenerNode;

    errlogInit(0);
    if (pvtData.atExit)
        return;

    plistenerNode = callocMustSucceed(1,sizeof(listenerNode),
        "errlogAddListener");
    epicsMutexMustLock(pvtData.listenerLock);
    plistenerNode->listener = listener;
    plistenerNode->pPrivate = pPrivate;
    ellAdd(&pvtData.listenerList,&plistenerNode->node);
    epicsMutexUnlock(pvtData.listenerLock);
}

epicsShareFunc int epicsShareAPI errlogRemoveListeners(
    errlogListener listener, void *pPrivate)
{
    listenerNode *plistenerNode;
    int count = 0;

    errlogInit(0);
    if (!pvtData.atExit)
        epicsMutexMustLock(pvtData.listenerLock);

    plistenerNode = (listenerNode *)ellFirst(&pvtData.listenerList);
    while (plistenerNode) {
        listenerNode *pnext = (listenerNode *)ellNext(&plistenerNode->node);

        if (plistenerNode->listener == listener &&
            plistenerNode->pPrivate == pPrivate) {
            ellDelete(&pvtData.listenerList, &plistenerNode->node);
            free(plistenerNode);
            ++count;
        }
        plistenerNode = pnext;
    }

    if (!pvtData.atExit)
        epicsMutexUnlock(pvtData.listenerLock);

    if (count == 0) {
        FILE *console = pvtData.console ? pvtData.console : stderr;

        fprintf(console,
            "errlogRemoveListeners: No listeners found\n");
    }
    return count;
}

epicsShareFunc int epicsShareAPI eltc(int yesno)
{
    errlogInit(0);
    errlogFlush();
    pvtData.toConsole = yesno;
    return 0;
}

epicsShareFunc int errlogSetConsole(FILE *stream)
{
    errlogInit(0);
    pvtData.console = stream;
    return 0;
}

epicsShareFunc void errPrintf(long status, const char *pFileName, 
    int lineno, const char *pformat, ...)
{
    va_list pvar;
    char    *pnext;
    int     nchar;
    int     totalChar=0;
    int     isOkToBlock;
    char    name[256];

    if (epicsInterruptIsInterruptContext()) {
        epicsInterruptContextMessage("errPrintf called from interrupt level\n");
        return;
    }

    errlogInit(0);
    isOkToBlock = epicsThreadIsOkToBlock();
    if (status == 0)
        status = errno;

    if (status > 0) {
        errSymLookup(status, name, sizeof(name));
    }

    if (pvtData.atExit || (isOkToBlock && pvtData.toConsole)) {
        FILE *console = pvtData.console ? pvtData.console : stderr;

        if (pFileName)
            fprintf(console, "filename=\"%s\" line number=%d\n",
                pFileName, lineno);
        if (status > 0)
            fprintf(console, "%s ", name);

        va_start(pvar, pformat);
        vfprintf(console, pformat, pvar);
        va_end(pvar);
        fputc('\n', console);
        fflush(console);
    }

    if (pvtData.atExit)
        return;

    pnext = msgbufGetFree(isOkToBlock);
    if (!pnext)
        return;

    if (pFileName) {
        nchar = sprintf(pnext,"filename=\"%s\" line number=%d\n",
            pFileName, lineno);
        pnext += nchar; totalChar += nchar;
    }

    if (status > 0) {
        nchar = sprintf(pnext,"%s ",name);
        pnext += nchar; totalChar += nchar;
    }
    va_start(pvar, pformat);
    nchar = tvsnPrint(pnext, pvtData.maxMsgSize - totalChar - 1, pformat, pvar);
    va_end(pvar);
    if (nchar>0) {
        pnext += nchar;
        totalChar += nchar;
    }
    strcpy(pnext, "\n");
    totalChar++ ; /*include the \n */
    msgbufSetSize(totalChar);
}


static void errlogExitHandler(void *pvt)
{
    pvtData.atExit = 1;
    epicsEventSignal(pvtData.waitForWork);
    epicsEventMustWait(pvtData.waitForExit);
}

struct initArgs {
    int bufsize;
    int maxMsgSize;
};

static void errlogInitPvt(void *arg)
{
    struct initArgs *pconfig = (struct initArgs *) arg;
    epicsThreadId tid;

    pvtData.errlogInitFailed = TRUE;
    pvtData.buffersize = pconfig->bufsize;
    pvtData.maxMsgSize = pconfig->maxMsgSize;
    pvtData.msgNeeded = adjustToWorstCaseAlignment(pvtData.maxMsgSize +
        sizeof(msgNode));
    ellInit(&pvtData.listenerList);
    ellInit(&pvtData.msgQueue);
    pvtData.toConsole = TRUE;
    pvtData.console = NULL;
    pvtData.waitForWork = epicsEventMustCreate(epicsEventEmpty);
    pvtData.listenerLock = epicsMutexMustCreate();
    pvtData.msgQueueLock = epicsMutexMustCreate();
    pvtData.waitForFlush = epicsEventMustCreate(epicsEventEmpty);
    pvtData.flush = epicsEventMustCreate(epicsEventEmpty);
    pvtData.flushLock = epicsMutexMustCreate();
    pvtData.waitForExit = epicsEventMustCreate(epicsEventEmpty);
    pvtData.pbuffer = callocMustSucceed(1, pvtData.buffersize,
        "errlogInitPvt");

    errSymBld();    /* Better not to do this lazily... */

    tid = epicsThreadCreate("errlog", epicsThreadPriorityLow,
        epicsThreadGetStackSize(epicsThreadStackSmall),
        (EPICSTHREADFUNC)errlogThread, 0);
    if (tid) {
        pvtData.errlogInitFailed = FALSE;
    }
}


epicsShareFunc int epicsShareAPI errlogInit2(int bufsize, int maxMsgSize)
{
    static epicsThreadOnceId errlogOnceFlag = EPICS_THREAD_ONCE_INIT;
    struct initArgs config;

    if (pvtData.atExit)
        return 0;

    if (bufsize < BUFFER_SIZE)
        bufsize = BUFFER_SIZE;
    config.bufsize = bufsize;

    if (maxMsgSize < MAX_MESSAGE_SIZE)
        maxMsgSize = MAX_MESSAGE_SIZE;
    config.maxMsgSize = maxMsgSize;

    epicsThreadOnce(&errlogOnceFlag, errlogInitPvt, &config);
    if (pvtData.errlogInitFailed) {
        fprintf(stderr,"errlogInit failed\n");
        exit(1);
    }
    return 0;
}

epicsShareFunc int epicsShareAPI errlogInit(int bufsize)
{
    return errlogInit2(bufsize, MAX_MESSAGE_SIZE);
}

epicsShareFunc void epicsShareAPI errlogFlush(void)
{
    int count;

    errlogInit(0);
    if (pvtData.atExit)
        return;

   /*If nothing in queue dont wake up errlogThread*/
    epicsMutexMustLock(pvtData.msgQueueLock);
    count = ellCount(&pvtData.msgQueue);
    epicsMutexUnlock(pvtData.msgQueueLock);
    if (count <= 0)
        return;

    /*must let errlogThread empty queue*/
    epicsMutexMustLock(pvtData.flushLock);
    epicsEventSignal(pvtData.flush);
    epicsEventSignal(pvtData.waitForWork);
    epicsEventMustWait(pvtData.waitForFlush);
    epicsMutexUnlock(pvtData.flushLock);
}

static void errlogThread(void)
{
    listenerNode *plistenerNode;
    int noConsoleMessage;
    char *pmessage;

    epicsAtExit(errlogExitHandler,0);
    while (TRUE) {
        epicsEventMustWait(pvtData.waitForWork);
        while ((pmessage = msgbufGetSend(&noConsoleMessage))) {
            epicsMutexMustLock(pvtData.listenerLock);
            if (pvtData.toConsole && !noConsoleMessage) {
                FILE *console = pvtData.console ? pvtData.console : stderr;

                fprintf(console, "%s", pmessage);
                fflush(console);
            }

            plistenerNode = (listenerNode *)ellFirst(&pvtData.listenerList);
            while (plistenerNode) {
                (*plistenerNode->listener)(plistenerNode->pPrivate, pmessage);
                plistenerNode = (listenerNode *)ellNext(&plistenerNode->node);
            }

            epicsMutexUnlock(pvtData.listenerLock);
            msgbufFreeSend();
        }

        if (pvtData.atExit)
            break;
        if (epicsEventTryWait(pvtData.flush) != epicsEventWaitOK)
            continue;

        epicsThreadSleep(.2); /*just wait an extra .2 seconds*/
        epicsEventSignal(pvtData.waitForFlush);
    }
    epicsEventSignal(pvtData.waitForExit);
}


static msgNode *msgbufGetNode(void)
{
    char *pbuffer = pvtData.pbuffer;
    char *pnextFree;
    msgNode *pnextSend;

    if (ellCount(&pvtData.msgQueue) == 0 ) {
        pnextFree = pbuffer;        /* Reset if empty */
    }
    else {
        msgNode *pfirst = (msgNode *)ellFirst(&pvtData.msgQueue);
        msgNode *plast = (msgNode *)ellLast(&pvtData.msgQueue);
        char *plimit = pbuffer + pvtData.buffersize;

        pnextFree = plast->message + adjustToWorstCaseAlignment(plast->length);
        if (pfirst > plast) {
            plimit = (char *)pfirst;
        }
        else if (pnextFree + pvtData.msgNeeded > plimit) {
            pnextFree = pbuffer;    /* Hit end, wrap to start */
            plimit = (char *)pfirst;
        }
        if (pnextFree + pvtData.msgNeeded > plimit) {
            return 0;               /* No room */
        }
    }

    pnextSend = (msgNode *)pnextFree;
    pnextSend->message = pnextFree + sizeof(msgNode);
    pnextSend->length = 0;
    return pnextSend;
}

static char *msgbufGetFree(int noConsoleMessage)
{
    msgNode *pnextSend;

    if (epicsMutexLock(pvtData.msgQueueLock) != epicsMutexLockOK)
        return 0;

    if ((ellCount(&pvtData.msgQueue) == 0) && pvtData.missedMessages) {
        int nchar;

        pnextSend = msgbufGetNode();
        nchar = sprintf(pnextSend->message,
            "errlog: %d messages were discarded\n", pvtData.missedMessages);
        pnextSend->length = nchar + 1;
        pvtData.missedMessages = 0;
        ellAdd(&pvtData.msgQueue, &pnextSend->node);
    }

    pvtData.pnextSend = pnextSend = msgbufGetNode();
    if (pnextSend) {
        pnextSend->noConsoleMessage = noConsoleMessage;
        pnextSend->length = 0;
        return pnextSend->message;  /* NB: msgQueueLock is still locked */
    }

    ++pvtData.missedMessages;
    epicsMutexUnlock(pvtData.msgQueueLock);
    return 0;
}

static void msgbufSetSize(int size)
{
    msgNode *pnextSend = pvtData.pnextSend;

    pnextSend->length = size+1;
    ellAdd(&pvtData.msgQueue, &pnextSend->node);
    epicsMutexUnlock(pvtData.msgQueueLock);
    epicsEventSignal(pvtData.waitForWork);
}


static char * msgbufGetSend(int *noConsoleMessage)
{
    msgNode *pnextSend;

    epicsMutexMustLock(pvtData.msgQueueLock);
    pnextSend = (msgNode *)ellFirst(&pvtData.msgQueue);
    epicsMutexUnlock(pvtData.msgQueueLock);
    if (!pnextSend)
        return 0;

    *noConsoleMessage = pnextSend->noConsoleMessage;
    return pnextSend->message;
}

static void msgbufFreeSend(void)
{
    msgNode *pnextSend;

    epicsMutexMustLock(pvtData.msgQueueLock);
    pnextSend = (msgNode *)ellFirst(&pvtData.msgQueue);
    if (!pnextSend) {
        FILE *console = pvtData.console ? pvtData.console : stderr;

        fprintf(console, "errlog: msgbufFreeSend logic error\n");
        epicsThreadSuspendSelf();
    }
    ellDelete(&pvtData.msgQueue, &pnextSend->node);
    epicsMutexUnlock(pvtData.msgQueueLock);
}
