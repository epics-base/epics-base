/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 *      Original Author: Marty Kraimer
 *      Date:            07JAN1998
 */

#ifdef _WIN32
#  define VC_EXTRALEAN
#  include <windows.h>
#endif

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#define ERRLOG_INIT
#include "dbDefs.h"
#include "epicsThread.h"
#include "cantProceed.h"
#include "epicsMutex.h"
#include "epicsEvent.h"
#include "epicsString.h"
#include "epicsInterrupt.h"
#include "errMdef.h"
#include "errSymTbl.h"
#include "ellLib.h"
#include "errlog.h"
#include "epicsStdio.h"
#include "epicsExit.h"
#include "osiUnistd.h"


#define MIN_BUFFER_SIZE 1280
#define MIN_MESSAGE_SIZE 256
#define MAX_MESSAGE_SIZE 0x00ffffff

/* errlog buffers contain null terminated strings, each prefixed
 * with a 1 byte header containing flags.
 */
/* State of entries in a buffer. */
#define ERL_STATE_MASK  0xc0
#define ERL_STATE_FREE  0x00
#define ERL_STATE_WRITE 0x80
#define ERL_STATE_READY 0x40
/* should this message be echoed to the console? */
#define ERL_LOCALECHO   0x20

/*Declare storage for errVerbose */
int errVerbose = 0;

static void errlogExitHandler(void *);
static void errlogThread(void);

typedef struct listenerNode{
    ELLNODE node;
    errlogListener listener;
    void *pPrivate;
    unsigned active:1;
    unsigned removed:1;
} listenerNode;

typedef struct {
    char *base;
    size_t pos;
} buffer_t;

static struct {
    /* const after errlogInit() */
    size_t maxMsgSize;
    /* alloc size of both buffer_t::base */
    size_t bufSize;
    int    errlogInitFailed;

    epicsMutexId listenerLock;
    ELLLIST      listenerList;

    /* notify when log->size!=0 */
    epicsEventId waitForWork;
    /* signals when worker increments flushSeq */
    epicsEventId waitForSeq;
    epicsMutexId msgQueueLock;

    /* guarded by msgQueueLock */
    int          atExit;
    int          sevToLog;
    int          toConsole;
    int          ttyConsole;
    FILE         *console;

    /* A loop counter maintained by errlogThread. */
    epicsUInt32 flushSeq;
    size_t nFlushers;
    size_t nLost;

    /* 'log' and 'print' combine to form a double buffer. */
    buffer_t *log;
    buffer_t *print;

    /* actual storage which 'log' and 'print' point to */
    buffer_t bufs[2];
} pvt;

/* Returns an pointer to pvt.maxMsgSize bytes, or NULL if ring buffer is full.
 * When !NULL, caller _must_ later msgbufCommit()
 */
static
char* msgbufAlloc(void)
{
    char *ret = NULL;

    if (epicsInterruptIsInterruptContext()) {
        epicsInterruptContextMessage
            ("errlog called from interrupt level\n");
        return ret;
    }

    errlogInit(0);
    epicsMutexMustLock(pvt.msgQueueLock); /* matched in msgbufCommit() */
    if(pvt.bufSize - pvt.log->pos >= 1+pvt.maxMsgSize) {
        /* there is enough space for the worst cast */
        ret = pvt.log->base + pvt.log->pos;
        ret[0] = ERL_STATE_WRITE;
        ret++;
    }

    if(!ret) {
        pvt.nLost++;
        epicsMutexUnlock(pvt.msgQueueLock);
    }
    return ret;
}

static
size_t msgbufCommit(size_t nchar, int localEcho)
{
    int isOkToBlock = epicsThreadIsOkToBlock();
    int wasEmpty = pvt.log->pos==0;
    int atExit = pvt.atExit;
    char *start = pvt.log->base + pvt.log->pos;

    /* nchar returned by snprintf() is >= maxMsgSize when truncated */
    if(nchar >= pvt.maxMsgSize) {
        const char *trunc = "<<TRUNCATED>>\n";
        nchar = pvt.maxMsgSize - 1u;

        strcpy(start + 1u + nchar - strlen(trunc), trunc);
        /* assert(strlen(start+1u)==nchar); */
    }

    start[1u + nchar] = '\0';

    if(localEcho && isOkToBlock && atExit) {
        /* errlogThread is not running, so we print directly
         * and then abandon the buffer.
         */
        fprintf(pvt.console, "%s", start);

    } else if(!atExit) {
        start[0u] = ERL_STATE_READY | (localEcho ? ERL_LOCALECHO : 0);

        pvt.log->pos += 1u + nchar + 1u;

    } else {
        /* listeners will not see messages logged during errlog shutdown */
    }

    epicsMutexUnlock(pvt.msgQueueLock); /* matched in msgbufAlloc() */

    if(wasEmpty && !atExit)
        epicsEventMustTrigger(pvt.waitForWork);

    if(localEcho && isOkToBlock && !atExit)
        errlogFlush();

    return nchar;
}

static
void errlogSequence(void)
{
    int wakeNext = 0;
    size_t seq;

    if (pvt.atExit)
        return;

    epicsMutexMustLock(pvt.msgQueueLock);
    pvt.nFlushers++;
    seq = pvt.flushSeq;

    while(seq == pvt.flushSeq && !pvt.atExit) {
        epicsMutexUnlock(pvt.msgQueueLock);
        /* force worker to wake and increment seq */
        epicsEventMustTrigger(pvt.waitForWork);
        epicsEventMustWait(pvt.waitForSeq);
        epicsMutexMustLock(pvt.msgQueueLock);
    }

    pvt.nFlushers--;
    wakeNext = pvt.nFlushers!=0u;
    epicsMutexUnlock(pvt.msgQueueLock);

    if(wakeNext)
        epicsEventMustTrigger(pvt.waitForSeq);
}

#if !defined(_WIN32)
static
int isATTY(FILE* fp)
{
    int ret = 0;
    const char* term = getenv("TERM");
    int fd = fileno(fp);

    if(fd>=0 && isatty(fd)==1)
        ret = 1;
    /* We don't want to deal with the termcap database,
     * so assume any non-empty $TERM implies at least some
     * support for ANSI escapes
     */
    /* only attempt to use ANSI escapes if some terminal type is specified */
    if(ret && (!term || !term[0]))
        ret = 0;
    return ret;
}

#else /* _WIN32 */
static
int isATTY(FILE* fp)
{
    HANDLE hand = NULL;
    DWORD mode = 0;
    if(fp==stdout)
        hand = GetStdHandle(STD_OUTPUT_HANDLE);
    else if(fp==stderr)
        hand = GetStdHandle(STD_ERROR_HANDLE);
#ifdef ENABLE_VIRTUAL_TERMINAL_PROCESSING
    if(hand && GetConsoleMode(hand, &mode)) {
        (void)SetConsoleMode(hand, mode|ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        mode = 0u;
        if(GetConsoleMode(hand, &mode) && (mode&ENABLE_VIRTUAL_TERMINAL_PROCESSING))
            return 1;
    }
#else
    (void)hand;
    (void)mode;
#endif
    return 0;
}
#endif

/* in-place removal of ANSI terminal escape sequences.
 * exported for use by unit-test only
 */
LIBCOM_API
void errlogStripANSI(char *msg);

void errlogStripANSI(char *msg)
{
    size_t pos = 0, shift = 0;

    while(1) {
        char c = msg[pos];

        if(c=='\033') { /* ESC */
            c = msg[++pos];
            shift++;

            if(c=='[') {
                /* CSI escape sequence begins */
                pos++;
                shift++;

                /* '\033' '[' [?;0=9]* [A-Za-z]
                 */
                while(1) {
                    c = msg[pos];
                    if(c=='?' || c==';' || (c>='0' && c<='9')) {
                        pos++;
                        shift++;

                    } else {
                        break;
                    }
                }
                c = msg[pos];
                if((c>='A' && c<='Z') || (c>='a' && c<='z')) {
                    pos++;
                    shift++;
                }
            }
        }

        if(shift)
            msg[pos-shift] = c = msg[pos];

        if(c=='\0')
            break;

        pos++;
    }
}

int errlogPrintf(const char *pFormat, ...)
{
    int ret;
    va_list args;
    va_start(args, pFormat);
    ret = errlogVprintf(pFormat, args);
    va_end(args);
    return ret;
}

int errlogVprintf(const char *pFormat,va_list pvar)
{
    int nchar = 0;
    char *buf = msgbufAlloc();

    if(buf) {
        nchar = epicsVsnprintf(buf, pvt.maxMsgSize, pFormat, pvar);
        nchar = msgbufCommit(nchar, pvt.toConsole);
    }
    return nchar;
}

int errlogMessage(const char *message)
{
    errlogPrintf("%s", message);
    return 0;
}

int errlogPrintfNoConsole(const char *pFormat, ...)
{
    va_list pvar;
    int nchar;
    va_start(pvar, pFormat);
    nchar = errlogVprintfNoConsole(pFormat, pvar);
    va_end(pvar);
    return nchar;
}

int errlogVprintfNoConsole(const char *pFormat, va_list pvar)
{
    int nchar = 0;
    char *buf = msgbufAlloc();

    if(buf) {
        nchar = epicsVsnprintf(buf, pvt.maxMsgSize, pFormat, pvar);
        nchar = msgbufCommit(nchar, 0);
    }
    return nchar;
}


int errlogSevPrintf(errlogSevEnum severity, const char *pFormat, ...)
{
    va_list pvar;
    int nchar;
    va_start(pvar, pFormat);
    nchar = errlogSevVprintf(severity, pFormat, pvar);
    va_end(pvar);
    return nchar;
}

int errlogSevVprintf(errlogSevEnum severity, const char *pFormat, va_list pvar)
{
    int nchar = 0;
    char *buf = msgbufAlloc();

    if(buf) {
        nchar = sprintf(buf, "sevr=%s ", errlogGetSevEnumString(severity));
        if(nchar < pvt.maxMsgSize)
            nchar += epicsVsnprintf(buf + nchar, pvt.maxMsgSize - nchar, pFormat, pvar);
        nchar = msgbufCommit(nchar, pvt.toConsole);
    }
    return nchar;
}


const char * errlogGetSevEnumString(errlogSevEnum severity)
{
    errlogInit(0);
    if (severity > 3)
        return "unknown";
    return errlogSevEnumString[severity];
}

void errlogSetSevToLog(errlogSevEnum severity)
{
    errlogInit(0);
    epicsMutexMustLock(pvt.msgQueueLock);
    pvt.sevToLog = severity;
    epicsMutexUnlock(pvt.msgQueueLock);
}

errlogSevEnum errlogGetSevToLog(void)
{
    errlogSevEnum ret;
    errlogInit(0);
    epicsMutexMustLock(pvt.msgQueueLock);
    ret = pvt.sevToLog;
    epicsMutexUnlock(pvt.msgQueueLock);
    return ret;
}

void errlogAddListener(errlogListener listener, void *pPrivate)
{
    listenerNode *plistenerNode;

    errlogInit(0);
    if (pvt.atExit)
        return;

    plistenerNode = callocMustSucceed(1,sizeof(listenerNode),
        "errlogAddListener");
    epicsMutexMustLock(pvt.listenerLock);
    plistenerNode->listener = listener;
    plistenerNode->pPrivate = pPrivate;
    ellAdd(&pvt.listenerList,&plistenerNode->node);
    epicsMutexUnlock(pvt.listenerLock);
}

int errlogRemoveListeners(errlogListener listener, void *pPrivate)
{
    listenerNode *plistenerNode;
    int count = 0;

    errlogInit(0);
    epicsMutexMustLock(pvt.listenerLock);

    plistenerNode = (listenerNode *)ellFirst(&pvt.listenerList);
    while (plistenerNode) {
        listenerNode *pnext = (listenerNode *)ellNext(&plistenerNode->node);

        if (plistenerNode->listener == listener &&
            plistenerNode->pPrivate == pPrivate)
        {
            if(plistenerNode->active) { /* callback removing itself */
                plistenerNode->removed = 1;

            } else {
                ellDelete(&pvt.listenerList, &plistenerNode->node);
                free(plistenerNode);
            }
            ++count;
        }
        plistenerNode = pnext;
    }

    epicsMutexUnlock(pvt.listenerLock);
    return count;
}

int eltc(int yesno)
{
    errlogInit(0);
    epicsMutexMustLock(pvt.msgQueueLock);
    pvt.toConsole = yesno;
    epicsMutexUnlock(pvt.msgQueueLock);
    errlogFlush();
    return 0;
}

int errlogSetConsole(FILE *stream)
{
    errlogInit(0);
    epicsMutexMustLock(pvt.msgQueueLock);
    pvt.console = stream ? stream : stderr;
    pvt.ttyConsole = isATTY(pvt.console);
    epicsMutexUnlock(pvt.msgQueueLock);
    /* make sure worker has stopped writing to the previous stream */
    errlogSequence();
    return 0;
}

void errPrintf(long status, const char *pFileName, int lineno,
    const char *pformat, ...)
{
    va_list pvar;
    int     nchar = 0;
    char *buf = msgbufAlloc();

    va_start(pvar, pformat);

    if(buf) {
        char    name[256] = "";

        if (status > 0) {
            errSymLookup(status, name, sizeof(name));
        }

        nchar = epicsSnprintf(buf, pvt.maxMsgSize, "%s%sfilename=\"%s\" line number=%d",
                              name, status ? " " : "", pFileName, lineno);
        if(nchar < pvt.maxMsgSize)
            nchar += epicsVsnprintf(buf + nchar, pvt.maxMsgSize - nchar, pformat, pvar);
        msgbufCommit(nchar, pvt.toConsole);
    }

    va_end(pvar);
}

/* On *NIX.  also RTEM and vxWorks during controlled shutdown.
 * On Windows when main() explicitly calls epicsExit(0), like default IOC main().
 * Switch to sync. print and join errlogThread.
 *
 * On Windows otherwise, errlogThread killed by exit(), and this handler is never
 *            invoked.  Use of errlog from OS atexit() handler is undefined.
 */
static void errlogExitHandler(void *raw)
{
    epicsThreadId tid = raw;
    epicsMutexMustLock(pvt.msgQueueLock);
    pvt.atExit = 1;
    epicsMutexUnlock(pvt.msgQueueLock);
    epicsEventSignal(pvt.waitForWork);
    epicsThreadMustJoin(tid);
}

struct initArgs {
    size_t bufsize;
    size_t maxMsgSize;
};

static void errlogInitPvt(void *arg)
{
    struct initArgs *pconfig = (struct initArgs *) arg;
    epicsThreadId tid = NULL;
    epicsThreadOpts topts = EPICS_THREAD_OPTS_INIT;

    topts.priority = epicsThreadPriorityLow;
    topts.stackSize = epicsThreadStackSmall;
    topts.joinable = 1;

    /* Use of *Must* alloc functions would recurse on failure since
     * cantProceed() calls us.
     */

    pvt.errlogInitFailed = TRUE;
    pvt.bufSize = pconfig->bufsize;
    pvt.maxMsgSize = pconfig->maxMsgSize;
    ellInit(&pvt.listenerList);
    pvt.toConsole = TRUE;
    pvt.console = stderr;
    pvt.ttyConsole = isATTY(stderr);
    pvt.waitForWork = epicsEventCreate(epicsEventEmpty);
    pvt.listenerLock = epicsMutexCreate();
    pvt.msgQueueLock = epicsMutexCreate();
    pvt.waitForSeq = epicsEventCreate(epicsEventEmpty);
    pvt.log = &pvt.bufs[0];
    pvt.print = &pvt.bufs[1];
    pvt.log->base = calloc(1, pvt.bufSize);
    pvt.print->base = calloc(1, pvt.bufSize);

    errSymBld();    /* Better not to do this lazily... */

    if(pvt.waitForWork
            && pvt.listenerLock
            && pvt.msgQueueLock
            && pvt.waitForSeq
            && pvt.log->base
            && pvt.print->base
            ) {
        tid = epicsThreadCreateOpt("errlog", (EPICSTHREADFUNC)errlogThread, 0, &topts);
    }
    if (tid) {
        pvt.errlogInitFailed = FALSE;
        epicsAtExit(errlogExitHandler, tid);
    }
}


int errlogInit2(int bufsize, int maxMsgSize)
{
    static epicsThreadOnceId errlogOnceFlag = EPICS_THREAD_ONCE_INIT;
    struct initArgs config;

    if (pvt.atExit)
        return 0;

    if (bufsize < MIN_BUFFER_SIZE)
        bufsize = MIN_BUFFER_SIZE;
    config.bufsize = bufsize;

    if (maxMsgSize < MIN_MESSAGE_SIZE)
        maxMsgSize = MIN_MESSAGE_SIZE;
    else if (maxMsgSize > MAX_MESSAGE_SIZE)
        maxMsgSize = MAX_MESSAGE_SIZE;
    config.maxMsgSize = maxMsgSize;

    epicsThreadOnce(&errlogOnceFlag, errlogInitPvt, &config);
    if (pvt.errlogInitFailed) {
        fprintf(stderr,"errlogInit failed\n");
        exit(1);
    }
    return 0;
}

int errlogInit(int bufsize)
{
    return errlogInit2(bufsize, MIN_MESSAGE_SIZE);
}

void errlogFlush(void)
{
    /* wait for both buffers to be handled to know that all currently
     * logged message have been seen/sent.
     */
    errlogInit(0);
    errlogSequence();
    errlogSequence();
}

static void errlogThread(void)
{
    int wakeFlusher;
    epicsMutexMustLock(pvt.msgQueueLock);
    while (1) {
        pvt.flushSeq++;

        if(pvt.log->pos==0u) {
            if(pvt.atExit)
                break;
            wakeFlusher = pvt.nFlushers!=0;
            epicsMutexUnlock(pvt.msgQueueLock);
            if(wakeFlusher)
                epicsEventMustTrigger(pvt.waitForSeq);
            epicsEventMustWait(pvt.waitForWork);
            epicsMutexMustLock(pvt.msgQueueLock);

        } else {
            /* snapshot and swap buffers for use while unlocked */
            size_t nLost = pvt.nLost;
            FILE *console = pvt.toConsole ? pvt.console : NULL;
            int ttyConsole = pvt.ttyConsole;
            size_t pos = 0u;
            buffer_t *print;

            {
                buffer_t *temp = pvt.log;
                pvt.log = pvt.print;
                pvt.print = print = temp;
            }

            pvt.nLost = 0u;
            epicsMutexUnlock(pvt.msgQueueLock);

            while(pos < print->pos) {
                listenerNode *plistenerNode;
                char* base = print->base + pos;
                size_t mlen = epicsStrnLen(base+1u, pvt.bufSize - pos);
                int stripped = 0;

                if((base[0]&ERL_STATE_MASK) != ERL_STATE_READY || mlen>=pvt.bufSize - pos) {
                    fprintf(stderr, "Logic Error: errlog buffer corruption. %02x, %zu\n",
                            (unsigned)base[0], mlen);
                    /* try to reset and recover */
                    break;
                }

                if(base[0]&ERL_LOCALECHO && console) {
                    if(!ttyConsole) {
                        errlogStripANSI(base+1u);
                        stripped = 1;
                    }
                    fprintf(console, "%s", base+1u);
                }

                if(!stripped)
                    errlogStripANSI(base+1u);

                epicsMutexMustLock(pvt.listenerLock);
                plistenerNode = (listenerNode *)ellFirst(&pvt.listenerList);
                while (plistenerNode) {
                    listenerNode *next;

                    plistenerNode->active = 1;
                    (*plistenerNode->listener)(plistenerNode->pPrivate, base+1u);
                    plistenerNode->active = 0;

                    next = (listenerNode *)ellNext(&plistenerNode->node);
                    if(plistenerNode->removed) {
                        /* listener() called errlogRemoveListeners() */
                        ellDelete(&pvt.listenerList, &plistenerNode->node);
                        free(plistenerNode);
                    }
                    plistenerNode = next;
                }
                epicsMutexUnlock(pvt.listenerLock);

                pos += 1u + mlen+1u;
            }

            memset(print->base, 0, pvt.bufSize);
            print->pos = 0u;

            if(nLost && console)
                fprintf(console, "errlog: lost %zu messages\n", nLost);

            if(console)
                fflush(console);

            epicsMutexMustLock(pvt.msgQueueLock);

        }
    }
    wakeFlusher = pvt.nFlushers!=0;
    epicsMutexUnlock(pvt.msgQueueLock);
    if(wakeFlusher)
        epicsEventMustTrigger(pvt.waitForSeq);
}
