/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define epicsExportSharedSymbols
#include "epicsEvent.h"
#include "epicsExit.h"
#include "epicsMutex.h"
#include "epicsThread.h"
#include "errlog.h"
#include "epicsGeneralTime.h"
#include "generalTimeSup.h"
#include "iocsh.h"
#include "osiClockTime.h"
#include "taskwd.h"

#define NSEC_PER_SEC 1000000000
#define ClockTimeSyncInterval_value 60.0


static struct {
    int             synchronize;
    int             synchronized;
    epicsEventId    loopEvent;
    epicsTimeStamp  startTime;
    epicsTimeStamp  syncTime;
    double          ClockTimeSyncInterval;
    int             syncFromPriority;
    epicsMutexId    lock;
} ClockTimePvt;

static epicsThreadOnceId onceId = EPICS_THREAD_ONCE_INIT;


#if defined(CLOCK_REALTIME) && !defined(_WIN32)
/* This code is not used on systems without Posix CLOCK_REALTIME such
 * as Darwin, but the only way to detect that is from the OS headers,
 * so the Makefile can't exclude building this file on those systems.
 */

/* Forward references */

static int ClockTimeGetCurrent(epicsTimeStamp *pDest);
static void ClockTimeSync(void *dummy);


/* ClockTime_Report iocsh command */
static const iocshArg ReportArg0 = { "interest_level", iocshArgArgv};
static const iocshArg * const ReportArgs[1] = { &ReportArg0 };
static const iocshFuncDef ReportFuncDef = {"ClockTime_Report", 1, ReportArgs};
static void ReportCallFunc(const iocshArgBuf *args)
{
    ClockTime_Report(args[0].ival);
}

/* ClockTime_Shutdown iocsh command */
static const iocshFuncDef ShutdownFuncDef = {"ClockTime_Shutdown", 0, NULL};
static void ShutdownCallFunc(const iocshArgBuf *args)
{
    ClockTime_Shutdown(NULL);
}


/* Initialization */

static void ClockTime_InitOnce(void *pfirst)
{
    *(int *) pfirst = 1;

    ClockTimePvt.loopEvent   = epicsEventMustCreate(epicsEventEmpty);
    ClockTimePvt.lock        = epicsMutexCreate();
    ClockTimePvt.ClockTimeSyncInterval = 1.0;   /* First sync */

    epicsAtExit(ClockTime_Shutdown, NULL);

    /* Register the iocsh commands */
    iocshRegister(&ReportFuncDef, ReportCallFunc);
    iocshRegister(&ShutdownFuncDef, ShutdownCallFunc);

    /* Register as a time provider */
    generalTimeRegisterCurrentProvider("OS Clock", LAST_RESORT_PRIORITY,
        ClockTimeGetCurrent);
}

void ClockTime_Init(int synchronize)
{
    int firstTime = 0;

    epicsThreadOnce(&onceId, ClockTime_InitOnce, &firstTime);

    if (synchronize == CLOCKTIME_SYNC) {
        if (ClockTimePvt.synchronize == CLOCKTIME_NOSYNC) {
            /* Start synchronizing */
            ClockTimePvt.synchronize = synchronize;

            epicsThreadCreate("ClockTimeSync", epicsThreadPriorityHigh,
                epicsThreadGetStackSize(epicsThreadStackSmall),
                ClockTimeSync, NULL);
        }
        else {
            /* No change, sync thread should already be running */
        }
    }
    else {
        if (ClockTimePvt.synchronize == CLOCKTIME_SYNC) {
            /* Turn off synchronization thread */
            ClockTime_Shutdown(NULL);
        }
        else {
            /* No synchronization thread */
            if (firstTime)
                ClockTimeGetCurrent(&ClockTimePvt.startTime);
        }
    }
}


/* Shutdown */

void ClockTime_Shutdown(void *dummy)
{
    ClockTimePvt.synchronize = CLOCKTIME_NOSYNC;
    epicsEventSignal(ClockTimePvt.loopEvent);
}

void ClockTime_GetProgramStart(epicsTimeStamp *pDest)
{
    *pDest = ClockTimePvt.startTime;
}


/* Synchronization thread */

static void ClockTimeSync(void *dummy)
{
    taskwdInsert(0, NULL, NULL);

    for (epicsEventWaitWithTimeout(ClockTimePvt.loopEvent,
             ClockTimePvt.ClockTimeSyncInterval);
         ClockTimePvt.synchronize == CLOCKTIME_SYNC;
         epicsEventWaitWithTimeout(ClockTimePvt.loopEvent,
             ClockTimePvt.ClockTimeSyncInterval)) {
        epicsTimeStamp timeNow;
        int priority;

        if (generalTimeGetExceptPriority(&timeNow, &priority,
                LAST_RESORT_PRIORITY) == epicsTimeOK) {
            struct timespec clockNow;

            epicsTimeToTimespec(&clockNow, &timeNow);
            if (clock_settime(CLOCK_REALTIME, &clockNow)) {
                errlogPrintf("ClockTimeSync: clock_settime failed\n");
                continue;
            }

            epicsMutexMustLock(ClockTimePvt.lock);
            if (!ClockTimePvt.synchronized) {
                ClockTimePvt.startTime    = timeNow;
                ClockTimePvt.synchronized = 1;
            }
            ClockTimePvt.syncFromPriority = priority;
            ClockTimePvt.syncTime         = timeNow;
            epicsMutexUnlock(ClockTimePvt.lock);

            ClockTimePvt.ClockTimeSyncInterval = ClockTimeSyncInterval_value;
        }
    }

    ClockTimePvt.synchronized = 0;
    taskwdRemove(0);
}


/* Time Provider Routine */

static int ClockTimeGetCurrent(epicsTimeStamp *pDest)
{
    struct timespec clockNow;

    /* If a Hi-Res clock is available and works, use it */
    #ifdef CLOCK_REALTIME_HR
        clock_gettime(CLOCK_REALTIME_HR, &clockNow) &&
    #endif
    clock_gettime(CLOCK_REALTIME, &clockNow);

    if (!ClockTimePvt.synchronized &&
        clockNow.tv_sec < POSIX_TIME_AT_EPICS_EPOCH) {
        clockNow.tv_sec = POSIX_TIME_AT_EPICS_EPOCH + 86400;
        clockNow.tv_nsec = 0;
        clock_settime(CLOCK_REALTIME, &clockNow);
        errlogPrintf("WARNING: OS Clock time was read before being set.\n"
            "Using 1990-01-02 00:00:00.000000 UTC\n");
    }

    epicsTimeFromTimespec(pDest, &clockNow);
    return 0;
}

#endif /* CLOCK_REALTIME */

/* Allow the following report routine to be compiled anyway
 * to avoid getting a build warning from ranlib.
 */

/* Status Report */

int ClockTime_Report(int level)
{
    char timebuf[32];

    if (onceId == EPICS_THREAD_ONCE_INIT) {
        printf("OS Clock driver not %s.\n",
#ifdef CLOCK_REALTIME
            "initialized"
#else
            "available"
#endif /* CLOCK_REALTIME */
            );
    }
    else if (ClockTimePvt.synchronize == CLOCKTIME_SYNC) {
        int synchronized, syncFromPriority;
        epicsTimeStamp startTime, syncTime;

        epicsMutexMustLock(ClockTimePvt.lock);
        synchronized = ClockTimePvt.synchronized;
        syncFromPriority = ClockTimePvt.syncFromPriority;
        startTime = ClockTimePvt.startTime;
        syncTime = ClockTimePvt.syncTime;
        epicsMutexUnlock(ClockTimePvt.lock);

        if (synchronized) {
            printf("OS Clock driver is synchronized to a priority=%d provider\n",
                syncFromPriority);
            if (level) {
                epicsTimeToStrftime(timebuf, sizeof(timebuf),
                    "%Y-%m-%d %H:%M:%S.%06f", &startTime);
                printf("Initial sync was at %s\n", timebuf);
                epicsTimeToStrftime(timebuf, sizeof(timebuf),
                    "%Y-%m-%d %H:%M:%S.%06f", &syncTime);
                printf("Last successful sync was at %s\n", timebuf);
            }
            printf("Syncronization interval = %.0f seconds\n",
                ClockTimePvt.ClockTimeSyncInterval);
        }
        else
            printf("OS Clock driver is *not* synchronized\n");
    }
    else {
        epicsTimeToStrftime(timebuf, sizeof(timebuf),
            "%Y-%m-%d %H:%M:%S.%06f", &ClockTimePvt.startTime);
        printf("Program started at %s\n", timebuf);
        printf("OS Clock synchronization thread not running.\n");
    }
    return 0;
}
