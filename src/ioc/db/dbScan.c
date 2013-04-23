/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
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

#include "epicsStdlib.h"
#include "epicsStdio.h"
#include "epicsString.h"
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
#define epicsExportSharedSymbols
#include "dbBase.h"
#include "dbStaticLib.h"
#include "dbFldTypes.h"
#include "link.h"
#include "devSup.h"
#include "dbCommon.h"
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

typedef struct periodic_scan_list {
    scan_list           scan_list;
    double              period;
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

    initPeriodic();
    initOnce();
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
        sprintf(message, "Scan Period = %g seconds (%lu over-runs)",
            ppsl->period, ppsl->overruns);
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
    scan_list *psl;

    callbackGetUser(psl, pcallback);
    scanList(psl);
}

static void initEvent(void)
{
}

event_list *eventNameToHandle(const char *eventname)
{
    int prio;
    event_list *pel;
    static epicsMutexId lock = NULL;

    if (!lock) lock = epicsMutexMustCreate();
    if (!eventname || eventname[0] == 0) return NULL;
    epicsMutexMustLock(lock);
    for (pel = pevent_list[0]; pel; pel=pel->next) {
        if (strcmp(pel->event_name, eventname) == 0) break;
    }
    if (pel == NULL) {
        pel = dbCalloc(1, sizeof(event_list));
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
    epicsMutexUnlock(lock);
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
    double report_delay = 10.0;

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
            delay = 0.1;
            ppsl->overruns++;
            next = now;
            if (++overruns >= 10 &&
                epicsTimeDiffInSeconds(&now, &reported) > report_delay) {
                errlogPrintf("dbScan warning: %g second scan over-ran %u times\n",
                    ppsl->period, overruns);

                reported = now;
                if (report_delay < 1800.0)
                    report_delay *= 2;
                else
                    report_delay = 3600.0;  /* At most hourly */
            }
        }
        else {
            overruns = 0;
            report_delay = 10.0;
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

        ppsl->scan_list.lock = epicsMutexMustCreate();
        ellInit(&ppsl->scan_list.list);
        if (status || number == 0) {
            errlogPrintf("initPeriodic: Bad menuScan choice '%s'\n", choice);
            ppsl->period = i;
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
            ppsl->period = i;
        }
        number = ppsl->period / quantum;
        if ((ppsl->period < 2 * quantum) ||
            (number / floor(number) > 1.1)) {
            errlogPrintf("initPeriodic: Scan rate '%s' is not achievable.\n",
                choice);
        }
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
    sprintf(taskName, "scan-%g", ppsl->period);
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
