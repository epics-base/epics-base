/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Copyright (c) 2013 Helmholtz-Zentrum Berlin
*     für Materialien und Energie GmbH.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbScan.c */
/* tasks and subroutines to scan the database */
/*
 *      Original Authors: Bob Dalesio & Marty Kraimer
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <ctype.h>

#include "cantProceed.h"
#include "dbDefs.h"
#include "ellLib.h"
#include "epicsEvent.h"
#include "epicsMutex.h"
#include "epicsPrint.h"
#include "epicsRingBytes.h"
#include "epicsStdio.h"
#include "epicsStdlib.h"
#include "epicsString.h"
#include "epicsThread.h"
#include "epicsTime.h"
#include "taskwd.h"

#define epicsExportSharedSymbols
#include "callback.h"
#include "dbAccessDefs.h"
#include "dbAddr.h"
#include "dbBase.h"
#include "dbCommon.h"
#include "dbFldTypes.h"
#include "dbLock.h"
#include "dbScan.h"
#include "dbStaticLib.h"
#include "devSup.h"
#include "link.h"
#include "recGbl.h"


/* Task Control */
enum ctl {ctlInit, ctlRun, ctlPause, ctlExit};

/* Task Startup/Shutdown Synchronization */
static epicsEventId startStopEvent;

static volatile enum ctl scanCtl;

/* SCAN ONCE */

static int onceQueueSize = 1000;
static epicsEventId onceSem;
static epicsRingBytesId onceQ;
static epicsThreadId onceTaskId;
static void *exitOnce;


/* All other scan types */
typedef struct scan_list{
    epicsMutexId        lock;
    ELLLIST             list;
    short               modified;/*has list been modified?*/
} scan_list;
/*scan_elements are allocated and the address stored in dbCommon.spvt*/
typedef struct scan_element{
    ELLNODE             node;
    scan_list           *pscan_list;
    struct dbCommon     *precord;
} scan_element;


/* PERIODIC */

#define OVERRUN_REPORT_DELAY 10.0   /* Time between initial reports */
#define OVERRUN_REPORT_MAX 3600.0   /* Maximum time between reports */
typedef struct periodic_scan_list {
    scan_list           scan_list;
    double              period;
    const char          *name;
    unsigned long       overruns;
    volatile enum ctl   scanCtl;
    epicsEventId        loopEvent;
} periodic_scan_list;

static int nPeriodic = 0;
static periodic_scan_list **papPeriodic; /* pointer to array of pointers */
static epicsThreadId *periodicTaskId;    /* array of thread ids */


static char *priorityName[NUM_CALLBACK_PRIORITIES] = {
    "Low", "Medium", "High"
};


/* EVENT */

typedef struct event_list {
    CALLBACK            callback[NUM_CALLBACK_PRIORITIES];
    scan_list           scan_list[NUM_CALLBACK_PRIORITIES];
    struct event_list *next;
    char                event_name[MAX_STRING_SIZE];
} event_list;
static event_list * volatile pevent_list[256];
static epicsMutexId event_lock;

/* IO_EVENT*/

typedef struct io_scan_list {
    CALLBACK callback;
    scan_list scan_list;
} io_scan_list;

typedef struct ioscan_head {
    struct ioscan_head *next;
    struct io_scan_list iosl[NUM_CALLBACK_PRIORITIES];
    io_scan_complete cb;
    void *arg;
} ioscan_head;

static ioscan_head *pioscan_list = NULL;
static epicsMutexId ioscan_lock;

/* Private routines */
static void onceTask(void *);
static void initOnce(void);
static void periodicTask(void *arg);
static void initPeriodic(void);
static void deletePeriodic(void);
static void spawnPeriodic(int ind);
static void eventCallback(CALLBACK *pcallback);
static void ioscanInit(void);
static void ioscanCallback(CALLBACK *pcallback);
static void ioscanDestroy(void);
static void printList(scan_list *psl, char *message);
static void scanList(scan_list *psl);
static void buildScanLists(void);
static void addToList(struct dbCommon *precord, scan_list *psl);
static void deleteFromList(struct dbCommon *precord, scan_list *psl);

void scanStop(void)
{
    int i;

    if (scanCtl == ctlExit) return;
    scanCtl = ctlExit;

    interruptAccept = FALSE;

    for (i = 0; i < nPeriodic; i++) {
        periodic_scan_list *ppsl = papPeriodic[i];

        if (!ppsl) continue;
        ppsl->scanCtl = ctlExit;
        epicsEventSignal(ppsl->loopEvent);
        epicsEventWait(startStopEvent);
    }

    scanOnce((dbCommon *)&exitOnce);
    epicsEventWait(startStopEvent);
}

void scanCleanup(void)
{

    deletePeriodic();
    ioscanDestroy();

    epicsRingBytesDelete(onceQ);

    free(periodicTaskId);
    papPeriodic = NULL;
    periodicTaskId = NULL;
}

long scanInit(void)
{
    int i;

    if(!startStopEvent)
        startStopEvent = epicsEventMustCreate(epicsEventEmpty);
    scanCtl = ctlPause;

    initPeriodic();
    initOnce();
    buildScanLists();
    for (i = 0; i < nPeriodic; i++)
        spawnPeriodic(i);

    return 0;
}

void scanRun(void)
{
    int i;

    interruptAccept = TRUE;
    scanCtl = ctlRun;

    for (i = 0; i < nPeriodic; i++) {
        periodic_scan_list *ppsl = papPeriodic[i];

        if (!ppsl) continue;
        ppsl->scanCtl = ctlRun;
    }
}

void scanPause(void)
{
    int i;

    for (i = nPeriodic - 1; i >= 0; --i) {
        periodic_scan_list *ppsl = papPeriodic[i];

        if (!ppsl) continue;
        ppsl->scanCtl = ctlPause;
    }

    scanCtl = ctlPause;
    interruptAccept = FALSE;
}

void scanAdd(struct dbCommon *precord)
{
    int scan;

    /* get the list on which this record belongs */
    scan = precord->scan;
    if (scan == menuScanPassive) return;
    if (scan < 0 || scan >= nPeriodic + SCAN_1ST_PERIODIC) {
        recGblRecordError(-1, (void *)precord,
            "scanAdd detected illegal SCAN value");
    } else if (scan == menuScanEvent) {
        char* eventname;
        int prio;
        event_list *pel;

        eventname = precord->evnt;
        if (strlen(eventname) >= MAX_STRING_SIZE) {
            recGblRecordError(S_db_badField, (void *)precord,
                "scanAdd: too long EVNT value");
            return;
        }
        prio = precord->prio;
        if (prio < 0 || prio >= NUM_CALLBACK_PRIORITIES) {
            recGblRecordError(-1, (void *)precord,
                "scanAdd: illegal prio field");
            return;
        }
        pel = eventNameToHandle(eventname);
        if (pel) addToList(precord, &pel->scan_list[prio]);
    } else if (scan == menuScanI_O_Intr) {
        ioscan_head *piosh = NULL;
        int prio;
        DEVSUPFUN get_ioint_info;

        if (precord->dset == NULL){
            recGblRecordError(-1, (void *)precord,
                "scanAdd: I/O Intr not valid (no DSET) ");
            precord->scan = menuScanPassive;
            return;
        }
        get_ioint_info = precord->dset->get_ioint_info;
        if (get_ioint_info == NULL) {
            recGblRecordError(-1, (void *)precord,
                "scanAdd: I/O Intr not valid (no get_ioint_info)");
            precord->scan = menuScanPassive;
            return;
        }
        if (get_ioint_info(0, precord, &piosh)) {
            precord->scan = menuScanPassive;
            return;
        }
        if (piosh == NULL) {
            recGblRecordError(-1, (void *)precord,
                "scanAdd: I/O Intr not valid");
            precord->scan = menuScanPassive;
            return;
        }
        prio = precord->prio;
        if (prio < 0 || prio >= NUM_CALLBACK_PRIORITIES) {
            recGblRecordError(-1, (void *)precord,
                "scanAdd: illegal prio field");
            precord->scan = menuScanPassive;
            return;
        }
        addToList(precord, &piosh->iosl[prio].scan_list);
    } else if (scan >= SCAN_1ST_PERIODIC) {
        periodic_scan_list *ppsl = papPeriodic[scan - SCAN_1ST_PERIODIC];

        if (ppsl)
            addToList(precord, &ppsl->scan_list);
    }
}

void scanDelete(struct dbCommon *precord)
{
    short scan;

    /* get the list on which this record belongs */
    scan = precord->scan;
    if (scan == menuScanPassive) return;
    if (scan < 0 || scan >= nPeriodic + SCAN_1ST_PERIODIC) {
        recGblRecordError(-1, (void *)precord,
            "scanDelete detected illegal SCAN value");
    } else if (scan == menuScanEvent) {
        char* eventname;
        int prio;
        event_list *pel;
        scan_list *psl = 0;

        eventname = precord->evnt;
        prio = precord->prio;
        if (prio < 0 || prio >= NUM_CALLBACK_PRIORITIES) {
            recGblRecordError(-1, (void *)precord,
                "scanDelete detected illegal PRIO field");
            return;
        }
        do /* multithreading: make sure pel is consistent */
            pel = pevent_list[0];
        while (pel != pevent_list[0]);
        for (; pel; pel=pel->next) {
            if (strcmp(pel->event_name, eventname) == 0) break;
        }
        if (pel && (psl = &pel->scan_list[prio]))
            deleteFromList(precord, psl);
    } else if (scan == menuScanI_O_Intr) {
        ioscan_head *piosh = NULL;
        int prio;
        DEVSUPFUN get_ioint_info;

        if (precord->dset==NULL) {
            recGblRecordError(-1, (void *)precord,
                "scanDelete: I/O Intr not valid (no DSET)");
            return;
        }
        get_ioint_info=precord->dset->get_ioint_info;
        if (get_ioint_info == NULL) {
            recGblRecordError(-1, (void *)precord,
                "scanDelete: I/O Intr not valid (no get_ioint_info)");
            return;
        }
        if (get_ioint_info(1, precord, &piosh)) return;
        if (piosh == NULL) {
            recGblRecordError(-1, (void *)precord,
                "scanDelete: I/O Intr not valid");
            return;
        }
        prio = precord->prio;
        if (prio < 0 || prio >= NUM_CALLBACK_PRIORITIES) {
            recGblRecordError(-1, (void *)precord,
                "scanDelete: get_ioint_info returned illegal priority");
            return;
        }
        deleteFromList(precord, &piosh->iosl[prio].scan_list);
    } else if (scan >= SCAN_1ST_PERIODIC) {
        periodic_scan_list *ppsl = papPeriodic[scan - SCAN_1ST_PERIODIC];

        if (ppsl)
            deleteFromList(precord, &ppsl->scan_list);
    }
}

double scanPeriod(int scan) {
    periodic_scan_list *ppsl;

    scan -= SCAN_1ST_PERIODIC;
    if (scan < 0 || scan >= nPeriodic)
        return 0.0;
    ppsl = papPeriodic[scan];
    return ppsl ? ppsl->period : 0.0;
}

int scanppl(double period)      /* print periodic scan list(s) */
{
    dbMenu *pmenu = dbFindMenu(pdbbase, "menuScan");
    char message[80];
    int i;

    if (!pmenu || !papPeriodic) {
        printf("scanppl: dbScan subsystem not initialized\n");
        return -1;
    }

    for (i = 0; i < nPeriodic; i++) {
        periodic_scan_list *ppsl = papPeriodic[i];

        if (!ppsl) {
            const char *choice = pmenu->papChoiceValue[i + SCAN_1ST_PERIODIC];

            printf("Periodic scan list for SCAN = '%s' not initialized\n",
                choice);
            continue;
        }
        if (period > 0.0 &&
            (fabs(period - ppsl->period) > 0.05))
            continue;

        sprintf(message, "Records with SCAN = '%s' (%lu over-runs):",
            ppsl->name, ppsl->overruns);
        printList(&ppsl->scan_list, message);
    }
    return 0;
}

int scanpel(const char* eventname)   /* print event list */
{
    char message[80];
    int prio;
    event_list *pel;

    do /* multithreading: make sure pel is consistent */
        pel = pevent_list[0];
    while (pel != pevent_list[0]);
    for (; pel; pel = pel->next) {
        if (!eventname || strcmp(pel->event_name, eventname) == 0) {
            for (prio = 0; prio < NUM_CALLBACK_PRIORITIES; prio++) {
                if (ellCount(&pel->scan_list[prio].list) == 0) continue;
                sprintf(message, "Event \"%s\" Priority %s", pel->event_name, priorityName[prio]);
                printList(&pel->scan_list[prio], message);
            }
        }
    }
    return 0;
}

int scanpiol(void)                  /* print pioscan_list */
{
    ioscan_head *piosh;

    ioscanInit();
    epicsMutexMustLock(ioscan_lock);
    piosh = pioscan_list;

    while (piosh) {
        int prio;

        for (prio = 0; prio < NUM_CALLBACK_PRIORITIES; prio++) {
            io_scan_list *piosl = &piosh->iosl[prio];
            char message[80];

            sprintf(message, "IO Event %p: Priority %s",
                piosh, priorityName[prio]);
            printList(&piosl->scan_list, message);
        }
        piosh = piosh->next;
    }
    epicsMutexUnlock(ioscan_lock);
    return 0;
}

static void eventCallback(CALLBACK *pcallback)
{
    scan_list *psl;

    callbackGetUser(psl, pcallback);
    scanList(psl);
}

static void eventOnce(void *arg)
{
    event_lock = epicsMutexMustCreate();
}

event_list *eventNameToHandle(const char *eventname)
{
    int prio;
    event_list *pel;
    static epicsThreadOnceId onceId = EPICS_THREAD_ONCE_INIT;

    if (!eventname || eventname[0] == 0)
        return NULL;

    epicsThreadOnce(&onceId, eventOnce, NULL);
    epicsMutexMustLock(event_lock);
    for (pel = pevent_list[0]; pel; pel=pel->next) {
        if (strcmp(pel->event_name, eventname) == 0) break;
    }
    if (pel == NULL) {
        pel = calloc(1, sizeof(event_list));
        if (!pel)
            goto done;
        strcpy(pel->event_name, eventname);
        for (prio = 0; prio < NUM_CALLBACK_PRIORITIES; prio++) {
            callbackSetUser(&pel->scan_list[prio], &pel->callback[prio]);
            callbackSetPriority(prio, &pel->callback[prio]);
            callbackSetCallback(eventCallback, &pel->callback[prio]);
            pel->scan_list[prio].lock = epicsMutexMustCreate();
            ellInit(&pel->scan_list[prio].list);
        }
        pel->next=pevent_list[0];
        pevent_list[0]=pel;
        { /* backward compatibility */
            char* p;
            long e = strtol(eventname, &p, 0);
            if (*p == 0 && e > 0 && e <= 255)
                pevent_list[e] = pel;
        }
    }
done:
    epicsMutexUnlock(event_lock);
    return pel;
}

void postEvent(event_list *pel)
{
    int prio;

    if (scanCtl != ctlRun) return;
    if (!pel) return;
    for (prio = 0; prio < NUM_CALLBACK_PRIORITIES; prio++) {
        if (ellCount(&pel->scan_list[prio].list) >0)
            callbackRequest(&pel->callback[prio]);
    }
}

/* backward compatibility */
void post_event(int event)
{
    event_list* pel;

    if (event <= 0 || event > 255) return;
    do { /* multithreading: make sure pel is consistent */
        pel = pevent_list[event];
    } while (pel != pevent_list[event]);
    postEvent(pel);
}

static void ioscanOnce(void *arg)
{
    ioscan_lock = epicsMutexMustCreate();
}

static void ioscanInit(void)
{
    static epicsThreadOnceId onceId = EPICS_THREAD_ONCE_INIT;

    epicsThreadOnce(&onceId, ioscanOnce, NULL);
}

static void ioscanDestroy(void)
{
    ioscan_head *piosh;

    ioscanInit();
    epicsMutexMustLock(ioscan_lock);
    piosh = pioscan_list;
    pioscan_list = NULL;
    epicsMutexUnlock(ioscan_lock);
    while (piosh) {
        ioscan_head *pnext = piosh->next;
        int prio;

        for (prio = 0; prio < NUM_CALLBACK_PRIORITIES; prio++) {
            epicsMutexDestroy(piosh->iosl[prio].scan_list.lock);
            ellFree(&piosh->iosl[prio].scan_list.list);
        }
        free(piosh);
        piosh = pnext;
    }
}

void scanIoInit(IOSCANPVT *pioscanpvt)
{
    ioscan_head *piosh = dbCalloc(1, sizeof(ioscan_head));
    int prio;

    ioscanInit();
    for (prio = 0; prio < NUM_CALLBACK_PRIORITIES; prio++) {
        io_scan_list *piosl = &piosh->iosl[prio];

        callbackSetCallback(ioscanCallback, &piosl->callback);
        callbackSetPriority(prio, &piosl->callback);
        callbackSetUser(piosh, &piosl->callback);
        ellInit(&piosl->scan_list.list);
        piosl->scan_list.lock = epicsMutexMustCreate();
    }
    epicsMutexMustLock(ioscan_lock);
    piosh->next = pioscan_list;
    pioscan_list = piosh;
    epicsMutexUnlock(ioscan_lock);
    *pioscanpvt = piosh;
}

/* Return a bit mask indicating each priority level
 * in which a callback request was successfully queued.
 */
unsigned int scanIoRequest(IOSCANPVT piosh)
{
    int prio;
    unsigned int queued = 0;

    if (scanCtl != ctlRun)
        return 0;

    for (prio = 0; prio < NUM_CALLBACK_PRIORITIES; prio++) {
        io_scan_list *piosl = &piosh->iosl[prio];

        if (ellCount(&piosl->scan_list.list) > 0)
            if (!callbackRequest(&piosl->callback))
                queued |= 1 << prio;
    }

    return queued;
}

unsigned int scanIoImmediate(IOSCANPVT piosh, int prio)
{
    io_scan_list *piosl;

    if (prio<0 || prio>=NUM_CALLBACK_PRIORITIES)
        return S_db_errArg;
    else if (scanCtl != ctlRun)
        return 0;

    piosl = &piosh->iosl[prio];

    if (ellCount(&piosl->scan_list.list) == 0)
        return 0;

    scanList(&piosl->scan_list);

    if (piosh->cb)
        piosh->cb(piosh->arg, piosh, prio);

    return 1 << prio;
}

/* May not be called while a scan request is queued or running */
void scanIoSetComplete(IOSCANPVT piosh, io_scan_complete cb, void *arg)
{
    piosh->cb = cb;
    piosh->arg = arg;
}

int scanOnce(struct dbCommon *precord) {
    return scanOnceCallback(precord, NULL, NULL);
}

typedef struct {
    struct dbCommon *prec;
    once_complete cb;
    void *usr;
} onceEntry;

int scanOnceCallback(struct dbCommon *precord, once_complete cb, void *usr)
{
    static int newOverflow = TRUE;
    onceEntry ent;
    int pushOK;

    ent.prec = precord;
    ent.cb = cb;
    ent.usr = usr;

    pushOK = epicsRingBytesPut(onceQ, (void*)&ent, sizeof(ent));

    if (!pushOK) {
        if (newOverflow) errlogPrintf("scanOnce: Ring buffer overflow\n");
        newOverflow = FALSE;
    } else {
        newOverflow = TRUE;
    }
    epicsEventSignal(onceSem);

    return !pushOK;
}

static void onceTask(void *arg)
{
    taskwdInsert(0, NULL, NULL);
    epicsEventSignal(startStopEvent);

    while (TRUE) {

        epicsEventMustWait(onceSem);
        while(1) {
            onceEntry ent;
            int bytes = epicsRingBytesGet(onceQ, (void*)&ent, sizeof(ent));
            if(bytes==0)
                break;
            if(bytes!=sizeof(ent)) {
                errlogPrintf("onceTask: received incomplete %d of %u\n",
                             bytes, (unsigned)sizeof(ent));
                continue; /* what to do? */
            } else if (ent.prec == (void*)&exitOnce) goto shutdown;

            dbScanLock(ent.prec);
            dbProcess(ent.prec);
            dbScanUnlock(ent.prec);
            if(ent.cb)
                ent.cb(ent.usr, ent.prec);
        }
    }

shutdown:
    taskwdRemove(0);
    epicsEventSignal(startStopEvent);
}

int scanOnceSetQueueSize(int size)
{
    onceQueueSize = size;
    return 0;
}

static void initOnce(void)
{
    if ((onceQ = epicsRingBytesLockedCreate(sizeof(onceEntry)*onceQueueSize)) == NULL) {
        cantProceed("initOnce: Ring buffer create failed\n");
    }
    if(!onceSem)
        onceSem = epicsEventMustCreate(epicsEventEmpty);
    onceTaskId = epicsThreadCreate("scanOnce",
        epicsThreadPriorityScanLow + nPeriodic,
        epicsThreadGetStackSize(epicsThreadStackBig), onceTask, 0);

    epicsEventWait(startStopEvent);
}

static void periodicTask(void *arg)
{
    periodic_scan_list *ppsl = (periodic_scan_list *)arg;
    epicsTimeStamp next, reported;
    unsigned int overruns = 0;
    double report_delay = OVERRUN_REPORT_DELAY;
    double overtime = 0.0;
    double over_min = 0.0;
    double over_max = 0.0;
    const double penalty = (ppsl->period >= 2) ? 1 : (ppsl->period / 2);

    taskwdInsert(0, NULL, NULL);
    epicsEventSignal(startStopEvent);

    epicsTimeGetCurrent(&next);
    reported = next;

    while (ppsl->scanCtl != ctlExit) {
        double delay;
        epicsTimeStamp now;

        if (ppsl->scanCtl == ctlRun)
            scanList(&ppsl->scan_list);

        epicsTimeAddSeconds(&next, ppsl->period);
        epicsTimeGetCurrent(&now);
        delay = epicsTimeDiffInSeconds(&next, &now);
        if (delay <= 0.0) {
            if (overtime == 0.0) {
                overtime = over_min = over_max = -delay;
            }
            else {
                overtime -= delay;
                if (over_min + delay > 0)
                    over_min = -delay;
                if (over_max + delay < 0)
                    over_max = -delay;
            }
            delay = penalty;
            ppsl->overruns++;
            next = now;
            epicsTimeAddSeconds(&next, delay);
            if (++overruns >= 10 &&
                epicsTimeDiffInSeconds(&now, &reported) > report_delay) {
                errlogPrintf("\ndbScan warning from '%s' scan thread:\n"
                    "\tScan processing averages %.3f seconds (%.3f .. %.3f).\n"
                    "\tOver-runs have now happened %u times in a row.\n"
                    "\tTo fix this, move some records to a slower scan rate.\n",
                    ppsl->name, ppsl->period + overtime / overruns,
                    ppsl->period + over_min, ppsl->period + over_max, overruns);

                reported = now;
                if (report_delay < (OVERRUN_REPORT_MAX / 2))
                    report_delay *= 2;
                else
                    report_delay = OVERRUN_REPORT_MAX;
            }
        }
        else {
            overruns = 0;
            report_delay = OVERRUN_REPORT_DELAY;
            overtime = 0.0;
        }

        epicsEventWaitWithTimeout(ppsl->loopEvent, delay);
    }

    taskwdRemove(0);
    epicsEventSignal(startStopEvent);
}


static void initPeriodic(void)
{
    dbMenu *pmenu = dbFindMenu(pdbbase, "menuScan");
    double quantum = epicsThreadSleepQuantum();
    int i;

    if (!pmenu) {
        errlogPrintf("initPeriodic: menuScan not present\n");
        return;
    }
    nPeriodic = pmenu->nChoice - SCAN_1ST_PERIODIC;
    papPeriodic = dbCalloc(nPeriodic, sizeof(periodic_scan_list*));
    periodicTaskId = dbCalloc(nPeriodic, sizeof(void *));
    for (i = 0; i < nPeriodic; i++) {
        periodic_scan_list *ppsl = dbCalloc(1, sizeof(periodic_scan_list));
        const char *choice = pmenu->papChoiceValue[i + SCAN_1ST_PERIODIC];
        double number;
        char *unit;
        int status = epicsParseDouble(choice, &number, &unit);

        if (status || number <= 0) {
            errlogPrintf("initPeriodic: Bad menuScan choice '%s'\n", choice);
        }
        else if (!*unit ||
                 !epicsStrCaseCmp(unit, "second") ||
                 !epicsStrCaseCmp(unit, "seconds")) {
            ppsl->period = number;
        }
        else if (!epicsStrCaseCmp(unit, "minute") ||
                 !epicsStrCaseCmp(unit, "minutes")) {
            ppsl->period = number * 60;
        }
        else if (!epicsStrCaseCmp(unit, "hour") ||
                 !epicsStrCaseCmp(unit, "hours")) {
            ppsl->period = number * 60 * 60;
        }
        else if (!epicsStrCaseCmp(unit, "Hz") ||
                 !epicsStrCaseCmp(unit, "Hertz")) {
            ppsl->period = 1 / number;
        }
        else {
            errlogPrintf("initPeriodic: Bad menuScan choice '%s'\n", choice);
        }
        if (ppsl->period == 0) {
            free(ppsl);
            continue;
        }

        ppsl->scan_list.lock = epicsMutexMustCreate();
        ellInit(&ppsl->scan_list.list);
        ppsl->name = choice;
        ppsl->scanCtl = ctlPause;
        ppsl->loopEvent = epicsEventMustCreate(epicsEventEmpty);

        number = ppsl->period / quantum;
        if ((ppsl->period < 2 * quantum) ||
            (number / floor(number) > 1.1)) {
            errlogPrintf("initPeriodic: Scan rate '%s' is not achievable.\n",
                choice);
        }

        papPeriodic[i] = ppsl;
    }
}

static void deletePeriodic(void)
{
    int i;

    for (i = 0; i < nPeriodic; i++) {
        periodic_scan_list *ppsl = papPeriodic[i];

        if (!ppsl) continue;
        ellFree(&ppsl->scan_list.list);
        epicsEventDestroy(ppsl->loopEvent);
        epicsMutexDestroy(ppsl->scan_list.lock);
        free(ppsl);
    }

    free(papPeriodic);
    papPeriodic = NULL;
}

static void spawnPeriodic(int ind)
{
    periodic_scan_list *ppsl = papPeriodic[ind];
    char taskName[20];

    if (!ppsl) return;

    sprintf(taskName, "scan-%g", ppsl->period);
    periodicTaskId[ind] = epicsThreadCreate(
        taskName, epicsThreadPriorityScanLow + ind,
        epicsThreadGetStackSize(epicsThreadStackBig),
        periodicTask, (void *)ppsl);

    epicsEventWait(startStopEvent);
}

static void ioscanCallback(CALLBACK *pcallback)
{
    ioscan_head *piosh;
    int prio;

    callbackGetUser(piosh, pcallback);
    callbackGetPriority(prio, pcallback);
    scanList(&piosh->iosl[prio].scan_list);
    if (piosh->cb)
        piosh->cb(piosh->arg, piosh, prio);
}

static void printList(scan_list *psl, char *message)
{
    scan_element *pse;

    epicsMutexMustLock(psl->lock);
    pse = (scan_element *)ellFirst(&psl->list);
    epicsMutexUnlock(psl->lock);

    if (!pse)
        return;

    printf("%s\n", message);
    while (pse) {
        printf("    %-28s\n", pse->precord->name);
        epicsMutexMustLock(psl->lock);
        if (pse->pscan_list != psl) {
            epicsMutexUnlock(psl->lock);
            printf("    Scan list changed while printing, try again.\n");
            return;
        }
        pse = (scan_element *)ellNext(&pse->node);
        epicsMutexUnlock(psl->lock);
    }
}

static void scanList(scan_list *psl)
{
    /* When reading this code remember that the call to dbProcess can result
     * in the SCAN field being changed in an arbitrary number of records.
     */

    scan_element *pse;
    scan_element *prev = NULL;
    scan_element *next = NULL;

    epicsMutexMustLock(psl->lock);
    psl->modified = FALSE;
    pse = (scan_element *)ellFirst(&psl->list);
    if (pse) next = (scan_element *)ellNext(&pse->node);
    epicsMutexUnlock(psl->lock);

    while (pse) {
        struct dbCommon *precord = pse->precord;

        dbScanLock(precord);
        dbProcess(precord);
        dbScanUnlock(precord);

        epicsMutexMustLock(psl->lock);
        if (!psl->modified) {
            prev = pse;
            pse = (scan_element *)ellNext(&pse->node);
            if (pse) next = (scan_element *)ellNext(&pse->node);
        } else if (pse->pscan_list == psl) {
            /*This scan element is still in same scan list*/
            prev = pse;
            pse = (scan_element *)ellNext(&pse->node);
            if (pse) next = (scan_element *)ellNext(&pse->node);
            psl->modified = FALSE;
        } else if (prev && prev->pscan_list == psl) {
            /*Previous scan element is still in same scan list*/
            pse = (scan_element *)ellNext(&prev->node);
            if (pse) {
                prev = (scan_element *)ellPrevious(&pse->node);
                next = (scan_element *)ellNext(&pse->node);
            }
            psl->modified = FALSE;
        } else if (next && next->pscan_list == psl) {
            /*Next scan element is still in same scan list*/
            pse = next;
            prev = (scan_element *)ellPrevious(&pse->node);
            next = (scan_element *)ellNext(&pse->node);
            psl->modified = FALSE;
        } else {
            /*Too many changes. Just wait till next period*/
            epicsMutexUnlock(psl->lock);
            return;
        }
        epicsMutexUnlock(psl->lock);
    }
}

static void buildScanLists(void)
{
    dbRecordType *pdbRecordType;

    for (pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
         pdbRecordType;
         pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
        dbRecordNode *pdbRecordNode;

        for (pdbRecordNode = (dbRecordNode *)ellFirst(&pdbRecordType->recList);
             pdbRecordNode;
             pdbRecordNode = (dbRecordNode *)ellNext(&pdbRecordNode->node)) {
            dbCommon *precord = pdbRecordNode->precord;

            if (!precord->name[0] ||
                pdbRecordNode->flags & DBRN_FLAGS_ISALIAS)
                continue;

            scanAdd(precord);
        }
    }
}

static void addToList(struct dbCommon *precord, scan_list *psl)
{
    scan_element *pse, *ptemp;

    epicsMutexMustLock(psl->lock);
    pse = precord->spvt;
    if (pse == NULL) {
        pse = dbCalloc(1, sizeof(scan_element));
        precord->spvt = pse;
        pse->precord = precord;
    }
    pse->pscan_list = psl;
    ptemp = (scan_element *)ellLast(&psl->list);
    while (ptemp) {
        if (ptemp->precord->phas <= precord->phas) {
            ellInsert(&psl->list, &ptemp->node, &pse->node);
            break;
        }
        ptemp = (scan_element *)ellPrevious(&ptemp->node);
    }
    if (ptemp == NULL) ellAdd(&psl->list, (void *)pse);
    psl->modified = TRUE;
    epicsMutexUnlock(psl->lock);
}

static void deleteFromList(struct dbCommon *precord, scan_list *psl)
{
    scan_element *pse;

    epicsMutexMustLock(psl->lock);
    pse = precord->spvt;
    if (pse == NULL) {
        epicsMutexUnlock(psl->lock);
        errlogPrintf("dbScan: Tried to delete record from wrong scan list!\n"
            "\t%s.SPVT = NULL, but psl = %p\n",
            precord->name, (void *)psl);
        return;
    }
    if (pse->pscan_list != psl) {
        epicsMutexUnlock(psl->lock);
        errlogPrintf("dbScan: Tried to delete record from wrong scan list!\n"
            "\t%s.SPVT->pscan_list = %p but psl = %p\n",
            precord->name, (void *)pse, (void *)psl);
        return;
    }
    pse->pscan_list = NULL;
    ellDelete(&psl->list, (void *)pse);
    psl->modified = TRUE;
    epicsMutexUnlock(psl->lock);
}
