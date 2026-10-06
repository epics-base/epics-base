/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Copyright (c) 2013 Helmholtz-Zentrum Berlin
*     für Materialien und Energie GmbH.
* SPDX-License-Identifier: EPICS
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
#include "epicsAtomic.h"
#include "epicsEvent.h"
#include "epicsGeneralTime.h"
#include "epicsMutex.h"
#include "epicsPrint.h"
#include "epicsRingBytes.h"
#include "epicsStdio.h"
#include "epicsStdlib.h"
#include "epicsString.h"
#include "epicsThread.h"
#include "epicsTime.h"
#include "taskwd.h"

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
#include "epicsExport.h"
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
static int onceQOverruns = 0;
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

/* A rate that helpers serve works on a snapshot of its scan list, taken
 * in PHAS order at the start of the pass; a rate without helpers walks
 * its list with scanList() as it always has. The leader (the period's
 * own thread) publishes one PHAS group at a time and takes slots from
 * it like any helper; a slot is claimed by a CAS on `cursor`, and
 * claims stop at `limit`. Both words carry the pass generation in
 * their high bits so a stale limit from the previous pass can never be
 * paired with the new cursor. Within a pass the limit only grows, group
 * by group, after `outstanding` for the previous group has reached
 * zero, which is what keeps a higher PHAS from starting before a lower
 * one has finished.
 */
typedef struct scan_slot {
    struct dbCommon     *prec;
    short               phas;
} scan_slot;

#define SP_IDX_BITS   (sizeof(size_t) > 4 ? 32 : 20)
#define SP_IDX_MASK   (((size_t)1 << SP_IDX_BITS) - 1)
#define SP_PACK(g, i) (((size_t)(g) << SP_IDX_BITS) | (size_t)(i))
#define SP_IDX(w)     ((w) & SP_IDX_MASK)
#define SP_GEN(w)     ((w) >> SP_IDX_BITS)
#define SP_NONE       ((size_t)-1)

typedef struct periodic_scan_list {
    scan_list           scan_list;
    double              period;
    const char          *name;
    unsigned long       overruns;
    volatile enum ctl   scanCtl;
    epicsEventId        loopEvent;
    int                 scan;        /* menuScan value of this list */
    unsigned            prio;        /* thread priority of the leader */
    scan_slot           *snap;       /* snapshot of scan_list, PHAS order */
    size_t              snapCap;
    size_t              snapLen;
    scan_slot           *snapNext;   /* larger buffer queued by addToList */
    size_t              snapNextCap;
    size_t              cursor;      /* atomic SP_PACK(gen, next slot) */
    size_t              limit;       /* atomic SP_PACK(gen, end of group) */
    int                 outstanding; /* atomic: slots of the group not done */
    epicsEventId        doneEvent;   /* outstanding reached zero */
} periodic_scan_list;

static int nPeriodic = 0;
static periodic_scan_list **papPeriodic; /* pointer to array of pointers */
static epicsThreadId *periodicTaskId;    /* array of thread ids */

/* Helpers are a pool shared by every period. A leader that publishes a
 * group sets its bit in helpWanted and wakes as many sleeping helpers
 * as the group has further slots. A helper serves the fastest period
 * wanting help, at that period's leader priority, one slot at a time,
 * and looks for a faster period after each one. A helper announces
 * itself in helperSleepers before its last look for work, and a leader
 * publishes before it looks at helperSleepers, so one of them sees the
 * other. Leaders never depend on helpers for progress.
 *
 * At most slowCap pool helpers run slots of a rate other than the
 * fastest one with records at the same time, so a short pass of the
 * fastest rate never finds every helper inside a long record of a
 * slower rate. Any helper may take the slow work; which ones stay
 * free varies.
 *
 * A helper configured with scanRateThreads() serves one period only,
 * at that period's priority, so the period's passes always get the
 * same workers: the deterministic choice for an RT IOC. Both kinds
 * may exist at once.
 */
typedef struct scan_helper {
    epicsEventId        wake;
    epicsThreadId       tid;        /* written by the spawner, read at stop */
    unsigned            idx;
    size_t              serves;     /* bitmask of period indices */
} scan_helper;

#define SP_MAX_HELPERS (8 * sizeof(size_t))
#define SP_MAX_PERIODS SP_MAX_HELPERS
#define SP_ALL (~(size_t)0)
int scanParallelThreadsDefault = 8;
epicsExportAddress(int, scanParallelThreadsDefault);
static int nHelpersConfigured = 0;
static int nReserveConfigured = 0;
static int nRateConfigured[SP_MAX_PERIODS];  /* dedicated, per period */
static int nHelpers = 0;
static scan_helper *helpers;
static size_t periodHelpers[SP_MAX_PERIODS]; /* helper indices serving it */
static int nDedicated[SP_MAX_PERIODS];       /* serving only this period */
static int fastPeriod = -1;      /* period index kept a free helper */
static int slowCap;              /* pool helpers allowed on other periods */
static int slowBusy;             /* atomic: pool helpers on other periods */
static size_t helpWanted;        /* atomic bitmask of period indices */
static size_t helperSleepers;    /* atomic bitmask of helper indices */
static int helperShutdown;       /* atomic */


static char *priorityName[NUM_CALLBACK_PRIORITIES] = {
    "Low", "Medium", "High"
};


/* EVENT */

typedef struct event_list {
    epicsCallback            callback[NUM_CALLBACK_PRIORITIES];
    scan_list           scan_list[NUM_CALLBACK_PRIORITIES];
    struct event_list   *next;
    char                eventname[1]; /* actually arbitrary size */
} event_list;
/* All event_list are singly linked from pevent_list[0] via next.
 * Numbered events are also stored in pevent_list[1..255].
 */
static event_list * volatile pevent_list[NUM_TIME_EVENTS];
static epicsMutexId event_lock;

/* IO_EVENT*/

typedef struct io_scan_list {
    epicsCallback callback;
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
static void periodicPass(periodic_scan_list *ppsl);
static void helperTask(void *arg);
static void spawnHelpers(void);
static void stopHelpers(void);
static void eventCallback(epicsCallback *pcallback);
static void ioscanInit(void);
static void ioscanCallback(epicsCallback *pcallback);
static void ioscanDestroy(void);
static void printList(scan_list *psl, char *message);
static void scanList(scan_list *psl);
static void buildScanLists(void);
static void addToList(struct dbCommon *precord, scan_list *psl);
static void deleteFromList(struct dbCommon *precord, scan_list *psl);

void scanStop(void)
{
    int i;

    if (scanCtl == ctlInit || scanCtl == ctlExit) return;
    scanCtl = ctlExit;

    interruptAccept = FALSE;

    for (i = 0; i < nPeriodic; i++) {
        periodic_scan_list *ppsl = papPeriodic[i];

        if (!ppsl) continue;
        ppsl->scanCtl = ctlExit;
        epicsEventSignal(ppsl->loopEvent);
        epicsEventWait(startStopEvent);
    }
    for (i = 0; i < nPeriodic; i++) {
        epicsThreadMustJoin(periodicTaskId[i]);
    }
    stopHelpers();

    scanOnce((dbCommon *)&exitOnce);
    epicsEventWait(startStopEvent);
    epicsThreadMustJoin(onceTaskId);
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
    spawnHelpers();
    for (i = 0; i < nPeriodic; i++)
        spawnPeriodic(i);

    return 0;
}

int scanParallelThreads(int count, int reserve)
{
    if (papPeriodic) {
        fprintf(stderr, "scanParallelThreads: scan system already initialized\n");
        return -1;
    }
    if (count < 0)
        count = epicsThreadGetCPUs() + count;
    else if (count == 0) {
        /* the default leaves one CPU to the leaders; on a single CPU a
         * helper could only take turns with them */
        int cpus = epicsThreadGetCPUs() - 1;

        count = scanParallelThreadsDefault;
        if (count > cpus)
            count = cpus;
    }
    if (count < 0) count = 0;
    if (count > (int)SP_MAX_HELPERS) {
        fprintf(stderr, "scanParallelThreads: clamping %d to %d\n",
            count, (int)SP_MAX_HELPERS);
        count = SP_MAX_HELPERS;
    }
    if (reserve == 0)
        reserve = 1;
    else if (reserve < 0)
        reserve = 0;
    if (reserve > count) {
        fprintf(stderr, "scanParallelThreads: clamping reserve %d to %d\n",
            reserve, count);
        reserve = count;
    }
    nHelpersConfigured = count;
    nReserveConfigured = reserve;
    return 0;
}

int scanRateThreads(const char *rate, int count)
{
    dbMenu *pmenu;
    int i;

    if (papPeriodic) {
        fprintf(stderr, "scanRateThreads: scan system already initialized\n");
        return -1;
    }
    pmenu = pdbbase ? dbFindMenu(pdbbase, "menuScan") : NULL;
    if (!pmenu) {
        fprintf(stderr, "scanRateThreads: menuScan not present, load a .dbd first\n");
        return -1;
    }
    for (i = SCAN_1ST_PERIODIC; i < pmenu->nChoice; i++) {
        if (rate && strcmp(rate, pmenu->papChoiceValue[i]) == 0)
            break;
    }
    if (i >= pmenu->nChoice || i - SCAN_1ST_PERIODIC >= (int)SP_MAX_PERIODS) {
        fprintf(stderr, "scanRateThreads: '%s' is not a periodic SCAN rate\n",
            rate ? rate : "");
        return -1;
    }
    if (count < 0) count = 0;
    nRateConfigured[i - SCAN_1ST_PERIODIC] = count;
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
        recGblRecordError(-1, precord,
            "scanAdd detected illegal SCAN value");
    } else if (scan == menuScanEvent) {
        char* eventname;
        int prio;
        event_list *pel;

        eventname = precord->evnt;
        prio = precord->prio;
        if (prio < 0 || prio >= NUM_CALLBACK_PRIORITIES) {
            recGblRecordError(-1, precord,
                "scanAdd: illegal prio field");
            return;
        }
        pel = eventNameToHandle(eventname);
        if (pel) addToList(precord, &pel->scan_list[prio]);
    } else if (scan == menuScanI_O_Intr) {
        ioscan_head *piosh = NULL;
        int prio;
        long (*get_ioint_info)(int, struct dbCommon *, IOSCANPVT*);

        if (precord->dset == NULL){
            recGblRecordError(-1, precord,
                "scanAdd: I/O Intr not valid (no DSET) ");
            precord->scan = menuScanPassive;
            return;
        }
        get_ioint_info = precord->dset->get_ioint_info;
        if (get_ioint_info == NULL) {
            recGblRecordError(-1, precord,
                "scanAdd: I/O Intr not valid (no get_ioint_info)");
            precord->scan = menuScanPassive;
            return;
        }
        if (get_ioint_info(0, precord, &piosh)) {
            precord->scan = menuScanPassive;
            return;
        }
        if (piosh == NULL) {
            recGblRecordError(-1, precord,
                "scanAdd: I/O Intr not valid");
            precord->scan = menuScanPassive;
            return;
        }
        prio = precord->prio;
        if (prio < 0 || prio >= NUM_CALLBACK_PRIORITIES) {
            recGblRecordError(-1, precord,
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
        recGblRecordError(-1, precord,
            "scanDelete detected illegal SCAN value");
    } else if (scan == menuScanEvent) {
        int prio;
        event_list *pel;
        scan_list *psl = 0;

        prio = precord->prio;
        if (prio < 0 || prio >= NUM_CALLBACK_PRIORITIES) {
            recGblRecordError(-1, precord,
                "scanDelete detected illegal PRIO field");
            return;
        }
        pel = eventNameToHandle(precord->evnt);
        if (pel && (psl = &pel->scan_list[prio]))
            deleteFromList(precord, psl);
    } else if (scan == menuScanI_O_Intr) {
        ioscan_head *piosh = NULL;
        int prio;
        long (*get_ioint_info)(int, struct dbCommon *, IOSCANPVT*);

        if (precord->dset==NULL) {
            recGblRecordError(-1, precord,
                "scanDelete: I/O Intr not valid (no DSET)");
            return;
        }
        get_ioint_info=precord->dset->get_ioint_info;
        if (get_ioint_info == NULL) {
            recGblRecordError(-1, precord,
                "scanDelete: I/O Intr not valid (no get_ioint_info)");
            return;
        }
        if (get_ioint_info(1, precord, &piosh)) return;
        if (piosh == NULL) {
            recGblRecordError(-1, precord,
                "scanDelete: I/O Intr not valid");
            return;
        }
        prio = precord->prio;
        if (prio < 0 || prio >= NUM_CALLBACK_PRIORITIES) {
            recGblRecordError(-1, precord,
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

    for (pel = pevent_list[0]; pel; pel = pel->next) {
        if (!eventname || epicsStrGlobMatch(pel->eventname, eventname)) {
            printf("Event \"%s\"\n", pel->eventname);
            for (prio = 0; prio < NUM_CALLBACK_PRIORITIES; prio++) {
                if (ellCount(&pel->scan_list[prio].list) == 0) continue;
                sprintf(message, " Priority %s", priorityName[prio]);
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

static void eventCallback(epicsCallback *pcallback)
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
    double eventnumber = 0;
    size_t namelength;

    if (!eventname) return NULL;
    while (isspace((int) eventname[0])) eventname++;
    if (!eventname[0]) return NULL;
    namelength = strlen(eventname);
    while (isspace((int) eventname[namelength-1])) namelength--;

    /* Backward compatibility with numeric events:
       Treat any string that represents a double with an
       integer part between 0 and 255 the same as the integer
       because it is most probably a conversion from double
       like from a calc record.
    */
    if (epicsParseDouble(eventname, &eventnumber, NULL) == 0)
    {
        if (eventnumber >= 0 && eventnumber < NUM_TIME_EVENTS)
        {
            if (eventnumber < 1)
                return NULL; /* 0 is no event */
            if ((pel = pevent_list[(int)eventnumber]) != NULL)
                return pel;
        }
        else
            eventnumber = 0; /* not a numeric event between 1 and 255 */
    }

    epicsThreadOnce(&onceId, eventOnce, NULL);
    epicsMutexMustLock(event_lock);
    for (pel = pevent_list[0]; pel; pel=pel->next) {
        if (strncmp(pel->eventname, eventname, namelength) == 0
            && pel->eventname[namelength] == 0)
            break;
    }
    if (pel == NULL) {
        pel = calloc(1, sizeof(event_list) + namelength);
        if (!pel)
            goto done;
        if (eventnumber > 0) {
            /* backward compatibility: make all numeric events look like integers */
            sprintf(pel->eventname, "%i", (int)eventnumber);
            pevent_list[(int)eventnumber] = pel;
        }
        else
            strncpy(pel->eventname, eventname, namelength);
        for (prio = 0; prio < NUM_CALLBACK_PRIORITIES; prio++) {
            callbackSetUser(&pel->scan_list[prio], &pel->callback[prio]);
            callbackSetPriority(prio, &pel->callback[prio]);
            callbackSetCallback(eventCallback, &pel->callback[prio]);
            pel->scan_list[prio].lock = epicsMutexMustCreate();
            ellInit(&pel->scan_list[prio].list);
        }
        pel->next=pevent_list[0];
        pevent_list[0]=pel;
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
    if (event <= 0 || event >= NUM_TIME_EVENTS) return;
    postEvent(pevent_list[event]);
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
        if (newOverflow)
            errlogPrintf("%s : " ERL_WARNING " scanOnce: Ring buffer overflow\n",
                         precord->name);
        newOverflow = FALSE;
        epicsAtomicIncrIntT(&onceQOverruns);
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

int scanOnceQueueStatus(const int reset, scanOnceQueueStats *result)
{
    int ret;
    if (!onceQ) return -1;
    if (result) {
        result->size = epicsRingBytesSize(onceQ) / sizeof(onceEntry);
        result->numUsed = epicsRingBytesUsedBytes(onceQ) / sizeof(onceEntry);
        result->maxUsed = epicsRingBytesHighWaterMark(onceQ) / sizeof(onceEntry);
        result->numOverflow = epicsAtomicGetIntT(&onceQOverruns);
        ret = 0;
    } else {
        ret = -2;
    }
    if (reset) {
        epicsRingBytesResetHighWaterMark(onceQ);
    }
    return ret;
}

void scanOnceQueueShow(const int reset)
{
    scanOnceQueueStats stats;
    if (scanOnceQueueStatus(reset, &stats) == -1) {
        fprintf(stderr, "scanOnce system not initialized, yet. Please run "
            "iocInit before using this command.\n");
    } else {
        double qusage = 100.0 * stats.numUsed / stats.size;
        printf("PRIORITY  HIGH-WATER MARK  ITEMS IN Q  Q SIZE  %% USED  Q OVERFLOWS\n");
        printf("%8s  %15d  %10d  %6d  %6.1f  %11d\n", "scanOnce", stats.maxUsed,
               stats.numUsed, stats.size, qusage,
               epicsAtomicGetIntT(&onceQOverruns));
    }
}

static void initOnce(void)
{
    epicsThreadOpts opts = EPICS_THREAD_OPTS_INIT;
    opts.joinable = 1;
    opts.priority = epicsThreadPriorityScanLow + nPeriodic;
    opts.stackSize = epicsThreadStackBig;
    if ((onceQ = epicsRingBytesLockedCreate(sizeof(onceEntry)*onceQueueSize)) == NULL) {
        cantProceed("initOnce: Ring buffer create failed\n");
    }
    if(!onceSem)
        onceSem = epicsEventMustCreate(epicsEventEmpty);
    onceTaskId = epicsThreadCreateOpt("scanOnce", onceTask, 0, &opts);

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

    epicsTimeGetMonotonic(&next);
    reported = next;

    while (ppsl->scanCtl != ctlExit) {
        double delay;
        epicsTimeStamp now;

        if (ppsl->scanCtl == ctlRun)
            periodicPass(ppsl);

        epicsTimeAddSeconds(&next, ppsl->period);
        epicsTimeGetMonotonic(&now);
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
                errlogPrintf("\ndbScan " ERL_WARNING " from '%s' scan thread:\n"
                    "\tScan processing averages %.3f seconds (%.3f .. %.3f).\n"
                    "\tOver-runs have now happened %u times in a row.\n"
                    "\tTo fix this, move some records to a slower scan rate%s\n",
                    ppsl->name, ppsl->period + overtime / overruns,
                    ppsl->period + over_min, ppsl->period + over_max, overruns,
                    nHelpers ?
                        ",\n\tor add helper threads with scanParallelThreads()"
                        " or scanRateThreads() before iocInit." :
                    epicsThreadGetCPUs() > 1 ?
                        ",\n\tor add helper threads with scanParallelThreads()"
                        " before iocInit." : ".");

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
        ppsl->scan = i + SCAN_1ST_PERIODIC;
        ppsl->prio = epicsThreadPriorityScanLow + i;
        ppsl->doneEvent = epicsEventMustCreate(epicsEventEmpty);

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
        epicsEventDestroy(ppsl->doneEvent);
        epicsMutexDestroy(ppsl->scan_list.lock);
        free(ppsl->snap);
        free(ppsl->snapNext);
        free(ppsl);
    }

    free(papPeriodic);
    papPeriodic = NULL;
}

static void spawnPeriodic(int ind)
{
    periodic_scan_list *ppsl = papPeriodic[ind];
    char taskName[20];
    epicsThreadOpts opts = EPICS_THREAD_OPTS_INIT;
    opts.joinable = 1;
    opts.stackSize = epicsThreadStackBig;

    if (!ppsl) return;
    opts.priority = ppsl->prio;

    sprintf(taskName, "scan-%g", ppsl->period);
    periodicTaskId[ind] = epicsThreadCreateOpt(
        taskName, periodicTask, ppsl, &opts);

    epicsEventWait(startStopEvent);
}

/* atomic bitmask helpers */
static void maskSet(size_t *pmask, size_t bits)
{
    size_t m = epicsAtomicGetSizeT(pmask);
    for (;;) {
        size_t cur = epicsAtomicCmpAndSwapSizeT(pmask, m, m | bits);
        if (cur == m) return;
        m = cur;
    }
}

/* clear bits; return the bits that were set */
static size_t maskClear(size_t *pmask, size_t bits)
{
    size_t m = epicsAtomicGetSizeT(pmask);
    for (;;) {
        size_t cur = epicsAtomicCmpAndSwapSizeT(pmask, m, m & ~bits);
        if (cur == m) return m & bits;
        m = cur;
    }
}

static unsigned lowBit(size_t m)
{
    unsigned i = 0;
    while (!(m & 1)) { m >>= 1; i++; }
    return i;
}

/* Claim the next slot of the published group, or SP_NONE. A cursor and
 * a limit from different generations mean the leader is between the
 * two stores of a publish; there is no work to take from it yet. */
static size_t claimSlot(periodic_scan_list *ppsl)
{
    size_t c = epicsAtomicGetSizeT(&ppsl->cursor);
    for (;;) {
        size_t l = epicsAtomicGetSizeT(&ppsl->limit);
        size_t cur;

        if (SP_GEN(c) != SP_GEN(l) || SP_IDX(c) >= SP_IDX(l))
            return SP_NONE;
        cur = epicsAtomicCmpAndSwapSizeT(&ppsl->cursor, c, c + 1);
        if (cur == c)
            return SP_IDX(c);
        c = cur;
    }
}

/* Run slots of the published group until none is left or until a
 * faster period the caller serves wants help (serves is 0 for the
 * leader). Finished slots are subtracted from outstanding once, on
 * the way out. */
static void runSlots(periodic_scan_list *ppsl, size_t serves)
{
    size_t faster = serves &
        ~(((size_t)2 << (ppsl->scan - SCAN_1ST_PERIODIC)) - 1);
    size_t slot;
    int n = 0;

    while ((slot = claimSlot(ppsl)) != SP_NONE) {
        struct dbCommon *prec = ppsl->snap[slot].prec;

        /* SCAN may have changed since the snapshot, even by the
         * record's own processing: only process it while it is still
         * on this list, as the list walk would have. */
        dbScanLock(prec);
        if (prec->scan == ppsl->scan)
            dbProcess(prec);
        dbScanUnlock(prec);
        n++;
        if (faster && (epicsAtomicGetSizeT(&helpWanted) & faster))
            break;
    }
    if (n && epicsAtomicAddIntT(&ppsl->outstanding, -n) == 0)
        epicsEventSignal(ppsl->doneEvent);
}

static void wakeHelpers(size_t want, size_t eligible)
{
    while (want) {
        size_t m = epicsAtomicGetSizeT(&helperSleepers) & eligible;
        size_t bit;

        if (!m) return;
        bit = (size_t)1 << lowBit(m);
        if (!maskClear(&helperSleepers, bit)) continue;
        epicsEventSignal(helpers[lowBit(bit)].wake);
        want--;
    }
}

/* Copy the list into the snapshot under its lock. The leader never
 * allocates: when the list outgrew the buffer, addToList() queued a
 * larger one, which is taken over here. Returns non-zero if the list
 * still does not fit, i.e. records were added faster than the passes
 * came; the leader then walks the list itself this once. */
static int snapshotList(periodic_scan_list *ppsl)
{
    scan_list *psl = &ppsl->scan_list;
    scan_element *pse;
    size_t n, i = 0;

    epicsMutexMustLock(psl->lock);
    if (ppsl->snapNext) {
        free(ppsl->snap);
        ppsl->snap = ppsl->snapNext;
        ppsl->snapCap = ppsl->snapNextCap;
        ppsl->snapNext = NULL;
    }
    n = ellCount(&psl->list);
    if (n > ppsl->snapCap || n > SP_IDX_MASK) {
        epicsMutexUnlock(psl->lock);
        return -1;
    }
    for (pse = (scan_element *)ellFirst(&psl->list); pse;
         pse = (scan_element *)ellNext(&pse->node)) {
        ppsl->snap[i].prec = pse->precord;
        ppsl->snap[i].phas = pse->precord->phas;
        i++;
    }
    ppsl->snapLen = i;
    epicsMutexUnlock(psl->lock);
    return 0;
}

 /* One pass over the period's records. A rate no helper serves walks
 * its live list with scanList(), unchanged. With helpers the pass works
 * on a snapshot, PHAS group by PHAS group, the leader taking slots like
 * a helper; a record added to the list during the pass waits for the
 * next one. */
static void periodicPass(periodic_scan_list *ppsl)
{
    size_t gen, start = 0;
    int ind = ppsl->scan - SCAN_1ST_PERIODIC;
    size_t mybit = (size_t)1 << ind;

    if (!nHelpers || !periodHelpers[ind] || snapshotList(ppsl)) {
        scanList(&ppsl->scan_list);
        return;
    }
    if (ppsl->snapLen == 0)
        return;
    gen = (SP_GEN(epicsAtomicGetSizeT(&ppsl->cursor)) + 1) & SP_GEN(SP_NONE);

    while (start < ppsl->snapLen) {
        size_t end = start + 1;

        while (end < ppsl->snapLen &&
               ppsl->snap[end].phas == ppsl->snap[start].phas)
            end++;

        /* outstanding before the words a claim needs, cursor last on
         * the first group: a claim that succeeds has seen them all */
        epicsAtomicSetIntT(&ppsl->outstanding, (int)(end - start));
        epicsAtomicSetSizeT(&ppsl->limit, SP_PACK(gen, end));
        if (start == 0)
            epicsAtomicSetSizeT(&ppsl->cursor, SP_PACK(gen, 0));
        if (end - start > 1) {
            size_t want = end - start - 1;

            if (ind != fastPeriod) {
                /* the dedicated ones, plus as many pool helpers as
                 * may still take slow work */
                int room = slowCap - epicsAtomicGetIntT(&slowBusy);

                if (room < 0)
                    room = 0;
                room += nDedicated[ind];
                if (want > (size_t)room)
                    want = room;
            }
            maskSet(&helpWanted, mybit);
            wakeHelpers(want, periodHelpers[ind]);
        }

        runSlots(ppsl, 0);
        while (epicsAtomicGetIntT(&ppsl->outstanding) != 0)
            epicsEventWait(ppsl->doneEvent);
        maskClear(&helpWanted, mybit);
        start = end;
    }
}

static void helperTask(void *arg)
{
    scan_helper *me = (scan_helper *)arg;
    size_t mybit = (size_t)1 << me->idx;
    int pool = me->serves == SP_ALL;
    /* a helper sleeps at the priority it was created with: a pool
     * helper at the fastest rate's, so that on a machine with every
     * CPU busy it still preempts a slower leader the moment it is
     * woken, and lowers itself once it knows which rate it serves */
    unsigned sleepPrio = epicsThreadGetPrioritySelf();
    unsigned prio = sleepPrio;

    taskwdInsert(0, NULL, NULL);
    epicsEventSignal(startStopEvent);

    while (!epicsAtomicGetIntT(&helperShutdown)) {
        size_t wanted = epicsAtomicGetSizeT(&helpWanted) & me->serves;
        int announced = 0;
        int i;

        /* fastest period first; a second look after announcing sleep */
        for (;;) {
            for (i = nPeriodic - 1; i >= 0; i--) {
                periodic_scan_list *ppsl;

                if (!(wanted & ((size_t)1 << i))) continue;
                ppsl = papPeriodic[i];
                if (SP_IDX(epicsAtomicGetSizeT(&ppsl->cursor)) >=
                    SP_IDX(epicsAtomicGetSizeT(&ppsl->limit)))
                    continue;
                if (pool && i != fastPeriod &&
                    epicsAtomicIncrIntT(&slowBusy) > slowCap) {
                    /* enough pool helpers on slow work; stay free */
                    epicsAtomicDecrIntT(&slowBusy);
                    continue;
                }
                if (announced)
                    maskClear(&helperSleepers, mybit);
                if (prio != ppsl->prio) {
                    prio = ppsl->prio;
                    epicsThreadSetPriority(epicsThreadGetIdSelf(), prio);
                }
                runSlots(ppsl, me->serves);
                if (pool && i != fastPeriod)
                    epicsAtomicDecrIntT(&slowBusy);
                goto next;
            }
            if (announced) break;
            maskSet(&helperSleepers, mybit);
            announced = 1;
            wanted = epicsAtomicGetSizeT(&helpWanted) & me->serves;
        }
        if (prio != sleepPrio) {
            prio = sleepPrio;
            epicsThreadSetPriority(epicsThreadGetIdSelf(), prio);
        }
        epicsEventMustWait(me->wake);
next:   ;
    }

    taskwdRemove(0);
}

static void spawnHelpers(void)
{
    int i, p, nPool = nHelpersConfigured, nRate = 0;

    for (p = 0; p < nPeriodic && p < (int)SP_MAX_PERIODS; p++)
        nRate += nRateConfigured[p];
    nHelpers = nPool + nRate;
    if (!nHelpers) return;
    if (nPeriodic > (int)SP_MAX_PERIODS) {
        errlogPrintf("scanParallelThreads: %d scan rates exceed the %d the "
            "helper pool can serve, running without helpers\n",
            nPeriodic, (int)SP_MAX_PERIODS);
        nHelpers = 0;
        return;
    }
    if (nHelpers > (int)SP_MAX_HELPERS) {
        errlogPrintf("scanParallelThreads: %d helpers exceed %d, "
            "dropping pool helpers\n", nHelpers, (int)SP_MAX_HELPERS);
        nPool = SP_MAX_HELPERS > nRate ? (int)SP_MAX_HELPERS - nRate : 0;
        nHelpers = nPool + nRate;
        if (nHelpers > (int)SP_MAX_HELPERS) {
            nHelpers = 0;
            return;
        }
    }
    /* keep helpers free for the fastest rate with records, or for the
     * fastest rate at all when no list has any yet */
    fastPeriod = nPeriodic - 1;
    for (i = nPeriodic - 1; i >= 0; i--) {
        if (ellCount(&papPeriodic[i]->scan_list.list)) {
            fastPeriod = i;
            break;
        }
    }
    slowCap = nPool - nReserveConfigured;
    if (slowCap < 0) slowCap = 0;

    helpers = dbCalloc(nHelpers, sizeof(scan_helper));
    epicsAtomicSetIntT(&helperShutdown, 0);
    epicsAtomicSetIntT(&slowBusy, 0);
    epicsAtomicSetSizeT(&helpWanted, 0);
    epicsAtomicSetSizeT(&helperSleepers, 0);
    memset(periodHelpers, 0, sizeof periodHelpers);
    memset(nDedicated, 0, sizeof nDedicated);

    /* dedicated helpers first, so a leader waking by lowest index
     * reaches its own before the pool */
    for (i = 0, p = 0; p < nPeriodic; p++) {
        int k;

        nDedicated[p] = nRateConfigured[p];
        for (k = 0; k < nRateConfigured[p]; k++, i++) {
            helpers[i].serves = (size_t)1 << p;
            periodHelpers[p] |= (size_t)1 << i;
        }
    }
    for (; i < nHelpers; i++) {
        helpers[i].serves = SP_ALL;
        for (p = 0; p < nPeriodic; p++)
            periodHelpers[p] |= (size_t)1 << i;
    }

    /* snapshot buffers for the rates the helpers serve, sized here with
     * room to grow so the leaders never allocate */
    for (p = 0; p < nPeriodic; p++) {
        periodic_scan_list *ppsl = papPeriodic[p];
        size_t n;

        if (!periodHelpers[p]) continue;
        n = ellCount(&ppsl->scan_list.list);
        free(ppsl->snap);
        ppsl->snapCap = n + n / 2 + 8;
        ppsl->snap = dbCalloc(ppsl->snapCap, sizeof(scan_slot));
    }

    for (i = 0; i < nHelpers; i++) {
        epicsThreadOpts opts = EPICS_THREAD_OPTS_INIT;
        char name[32];

        opts.joinable = 1;
        opts.priority = helpers[i].serves == SP_ALL ?
            papPeriodic[nPeriodic - 1]->prio :
            papPeriodic[lowBit(helpers[i].serves)]->prio;
        opts.stackSize = epicsThreadStackBig;
        helpers[i].idx = i;
        helpers[i].wake = epicsEventMustCreate(epicsEventEmpty);
        sprintf(name, "scanHelper%d", i);
        helpers[i].tid = epicsThreadCreateOpt(name, helperTask,
            &helpers[i], &opts);
        epicsEventWait(startStopEvent);
    }
}

/* called from scanStop after the leaders have exited, so no helper
 * holds a claim; a helper still awake sees the flag on its next look */
static void stopHelpers(void)
{
    int i;

    if (!nHelpers) return;
    epicsAtomicSetIntT(&helperShutdown, 1);
    for (i = 0; i < nHelpers; i++)
        epicsEventSignal(helpers[i].wake);
    for (i = 0; i < nHelpers; i++) {
        epicsThreadMustJoin(helpers[i].tid);
        epicsEventDestroy(helpers[i].wake);
    }
    free(helpers);
    helpers = NULL;
    nHelpers = 0;
    fastPeriod = -1;
}

static void ioscanCallback(epicsCallback *pcallback)
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

static periodic_scan_list *periodicOf(scan_list *psl)
{
    int i;

    for (i = 0; i < nPeriodic; i++) {
        if (papPeriodic[i] && &papPeriodic[i]->scan_list == psl)
            return papPeriodic[i];
    }
    return NULL;
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
        if (ptemp->precord->phas <= precord->phas) break;
        ptemp = (scan_element *)ellPrevious(&ptemp->node);
    }
    ellInsert(&psl->list, (ptemp ? &ptemp->node : NULL), &pse->node);
    psl->modified = TRUE;
    if (nHelpers) {
        /* a periodic list helpers serve: queue a larger snapshot buffer
         * here, on the caller's thread, for the leader's next pass */
        periodic_scan_list *ppsl = periodicOf(psl);
        size_t n = ellCount(&psl->list);

        if (ppsl && periodHelpers[ppsl->scan - SCAN_1ST_PERIODIC] &&
            n > ppsl->snapCap &&
            (!ppsl->snapNext || n > ppsl->snapNextCap)) {
            free(ppsl->snapNext);
            ppsl->snapNextCap = n + n / 2 + 8;
            ppsl->snapNext = dbCalloc(ppsl->snapNextCap, sizeof(scan_slot));
        }
    }
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
            precord->name, psl);
        return;
    }
    if (pse->pscan_list != psl) {
        epicsMutexUnlock(psl->lock);
        errlogPrintf("dbScan: Tried to delete record from wrong scan list!\n"
            "\t%s.SPVT->pscan_list = %p but psl = %p\n",
            precord->name, pse, psl);
        return;
    }
    pse->pscan_list = NULL;
    ellDelete(&psl->list, &pse->node);
    psl->modified = TRUE;
    epicsMutexUnlock(psl->lock);
}
