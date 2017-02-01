/*************************************************************************\
* Copyright (c) 2016 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2008 Diamond Light Source Ltd
* Copyright (c) 2004 Oak Ridge National Laboratory
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Original Authors: David H. Thompson & Sheng Peng (ORNL) */

#include <string.h>
#include <stdlib.h>

#define epicsExportSharedSymbols
#include "epicsTypes.h"
#include "epicsEvent.h"
#include "epicsMutex.h"
#include "epicsMessageQueue.h"
#include "epicsString.h"
#include "epicsStdioRedirect.h"
#include "epicsThread.h"
#include "epicsTime.h"
#include "epicsTimer.h"
#include "epicsInterrupt.h"
#include "osiSock.h"
#include "ellLib.h"
#include "errlog.h"
#include "cantProceed.h"
#include "envDefs.h"
#include "generalTimeSup.h"
#include "epicsGeneralTime.h"

/* Change 'undef' to 'define' to turn on debug statements: */
#undef DEBUG_GENERAL_TIME

#ifdef DEBUG_GENERAL_TIME
    int generalTimeDebug = 10;
#   define IFDEBUG(n) \
        if (generalTimeDebug >= n) /* block or statement */
#else
#   define IFDEBUG(n) \
        if(0) /* Compiler will elide the block or statement */
#endif

/* Declarations */

typedef struct {
    ELLNODE node;
    char    *name;
    int     priority;
    union {
        TIMECURRENTFUN Time;
        TIMEEVENTFUN   Event;
    } get;
    union {
        TIMECURRENTFUN Time;
        TIMEEVENTFUN   Event;
    } getInt;
} gtProvider;

static struct {
    epicsMutexId    timeListLock;
    ELLLIST         timeProviders;
    gtProvider      *lastTimeProvider;
    epicsTimeStamp  lastProvidedTime;

    epicsMutexId    eventListLock;
    ELLLIST         eventProviders;
    gtProvider      *lastEventProvider;
    epicsTimeStamp  eventTime[NUM_TIME_EVENTS];
    epicsTimeStamp  lastProvidedBestTime;

    int               ErrorCounts;
} gtPvt;

static epicsThreadOnceId onceId = EPICS_THREAD_ONCE_INIT;

static const char * const tsfmt = "%Y-%m-%d %H:%M:%S.%09f";

/* Implementation */

static void generalTime_InitOnce(void *dummy)
{
    ellInit(&gtPvt.timeProviders);
    gtPvt.timeListLock = epicsMutexMustCreate();

    ellInit(&gtPvt.eventProviders);
    gtPvt.eventListLock = epicsMutexMustCreate();

    IFDEBUG(1)
        printf("General Time Initialized\n");
}

void generalTime_Init(void)
{
    epicsThreadOnce(&onceId, generalTime_InitOnce, NULL);
}


int generalTimeGetExceptPriority(epicsTimeStamp *pDest, int *pPrio, int ignore)
{
    gtProvider *ptp;
    int status = S_time_noProvider;

    generalTime_Init();

    IFDEBUG(2)
        printf("generalTimeGetExceptPriority(ignore=%d)\n", ignore);

    epicsMutexMustLock(gtPvt.timeListLock);
    for (ptp = (gtProvider *)ellFirst(&gtPvt.timeProviders);
         ptp; ptp = (gtProvider *)ellNext(&ptp->node)) {
        if ((ignore > 0 && ptp->priority == ignore) ||
            (ignore < 0 && ptp->priority != -ignore))
            continue;

        status = ptp->get.Time(pDest);
        if (status == epicsTimeOK) {
            /* No ratchet, time from this routine may go backwards */
            if (pPrio)
                *pPrio = ptp->priority;
            break;
        }
        else IFDEBUG(2)
            printf("gTGExP provider '%s' returned error\n", ptp->name);
    }
    epicsMutexUnlock(gtPvt.timeListLock);

    IFDEBUG(2) {
        if (ptp && status == epicsTimeOK) {
            char buff[40];

            epicsTimeToStrftime(buff, sizeof(buff), tsfmt, pDest);
            printf("gTGExP returning %s from provider '%s'\n",
                buff, ptp->name);
        }
        else
            printf("gTGExP returning error\n");
    }

    return status;
}

int epicsShareAPI epicsTimeGetCurrent(epicsTimeStamp *pDest)
{
    gtProvider *ptp;
    int status = S_time_noProvider;
    epicsTimeStamp ts;

    generalTime_Init();

    IFDEBUG(20)
        printf("epicsTimeGetCurrent()\n");

    epicsMutexMustLock(gtPvt.timeListLock);
    for (ptp = (gtProvider *)ellFirst(&gtPvt.timeProviders);
         ptp; ptp = (gtProvider *)ellNext(&ptp->node)) {

        status = ptp->get.Time(&ts);
        if (status == epicsTimeOK) {
            /* check time is monotonic */
            if (epicsTimeGreaterThanEqual(&ts, &gtPvt.lastProvidedTime)) {
                *pDest = ts;
                gtPvt.lastProvidedTime = ts;
                gtPvt.lastTimeProvider = ptp;
            } else {
                int key;

                *pDest = gtPvt.lastProvidedTime;
                key = epicsInterruptLock();
                gtPvt.ErrorCounts++;
                epicsInterruptUnlock(key);

                IFDEBUG(10) {
                    char last[40], buff[40];

                    epicsTimeToStrftime(last, sizeof(last), tsfmt,
                        &gtPvt.lastProvidedTime);
                    epicsTimeToStrftime(buff, sizeof(buff), tsfmt, &ts);
                    printf("eTGC provider '%s' returned older time\n"
                        "    %s, using %s instead\n", ptp->name, buff, last);
                }
            }
            break;
        }
    }
    if (status)
        gtPvt.lastTimeProvider = NULL;
    epicsMutexUnlock(gtPvt.timeListLock);

    IFDEBUG(20) {
        if (ptp && status == epicsTimeOK) {
            char buff[40];

            epicsTimeToStrftime(buff, sizeof(buff), tsfmt, &ts);
            printf("eTGC returning %s from provider '%s'\n",
                buff, ptp->name);
        }
        else
            printf("eTGC returning error\n");
    }

    return status;
}

int epicsTimeGetCurrentInt(epicsTimeStamp *pDest)
{
    gtProvider *ptp = gtPvt.lastTimeProvider;

    if (ptp == NULL ||
        ptp->getInt.Time == NULL) {
        IFDEBUG(20)
            epicsInterruptContextMessage("eTGCInt: No support\n");
        return S_time_noProvider;
    }

    return ptp->getInt.Time(pDest);
}


static int generalTimeGetEventPriority(epicsTimeStamp *pDest, int eventNumber,
    int *pPrio)
{
    gtProvider *ptp;
    int status = S_time_noProvider;
    epicsTimeStamp ts;

    generalTime_Init();

    IFDEBUG(2)
        printf("generalTimeGetEventPriority(eventNum=%d)\n", eventNumber);

    if ((eventNumber < 0 || eventNumber >= NUM_TIME_EVENTS) &&
        (eventNumber != epicsTimeEventBestTime))
        return S_time_badEvent;

    epicsMutexMustLock(gtPvt.eventListLock);
    for (ptp = (gtProvider *)ellFirst(&gtPvt.eventProviders);
         ptp; ptp = (gtProvider *)ellNext(&ptp->node)) {

        status = ptp->get.Event(&ts, eventNumber);
        if (status == epicsTimeOK) {
            gtPvt.lastEventProvider = ptp;
            if (pPrio)
                *pPrio = ptp->priority;

            if (eventNumber == epicsTimeEventBestTime) {
                if (epicsTimeGreaterThanEqual(&ts,
                        &gtPvt.lastProvidedBestTime)) {
                    *pDest = ts;
                    gtPvt.lastProvidedBestTime = ts;
                } else {
                    int key;

                    *pDest = gtPvt.lastProvidedBestTime;
                    key = epicsInterruptLock();
                    gtPvt.ErrorCounts++;
                    epicsInterruptUnlock(key);

                    IFDEBUG(10) {
                        char last[40], buff[40];

                        epicsTimeToStrftime(last, sizeof(last), tsfmt,
                            &gtPvt.lastProvidedBestTime);
                        epicsTimeToStrftime(buff, sizeof(buff), tsfmt, &ts);
                        printf("gTGEvP provider '%s' returned older time\n"
                            "    %s, using %s instead\n",
                            ptp->name, buff, last);
                    }
                }
            } else {
                if (epicsTimeGreaterThanEqual(pDest,
                        &gtPvt.eventTime[eventNumber])) {
                    *pDest = ts;
                    gtPvt.eventTime[eventNumber] = ts;
                } else {
                    int key;

                    *pDest = gtPvt.eventTime[eventNumber];
                    key = epicsInterruptLock();
                    gtPvt.ErrorCounts++;
                    epicsInterruptUnlock(key);
                }

                    IFDEBUG(10) {
                        char last[40], buff[40];

                        epicsTimeToStrftime(last, sizeof(last), tsfmt,
                            &gtPvt.lastProvidedBestTime);
                        epicsTimeToStrftime(buff, sizeof(buff), tsfmt, &ts);
                        printf("gTGEvP provider '%s' returned older time\n"
                            "    %s, using %s instead\n",
                            ptp->name, buff, last);
                    }
            }
            break;
        }
        else IFDEBUG(2)
            printf("gTGEvP provider '%s' returned error\n", ptp->name);
    }
    if (status)
        gtPvt.lastEventProvider = NULL;
    epicsMutexUnlock(gtPvt.eventListLock);

    IFDEBUG(10) {
        if (ptp && status == epicsTimeOK) {
            char buff[40];

            epicsTimeToStrftime(buff, sizeof(buff), tsfmt, &ts);
            printf("gTGEvP returning %s from provider '%s'\n",
                buff, ptp->name);
        }
        else
            printf("gTGEvP returning error\n");
    }

    return status;
}

int epicsShareAPI epicsTimeGetEvent(epicsTimeStamp *pDest, int eventNumber)
{
    if (eventNumber == epicsTimeEventCurrentTime) {
        return epicsTimeGetCurrent(pDest);
    } else {
        return generalTimeGetEventPriority(pDest, eventNumber, NULL);
    }
}

int epicsTimeGetEventInt(epicsTimeStamp *pDest, int eventNumber)
{
    if (eventNumber == epicsTimeEventCurrentTime) {
        return epicsTimeGetCurrentInt(pDest);
    } else {
        gtProvider *ptp = gtPvt.lastEventProvider;

        if (ptp == NULL ||
            ptp->getInt.Event == NULL) {
            IFDEBUG(20)
                epicsInterruptContextMessage("eTGEvInt: No support\n");
            return S_time_noProvider;
        }

        return ptp->getInt.Event(pDest, eventNumber);
    }
}


/* Provider Registration */

static void insertProvider(gtProvider *ptp, ELLLIST *plist, epicsMutexId lock)
{
    gtProvider *ptpref;

    epicsMutexMustLock(lock);

    for (ptpref = (gtProvider *)ellFirst(plist);
         ptpref; ptpref = (gtProvider *)ellNext(&ptpref->node)) {
        if (ptpref->priority > ptp->priority)
            break;
    }

    if (ptpref) {
        /* Found a provider below the new one */
        ptpref = (gtProvider *)ellPrevious(&ptpref->node);
        ellInsert(plist, &ptpref->node, &ptp->node);
    } else {
        ellAdd(plist, &ptp->node);
    }

    epicsMutexUnlock(lock);
}

static gtProvider * findProvider(ELLLIST *plist, epicsMutexId lock,
    const char *name, int priority)
{
    gtProvider *ptp;

    epicsMutexMustLock(lock);

    for (ptp = (gtProvider *)ellFirst(plist);
         ptp; ptp = (gtProvider *)ellNext(&ptp->node)) {
        if (ptp->priority == priority &&
            !strcmp(ptp->name, name))
            break;
    }

    epicsMutexUnlock(lock);
    return ptp;
}

int generalTimeRegisterEventProvider(const char *name, int priority,
    TIMEEVENTFUN getEvent)
{
    gtProvider *ptp;

    generalTime_Init();

    if (name == NULL || getEvent == NULL)
        return S_time_badArgs;

    ptp = (gtProvider *)malloc(sizeof(gtProvider));
    if (ptp == NULL)
        return S_time_noMemory;

    ptp->name         = epicsStrDup(name);
    ptp->priority     = priority;
    ptp->get.Event    = getEvent;
    ptp->getInt.Event = NULL;

    insertProvider(ptp, &gtPvt.eventProviders, gtPvt.eventListLock);

    IFDEBUG(1)
        printf("Registered event provider '%s' at %d\n", name, priority);

    return epicsTimeOK;
}

int generalTimeAddIntEventProvider(const char *name, int priority,
    TIMEEVENTFUN getEvent)
{
    gtProvider *ptp = findProvider(&gtPvt.eventProviders, gtPvt.eventListLock,
        name, priority);
    if (ptp == NULL)
        return S_time_noProvider;

    ptp->getInt.Event = getEvent;

    IFDEBUG(1)
        printf("Event provider '%s' is interrupt-callable\n", name);

    return epicsTimeOK;
}

int generalTimeRegisterCurrentProvider(const char *name, int priority,
    TIMECURRENTFUN getTime)
{
    gtProvider *ptp;

    generalTime_Init();

    if (name == NULL || getTime == NULL)
        return S_time_badArgs;

    ptp = (gtProvider *)malloc(sizeof(gtProvider));
    if (ptp == NULL)
        return S_time_noMemory;

    ptp->name        = epicsStrDup(name);
    ptp->priority    = priority;
    ptp->get.Time    = getTime;
    ptp->getInt.Time = NULL;

    insertProvider(ptp, &gtPvt.timeProviders, gtPvt.timeListLock);

    IFDEBUG(1)
        printf("Registered time provider '%s' at %d\n", name, priority);

    return epicsTimeOK;
}

int generalTimeAddIntCurrentProvider(const char *name, int priority,
    TIMECURRENTFUN getTime)
{
    gtProvider *ptp = findProvider(&gtPvt.timeProviders, gtPvt.timeListLock,
        name, priority);
    if (ptp == NULL)
        return S_time_noProvider;

    ptp->getInt.Time = getTime;

    IFDEBUG(1)
        printf("Time provider '%s' is interrupt-callable\n", name);

    return epicsTimeOK;
}

/* 
 * Provide an optional "last resort" provider for Event Time.
 * 
 * This is deliberately optional, as it represents site policy.
 * It is intended to be installed as an EventTime provider at the lowest
 * priority, to return the current time for an event if there is no
 * better time provider for event times.
 * 
 * Typically, this will only be used during startup, or a time-provider
 * resynchronisation, where events are being generated by the event system
 * but the time provided by the system is not yet valid.
 */
static int lastResortGetEvent(epicsTimeStamp *timeStamp, int eventNumber)
{
    return epicsTimeGetCurrent(timeStamp);
}

int installLastResortEventProvider(void)
{
    return generalTimeEventTpRegister("Last Resort Event",
        LAST_RESORT_PRIORITY, lastResortGetEvent);
}


/* Status Report */

long generalTimeReport(int level)
{
    int items;

    if (onceId == EPICS_THREAD_ONCE_INIT) {
        printf("General time framework not yet initialized.\n");
        return epicsTimeOK;
    }

    printf("Backwards time errors prevented %u times.\n\n",
        generalTimeGetErrorCounts());

    /* Use an output buffer to avoid holding mutexes during printing */

    printf("Current Time Providers:\n");
    epicsMutexMustLock(gtPvt.timeListLock);
    if ((items = ellCount(&gtPvt.timeProviders))) {
        gtProvider *ptp;
        char *message;     /* Temporary output buffer */
        char *pout;

        message = calloc(items, 80 * 2); /* Each provider needs 2 lines */
        if (!message) {
            epicsMutexUnlock(gtPvt.timeListLock);
            printf("Out of memory\n");
            return S_time_noMemory;
        }

        pout = message;

        for (ptp = (gtProvider *)ellFirst(&gtPvt.timeProviders);
             ptp; ptp = (gtProvider *)ellNext(&ptp->node)) {
            pout += sprintf(pout, "    \"%s\", priority = %d\n",
                ptp->name, ptp->priority);
            if (level) {
                epicsTimeStamp tempTS;
                if (ptp->get.Time(&tempTS) == epicsTimeOK) {
                    char tempTSText[40];
                    epicsTimeToStrftime(tempTSText, sizeof(tempTSText),
                        "%Y-%m-%d %H:%M:%S.%06f", &tempTS);
                    pout += sprintf(pout, "\tCurrent Time is %s.\n",
                        tempTSText);
                } else {
                    pout += sprintf(pout, "\tCurrent Time not available\n");
                }
            }
        }
        epicsMutexUnlock(gtPvt.timeListLock);
        puts(message);
        free(message);
    } else {
        epicsMutexUnlock(gtPvt.timeListLock);
        printf("\tNo Providers registered.\n");
    }

    printf("Event Time Providers:\n");
    epicsMutexMustLock(gtPvt.eventListLock);
    if ((items = ellCount(&gtPvt.eventProviders)))
    {
        gtProvider *ptp;
        char *message;     /* Temporary output buffer */
        char *pout;

        message = calloc(items, 80);     /* Each provider needs 1 line, */
        if (!message) {
            epicsMutexUnlock(gtPvt.eventListLock);
            printf("Out of memory\n");
            return S_time_noMemory;
        }

        pout = message;

        for (ptp = (gtProvider *)ellFirst(&gtPvt.eventProviders);
             ptp; ptp = (gtProvider *)ellNext(&ptp->node)) {
            pout += sprintf(pout, "    \"%s\", priority = %d\n",
                ptp->name, ptp->priority);
        }
        epicsMutexUnlock(gtPvt.eventListLock);
        puts(message);
        free(message);
    }
    else
    {
        epicsMutexUnlock(gtPvt.eventListLock);
        printf("\tNo Providers registered.\n");
    }

    return epicsTimeOK;
}

/*
 * Access to internal status values.
 */

void generalTimeResetErrorCounts(void)
{
    int key = epicsInterruptLock();
    gtPvt.ErrorCounts = 0;
    epicsInterruptUnlock(key);
}

int generalTimeGetErrorCounts(void)
{
    int key = epicsInterruptLock();
    int errors = gtPvt.ErrorCounts;
    epicsInterruptUnlock(key);
    return errors;
}

const char * generalTimeCurrentProviderName(void)
{
    if (gtPvt.lastTimeProvider)
        return gtPvt.lastTimeProvider->name;
    return NULL;
}

const char * generalTimeEventProviderName(void)
{
    if (gtPvt.lastEventProvider)
        return gtPvt.lastEventProvider->name;
    return NULL;
}

const char * generalTimeHighestCurrentName(void)
{
    gtProvider *ptp;

    epicsMutexMustLock(gtPvt.timeListLock);
    ptp = (gtProvider *)ellFirst(&gtPvt.timeProviders);
    epicsMutexUnlock(gtPvt.timeListLock);
    return ptp ? ptp->name : NULL;
}
