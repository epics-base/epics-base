/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
#define ClockTimeSyncInterval_initial 1.0
#define ClockTimeSyncInterval_normal 60.0


static struct {
    int             synchronize;
    double          ClockTimeSyncInterval;
    epicsEventId    loopEvent;
    epicsMutexId    lock;
    int             synchronized;     /* Protected by lock */
    int             syncFromPriority; /* Protected by lock */
    epicsTimeStamp  startTime;        /* Protected by lock */
    epicsTimeStamp  syncTime;         /* Protected by lock */
} ClockTimePvt;

static epicsThreadOnceId onceId = EPICS_THREAD_ONCE_INIT;


#if defined(CLOCK_REALTIME) && !defined(_WIN32) && !defined(__APPLE__)
/* This code is not used on systems without Posix CLOCK_REALTIME,
 * but the only way to detect that is from the OS headers, so the
 * Makefile can't exclude compiling this file on those systems.
 */

/* Forward references */

int osdTimeGetCurrent(epicsTimeStamp *pDest);

#if defined(vxWorks) || defined(__rtems__)
static void ClockTimeSync(void *dummy);

/* ClockTime_Init iocsh command */
static const iocshArg InitArg0 = { "enable_sync", iocshArgArgv};
static const iocshArg * const InitArgs[1] = { &InitArg0 };
static const iocshFuncDef InitFuncDef = {
    "ClockTime_Init", 1, InitArgs,
    "Starts or stops the IOC periodically synchronizing the OS clock\n"
    "with the higest priority working time provider.\n"};
static void InitCallFunc(const iocshArgBuf *args)
{
    ClockTime_Init(args[0].ival);
}

/* ClockTime_Shutdown iocsh command */
static const iocshFuncDef ShutdownFuncDef = {
    "ClockTime_Shutdown", 0, NULL,
    "Stops the IOC's OS clock synchronization thread.\n"};
static void ShutdownCallFunc(const iocshArgBuf *args)
{
    ClockTime_Shutdown(NULL);
}
#endif

/* ClockTime_Report iocsh command */
static const iocshArg ReportArg0 = { "interest_level", iocshArgArgv};
static const iocshArg * const ReportArgs[1] = { &ReportArg0 };
static const iocshFuncDef ReportFuncDef = {
    "ClockTime_Report", 1, ReportArgs,
    "Reports the IOC's OS clock synchronization status:\n"
    "  - On VxWorks and RTEMS when synchronized:\n"
    "      * Synchronization time provider priority\n"
    "      * Initial and last synchronization times\n"
    "      * Synchronization interval\n"
    "  - Otherwise:\n"
    "      * Program start time\n"};
static void ReportCallFunc(const iocshArgBuf *args)
{
    ClockTime_Report(args[0].ival);
}


/* Initialization */

static void ClockTime_InitOnce(void *pfirst)
{
    *(int *) pfirst = 1;

    ClockTimePvt.loopEvent   = epicsEventMustCreate(epicsEventEmpty);
    ClockTimePvt.lock        = epicsMutexCreate();

    epicsAtExit(ClockTime_Shutdown, NULL);

    /* Register the iocsh commands */
#if defined(vxWorks) || defined(__rtems__)
    iocshRegister(&InitFuncDef, InitCallFunc);
    iocshRegister(&ShutdownFuncDef, ShutdownCallFunc);
#endif
    iocshRegister(&ReportFuncDef, ReportCallFunc);

    /* Register as a time provider */
    generalTimeRegisterCurrentProvider("OS Clock", LAST_RESORT_PRIORITY,
        osdTimeGetCurrent);
}

void ClockTime_Init(int synchronize)
{
    int firstTime = 0;

    epicsThreadOnce(&onceId, ClockTime_InitOnce, &firstTime);

    if (synchronize) {
        if (ClockTimePvt.synchronize == CLOCKTIME_NOSYNC) {

#if defined(vxWorks) || defined(__rtems__)
            /* Start synchronizing */
            ClockTimePvt.synchronize = CLOCKTIME_SYNC;
            ClockTimePvt.ClockTimeSyncInterval = ClockTimeSyncInterval_initial;
            ClockTimePvt.syncFromPriority = -1;

            epicsThreadCreate("ClockTimeSync", epicsThreadPriorityHigh,
                epicsThreadGetStackSize(epicsThreadStackSmall),
                ClockTimeSync, NULL);
#else
            errlogPrintf("Clock synchronization must be performed by the OS\n");
#endif

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
                osdTimeGetCurrent(&ClockTimePvt.startTime);
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

#if defined(vxWorks) || defined(__rtems__)
CLOCKTIME_SYNCHOOK ClockTime_syncHook = NULL;

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

            if (ClockTime_syncHook)
                ClockTime_syncHook(1);

            ClockTimePvt.ClockTimeSyncInterval = ClockTimeSyncInterval_normal;
        }
    }

    ClockTimePvt.synchronized = 0;
    if (ClockTime_syncHook)
        ClockTime_syncHook(0);
    taskwdRemove(0);
}
#endif


/* Time Provider Routine */

int osdTimeGetCurrent(epicsTimeStamp *pDest)
{
    struct timespec clockNow;

    /* If a Hi-Res clock is available and works, use it */
    #ifdef CLOCK_REALTIME_HR
        clock_gettime(CLOCK_REALTIME_HR, &clockNow) &&
        /* Note: Uses the lo-res clock below if the above call fails */
    #endif
    clock_gettime(CLOCK_REALTIME, &clockNow);

    if (!ClockTimePvt.synchronized &&
        clockNow.tv_sec < POSIX_TIME_AT_EPICS_EPOCH) {
        clockNow.tv_sec = POSIX_TIME_AT_EPICS_EPOCH + 86400;
        clockNow.tv_nsec = 0;

#if defined(vxWorks) || defined(__rtems__)
        clock_settime(CLOCK_REALTIME, &clockNow);
        errlogPrintf("WARNING: OS Clock time was read before being set.\n"
            "Using 1990-01-02 00:00:00.000000 UTC\n");
#else
        errlogPrintf("WARNING: OS Clock pre-dates the EPICS epoch!\n"
            "Using 1990-01-02 00:00:00.000000 UTC\n");
#endif
    }

    epicsTimeFromTimespec(pDest, &clockNow);
    return 0;
}

/* Used in Report function below: */
#define UNINIT_ERROR "initialized"
#else
#define UNINIT_ERROR "available"
#endif /* CLOCK_REALTIME && !WIN32 */

/* Allow the following report routine to be compiled anyway
 * to avoid getting a build warning from ranlib.
 */

/* Status Report */

int ClockTime_Report(int level)
{
    char timebuf[32];

    if (onceId == EPICS_THREAD_ONCE_INIT) {
        puts("OS Clock provider not " UNINIT_ERROR);
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
            printf("IOC is synchronizing OS Clock to a priority=%d provider\n",
                syncFromPriority);
            if (level) {
                epicsTimeToStrftime(timebuf, sizeof(timebuf),
                    "%Y-%m-%d %H:%M:%S.%06f", &startTime);
                printf("Initial sync was at %s\n", timebuf);
                epicsTimeToStrftime(timebuf, sizeof(timebuf),
                    "%Y-%m-%d %H:%M:%S.%06f", &syncTime);
                printf("Last successful sync was at %s\n", timebuf);
            }
        }
        else
            printf("OS Clock is *not* currently synchronized\n");

        printf("IOC synchronization interval = %.0f seconds\n",
            ClockTimePvt.ClockTimeSyncInterval);
    }
    else {
        epicsTimeToStrftime(timebuf, sizeof(timebuf),
            "%Y-%m-%d %H:%M:%S.%06f", &ClockTimePvt.startTime);
        printf("Program started at %s\n", timebuf);
#if defined(vxWorks) || defined(__rtems__)
        printf("IOC's OS Clock synchronization thread is not running.\n");
#endif
    }
#ifdef osdClockReport
    osdClockReport(level);
#endif
    return 0;
}
