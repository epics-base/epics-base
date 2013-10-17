/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbScan.c */
/* tasks and subroutines to scan the database */
/*
 *      Original Author:        Bob Dalesio
 *      Current Author:         Marty Kraimer
 *      Date:                   07/18/91
 */

#include <epicsStdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include "epicsStdioRedirect.h"
#include "dbDefs.h"
#include "ellLib.h"
#include "taskwd.h"
#include "epicsMutex.h"
#include "epicsEvent.h"
#include "epicsExit.h"
#include "epicsInterrupt.h"
#include "epicsThread.h"
#include "epicsTime.h"
#include "cantProceed.h"
#include "epicsRingPointer.h"
#include "epicsPrint.h"
#include "dbBase.h"
#include "dbStaticLib.h"
#include "dbFldTypes.h"
#include "link.h"
#include "devSup.h"
#include "dbCommon.h"
#define epicsExportSharedSymbols
#include "dbAddr.h"
#include "callback.h"
#include "dbAccessDefs.h"
#include "dbLock.h"
#include "recGbl.h"
#include "dbScan.h"


/* Task Control */
enum ctl {ctlInit, ctlRun, ctlPause, ctlExit};

/* Task Startup/Shutdown Synchronization */
static epicsEventId startStopEvent;

static volatile enum ctl scanCtl;

/* SCAN ONCE */

static int onceQueueSize = 1000;
static epicsEventId onceSem;
static epicsRingPointerId onceQ;
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

#define MAX_EVENTS 256
typedef struct event_scan_list {
    CALLBACK            callback;
    scan_list           scan_list;
} event_scan_list;
static event_scan_list *pevent_list[NUM_CALLBACK_PRIORITIES][MAX_EVENTS];


/* IO_EVENT*/

typedef struct io_scan_list {
    CALLBACK            callback;
    scan_list           scan_list;
    struct io_scan_list *next;
} io_scan_list;

static io_scan_list *iosl_head[NUM_CALLBACK_PRIORITIES] = {
    NULL, NULL, NULL
};


/* Private routines */
static void onceTask(void *);
static void initOnce(void);
static void periodicTask(void *arg);
static void initPeriodic(void);
static void spawnPeriodic(int ind);
static void initEvent(void);
static void eventCallback(CALLBACK *pcallback);
static void ioeventCallback(CALLBACK *pcallback);
static void printList(scan_list *psl, char *message);
static void scanList(scan_list *psl);
static void buildScanLists(void);
static void addToList(struct dbCommon *precord, scan_list *psl);
static void deleteFromList(struct dbCommon *precord, scan_list *psl);

static void scanShutdown(void *arg)
{
    int i;

    interruptAccept = FALSE;

    for (i = 0; i < nPeriodic; i++) {
        papPeriodic[i]->scanCtl = ctlExit;
        epicsEventSignal(papPeriodic[i]->loopEvent);
        epicsEventWait(startStopEvent);
    }

    scanOnce((dbCommon *)&exitOnce);
    epicsEventWait(startStopEvent);
}

long scanInit(void)
{
    int i;

    startStopEvent = epicsEventMustCreate(epicsEventEmpty);
    scanCtl = ctlPause;

    initOnce();
    initPeriodic();
    initEvent();
    buildScanLists();
    for (i = 0; i < nPeriodic; i++)
        spawnPeriodic(i);

    epicsAtExit(scanShutdown, NULL);
    return 0;
}

void scanRun(void)
{
    int i;

    interruptAccept = TRUE;
    scanCtl = ctlRun;

    for (i = 0; i < nPeriodic; i++)
        papPeriodic[i]->scanCtl = ctlRun;
}

void scanPause(void)
{
    int i;

    for (i = nPeriodic - 1; i >= 0; --i)
        papPeriodic[i]->scanCtl = ctlPause;

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
        int evnt;
        int prio;
        event_scan_list *pesl;

        evnt = precord->evnt;
        if (evnt < 0 || evnt >= MAX_EVENTS) {
            recGblRecordError(S_db_badField, (void *)precord,
                "scanAdd detected illegal EVNT value");
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
        pesl = pevent_list[prio][evnt];
        if (pesl == NULL) {
            pesl = dbCalloc(1, sizeof(event_scan_list));
            pevent_list[prio][evnt] = pesl;
            pesl->scan_list.lock = epicsMutexMustCreate();
            callbackSetCallback(eventCallback, &pesl->callback);
            callbackSetPriority(prio, &pesl->callback);
            callbackSetUser(pesl, &pesl->callback);
            ellInit(&pesl->scan_list.list);
        }
        addToList(precord, &pesl->scan_list);
    } else if (scan == menuScanI_O_Intr) {
        io_scan_list *piosl = NULL;
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
        if (get_ioint_info(0, precord, &piosl)) {
            precord->scan = menuScanPassive;
            return;
        }
        if (piosl == NULL) {
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
        piosl += prio; /* get piosl for correct priority*/
        addToList(precord, &piosl->scan_list);
    } else if (scan >= SCAN_1ST_PERIODIC) {
        addToList(precord, &papPeriodic[scan - SCAN_1ST_PERIODIC]->scan_list);
    }
    return;
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
        int evnt;
        int prio;
        event_scan_list *pesl;
        scan_list *psl = 0;

        evnt = precord->evnt;
        if (evnt < 0 || evnt >= MAX_EVENTS) {
            recGblRecordError(S_db_badField, (void *)precord,
                "scanAdd detected illegal EVNT value");
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
        pesl = pevent_list[prio][evnt];
        if (pesl) psl = &pesl->scan_list;
        if (!pesl || !psl) 
            recGblRecordError(-1, (void *)precord,
                "scanDelete for bad evnt");
        else
            deleteFromList(precord, psl);
    } else if (scan == menuScanI_O_Intr) {
        io_scan_list *piosl=NULL;
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
        if (get_ioint_info(1, precord, &piosl)) return;
        if (piosl == NULL) {
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
        piosl += prio; /*get piosl for correct priority*/
        deleteFromList(precord, &piosl->scan_list);
    } else if (scan >= SCAN_1ST_PERIODIC) {
        deleteFromList(precord, &papPeriodic[scan - SCAN_1ST_PERIODIC]->scan_list);
    }
    return;
}

double scanPeriod(int scan) {
    scan -= SCAN_1ST_PERIODIC;
    if (scan < 0 || scan >= nPeriodic)
        return 0.0;
    return papPeriodic[scan]->period;
}

int scanppl(double period)      /* print periodic list */
{
    periodic_scan_list *ppsl;
    char message[80];
    int i;

    for (i = 0; i < nPeriodic; i++) {
        ppsl = papPeriodic[i];
        if (ppsl == NULL) continue;
        if (period > 0.0 && (fabs(period - ppsl->period) >.05)) continue;
        sprintf(message, "Records with SCAN = '%s' (%lu over-runs):",
            ppsl->name, ppsl->overruns);
        printList(&ppsl->scan_list, message);
    }
    return 0;
}

int scanpel(int event_number)   /* print event list */
{
    char message[80];
    int prio, evnt;
    event_scan_list *pesl;

    for (evnt = 0; evnt < MAX_EVENTS; evnt++) {
        if (event_number && evnt<event_number) continue;
        if (event_number && evnt>event_number) break;
        for (prio = 0; prio < NUM_CALLBACK_PRIORITIES; prio++) {
            pesl = pevent_list[prio][evnt];
            if (!pesl) continue;
            if (ellCount(&pesl->scan_list.list) == 0) continue;
            sprintf(message, "Event %d Priority %s", evnt, priorityName[prio]);
            printList(&pesl->scan_list, message);
        }
    }
    return 0;
}

int scanpiol(void)                  /* print io_event list */
{
    io_scan_list *piosl;
    int prio;
    char message[80];

    for(prio = 0; prio < NUM_CALLBACK_PRIORITIES; prio++) {
        piosl = iosl_head[prio];
        if (piosl == NULL) continue;
        sprintf(message, "IO Event: Priority %s", priorityName[prio]);
        while(piosl != NULL) {
            printList(&piosl->scan_list, message);
            piosl = piosl->next;
        }
    }
    return 0;
}

static void eventCallback(CALLBACK *pcallback)
{
    event_scan_list *pesl;

    callbackGetUser(pesl, pcallback);
    scanList(&pesl->scan_list);
}

static void initEvent(void)
{
    int evnt, prio;

    for (prio = 0; prio < NUM_CALLBACK_PRIORITIES; prio++) {
        for (evnt = 0; evnt < MAX_EVENTS; evnt++) {
            pevent_list[prio][evnt] = NULL;
        }
    }
}

void post_event(int event)
{
    int prio;
    event_scan_list *pesl;

    if (scanCtl != ctlRun) return;
    if (event < 0 || event >= MAX_EVENTS) {
        errMessage(-1, "illegal event passed to post_event");
        return;
    }
    for (prio=0; prio<NUM_CALLBACK_PRIORITIES; prio++) {
        pesl = pevent_list[prio][event];
        if (!pesl) continue;
        if (ellCount(&pesl->scan_list.list) >0)
            callbackRequest((void *)pesl);
    }
}

void scanIoInit(IOSCANPVT *ppioscanpvt)
{
    int prio;

    /* Allocate an array of io_scan_lists, one for each priority. */
    /* IOSCANPVT will hold the address of this array of structures */
    *ppioscanpvt = dbCalloc(NUM_CALLBACK_PRIORITIES, sizeof(io_scan_list));
    for (prio = 0; prio < NUM_CALLBACK_PRIORITIES; prio++) {
        io_scan_list *piosl = &(*ppioscanpvt)[prio];
        callbackSetCallback(ioeventCallback, &piosl->callback);
        callbackSetPriority(prio, &piosl->callback);
        callbackSetUser(piosl, &piosl->callback);
        ellInit(&piosl->scan_list.list);
        piosl->scan_list.lock = epicsMutexMustCreate();
        piosl->next = iosl_head[prio];
        iosl_head[prio] = piosl;
    }
}


void scanIoRequest(IOSCANPVT pioscanpvt)
{
    int prio;

    if (scanCtl != ctlRun) return;
    for (prio = 0; prio < NUM_CALLBACK_PRIORITIES; prio++) {
        io_scan_list *piosl = &pioscanpvt[prio];
        if (ellCount(&piosl->scan_list.list) > 0)
            callbackRequest(&piosl->callback);
    }
}

void scanOnce(struct dbCommon *precord)
{
    static int newOverflow = TRUE;
    int lockKey;
    int pushOK;

    lockKey = epicsInterruptLock();
    pushOK = epicsRingPointerPush(onceQ, precord);
    epicsInterruptUnlock(lockKey);

    if (!pushOK) {
        if (newOverflow) errlogPrintf("scanOnce: Ring buffer overflow\n");
        newOverflow = FALSE;
    } else {
        newOverflow = TRUE;
    }
    epicsEventSignal(onceSem);
}

static void onceTask(void *arg)
{
    taskwdInsert(0, NULL, NULL);
    epicsEventSignal(startStopEvent);

    while (TRUE) {
        void *precord;

        epicsEventMustWait(onceSem);
        while ((precord = epicsRingPointerPop(onceQ))) {
            if (precord == &exitOnce) goto shutdown;
            dbScanLock(precord);
            dbProcess(precord);
            dbScanUnlock(precord);
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
    if ((onceQ = epicsRingPointerCreate(onceQueueSize)) == NULL) {
        cantProceed("initOnce: Ring buffer create failed\n");
    }
    onceSem = epicsEventMustCreate(epicsEventEmpty);
    onceTaskId = epicsThreadCreate("scanOnce", epicsThreadPriorityScanHigh,
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
                    "\tScan processing averages %.2f seconds (%.2f .. %.2f).\n"
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
    dbMenu *pmenu;
    periodic_scan_list *ppsl;
    int i;

    pmenu = dbFindMenu(pdbbase, "menuScan");
    if (!pmenu) {
        epicsPrintf("initPeriodic: menuScan not present\n");
        return;
    }
    nPeriodic = pmenu->nChoice - SCAN_1ST_PERIODIC;
    papPeriodic = dbCalloc(nPeriodic, sizeof(periodic_scan_list*));
    periodicTaskId = dbCalloc(nPeriodic, sizeof(void *));
    for (i = 0; i < nPeriodic; i++) {
        ppsl = dbCalloc(1, sizeof(periodic_scan_list));

        ppsl->scan_list.lock = epicsMutexMustCreate();
        ellInit(&ppsl->scan_list.list);
        ppsl->name = pmenu->papChoiceValue[i + SCAN_1ST_PERIODIC];
        epicsScanDouble(ppsl->name, &ppsl->period);
        ppsl->scanCtl = ctlPause;
        ppsl->loopEvent = epicsEventMustCreate(epicsEventEmpty);

        papPeriodic[i] = ppsl;
    }
}

static void spawnPeriodic(int ind)
{
    periodic_scan_list *ppsl;
    char taskName[20];

    ppsl = papPeriodic[ind];
    sprintf(taskName, "scan%g", ppsl->period);
    periodicTaskId[ind] = epicsThreadCreate(
        taskName, epicsThreadPriorityScanLow + ind,
        epicsThreadGetStackSize(epicsThreadStackBig),
        periodicTask, (void *)ppsl);

    epicsEventWait(startStopEvent);
}

static void ioeventCallback(CALLBACK *pcallback)
{
    io_scan_list *piosl;

    callbackGetUser(piosl, pcallback);
    scanList(&piosl->scan_list);
}

static void printList(scan_list *psl, char *message)
{
    scan_element *pse;

    epicsMutexMustLock(psl->lock);
    pse = (scan_element *)ellFirst(&psl->list);
    epicsMutexUnlock(psl->lock);
    if (pse == NULL) return;
    printf("%s\n", message);
    while (pse != NULL) {
        printf("    %-28s\n", pse->precord->name);
        epicsMutexMustLock(psl->lock);
        if (pse->pscan_list != psl) {
            epicsMutexUnlock(psl->lock);
            printf("Scan list changed while processing.");
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

    /*Look for first record*/
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
    ptemp = (scan_element *)ellFirst(&psl->list);
    while (ptemp) {
        if (ptemp->precord->phas > precord->phas) {
            ellInsert(&psl->list, ellPrevious(&ptemp->node), &pse->node);
            break;
        }
        ptemp = (scan_element *)ellNext(&ptemp->node);
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
