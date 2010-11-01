/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* Original Author: Marty Kraimer
 * Date:  16JUN2000
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <errno.h>

#include "epicsEvent.h"
#include "epicsExit.h"
#include "epicsTypes.h"
#include "cantProceed.h"
#include "epicsThread.h"
#include "epicsMutex.h"
#include "errlog.h"
#include "epicsGeneralTime.h"
#include "generalTimeSup.h"
#include "iocsh.h"
#include "osdTime.h"
#include "osiNTPTime.h"
#include "taskwd.h"

#define NSEC_PER_SEC 1000000000
#define NTPTimeSyncInterval 60.0
#define NTPTimeSyncRetries 4


static struct {
    int             synchronize;
    int             synchronized;
    epicsEventId    loopEvent;
    int             syncsFailed;
    epicsMutexId    lock;
    epicsTimeStamp  syncTime;
    epicsUInt32     syncTick;
    epicsTimeStamp  clockTime;
    epicsUInt32     clockTick;
    epicsUInt32     nsecsPerTick;
    epicsUInt32     ticksPerSecond;
    epicsUInt32     ticksToSkip;
    double          tickRate;
} NTPTimePvt;

static epicsThreadOnceId onceId = EPICS_THREAD_ONCE_INIT;


/* Forward references */

static int NTPTimeGetCurrent(epicsTimeStamp *pDest);
static void NTPTimeSync(void *dummy);


/* NTPTime_Report iocsh command */
static const iocshArg ReportArg0 = { "interest_level", iocshArgArgv};
static const iocshArg * const ReportArgs[1] = { &ReportArg0 };
static const iocshFuncDef ReportFuncDef = {"NTPTime_Report", 1, ReportArgs};
static void ReportCallFunc(const iocshArgBuf *args)
{
    NTPTime_Report(args[0].ival);
}

/* NTPTime_Shutdown iocsh command */
static const iocshFuncDef ShutdownFuncDef = {"NTPTime_Shutdown", 0, NULL};
static void ShutdownCallFunc(const iocshArgBuf *args)
{
    NTPTime_Shutdown(NULL);
}


/* Initialization */

static void NTPTime_InitOnce(void *pprio)
{
    struct timespec timespecNow;

    NTPTimePvt.synchronize    = 1;
    NTPTimePvt.synchronized   = 0;
    NTPTimePvt.loopEvent      = epicsEventMustCreate(epicsEventEmpty);
    NTPTimePvt.syncsFailed    = 0;
    NTPTimePvt.lock           = epicsMutexCreate();
    NTPTimePvt.ticksPerSecond = osdTickRateGet();
    NTPTimePvt.nsecsPerTick   = NSEC_PER_SEC / NTPTimePvt.ticksPerSecond;

    /* Initialize OS-dependent code */
    osdNTPInit();

    /* Try to sync with NTP server */
    if (!osdNTPGet(&timespecNow)) {
        NTPTimePvt.syncTick = osdTickGet();
        if (timespecNow.tv_sec > POSIX_TIME_AT_EPICS_EPOCH && epicsTimeOK ==
            epicsTimeFromTimespec(&NTPTimePvt.syncTime, &timespecNow)) {
            NTPTimePvt.clockTick = NTPTimePvt.syncTick;
            NTPTimePvt.clockTime = NTPTimePvt.syncTime;
            NTPTimePvt.synchronized = 1;
        }
    }

    /* Start the sync thread */
    epicsThreadCreate("NTPTimeSync", epicsThreadPriorityHigh,
        epicsThreadGetStackSize(epicsThreadStackSmall),
        NTPTimeSync, NULL);

    epicsAtExit(NTPTime_Shutdown, NULL);

    /* Register the iocsh commands */
    iocshRegister(&ReportFuncDef, ReportCallFunc);
    iocshRegister(&ShutdownFuncDef, ShutdownCallFunc);

    /* Finally register as a time provider */
    generalTimeRegisterCurrentProvider("NTP", *(int *)pprio, NTPTimeGetCurrent);
}

void NTPTime_Init(int priority)
{
    epicsThreadOnce(&onceId, NTPTime_InitOnce, &priority);
}


/* Shutdown */

void NTPTime_Shutdown(void *dummy)
{
    NTPTimePvt.synchronize = 0;
    epicsEventSignal(NTPTimePvt.loopEvent);
}


/* Synchronization thread */

static void NTPTimeSync(void *dummy)
{
    taskwdInsert(0, NULL, NULL);

    for (epicsEventWaitWithTimeout(NTPTimePvt.loopEvent, NTPTimeSyncInterval);
         NTPTimePvt.synchronize;
         epicsEventWaitWithTimeout(NTPTimePvt.loopEvent, NTPTimeSyncInterval)) {
        int             status;
        struct timespec timespecNow;
        epicsTimeStamp  timeNow;
        epicsUInt32     tickNow;
        double          diff;
        double          ntpDelta;

        status = osdNTPGet(&timespecNow);
        tickNow = osdTickGet();

        if (status) {
            if (++NTPTimePvt.syncsFailed > NTPTimeSyncRetries &&
                NTPTimePvt.synchronized) {
                errlogPrintf("NTPTimeSync: NTP requests failing - %s\n",
                    strerror(errno));
                NTPTimePvt.synchronized = 0;
            }
            continue;
        }

        if (timespecNow.tv_sec <= POSIX_TIME_AT_EPICS_EPOCH ||
            epicsTimeFromTimespec(&timeNow, &timespecNow) == epicsTimeERROR) {
            errlogPrintf("NTPTimeSync: Bad time received from NTP server\n");
            NTPTimePvt.synchronized = 0;
            continue;
        }

        ntpDelta = epicsTimeDiffInSeconds(&timeNow, &NTPTimePvt.syncTime);
        if (ntpDelta <= 0.0 && NTPTimePvt.synchronized) {
            errlogPrintf("NTPTimeSync: NTP time not increasing, delta = %g\n",
                ntpDelta);
            NTPTimePvt.synchronized = 0;
            continue;
        }

        NTPTimePvt.syncsFailed = 0;
        if (!NTPTimePvt.synchronized) {
            errlogPrintf("NTPTimeSync: Sync recovered.\n");
        }

        epicsMutexMustLock(NTPTimePvt.lock);
        diff = epicsTimeDiffInSeconds(&timeNow, &NTPTimePvt.clockTime);
        if (diff >= 0.0) {
            NTPTimePvt.ticksToSkip = 0;
        } else { /* dont go back in time */
            NTPTimePvt.ticksToSkip = -diff * NTPTimePvt.ticksPerSecond;
        }
        NTPTimePvt.clockTick = tickNow;
        NTPTimePvt.clockTime = timeNow;
        NTPTimePvt.synchronized = 1;
        epicsMutexUnlock(NTPTimePvt.lock);

        NTPTimePvt.tickRate = (tickNow - NTPTimePvt.syncTick) / ntpDelta;
        NTPTimePvt.syncTick = tickNow;
        NTPTimePvt.syncTime = timeNow;
    }

    NTPTimePvt.synchronized = 0;
    taskwdRemove(0);
}


/* Time Provider Routine */

static int NTPTimeGetCurrent(epicsTimeStamp *pDest)
{
    epicsUInt32 tickNow;
    epicsUInt32 ticksSince;

    if (!NTPTimePvt.synchronized)
        return epicsTimeERROR;

    epicsMutexMustLock(NTPTimePvt.lock);

    tickNow = osdTickGet();
    ticksSince = tickNow - NTPTimePvt.clockTick;

    if (NTPTimePvt.ticksToSkip <= ticksSince) {
        if (NTPTimePvt.ticksToSkip) {
            ticksSince -= NTPTimePvt.ticksToSkip;
            NTPTimePvt.ticksToSkip = 0;
        }

        if (ticksSince) {
            epicsUInt32 secsSince = ticksSince / NTPTimePvt.ticksPerSecond;
            ticksSince -= secsSince * NTPTimePvt.ticksPerSecond;

            NTPTimePvt.clockTime.nsec += ticksSince * NTPTimePvt.nsecsPerTick;
            if (NTPTimePvt.clockTime.nsec >= NSEC_PER_SEC) {
                secsSince++;
                NTPTimePvt.clockTime.nsec -= NSEC_PER_SEC;
            }
            NTPTimePvt.clockTime.secPastEpoch += secsSince;
            NTPTimePvt.clockTick = tickNow;
        }
    }

    *pDest = NTPTimePvt.clockTime;

    epicsMutexUnlock(NTPTimePvt.lock);
    return 0;
}


/* Status Report */

int NTPTime_Report(int level)
{
    if (onceId == EPICS_THREAD_ONCE_INIT) {
        printf("NTP driver not initialized\n");
    } else if (NTPTimePvt.synchronize) {
        printf("NTP driver %s synchronized with server\n",
            NTPTimePvt.synchronized ? "is" : "is *not*");
        if (NTPTimePvt.syncsFailed) {
            printf("Last successful sync was %.1f minutes ago\n",
                NTPTimePvt.syncsFailed * NTPTimeSyncInterval / 60.0);
        }
        if (level) {
            char lastSync[32];
            epicsTimeToStrftime(lastSync, sizeof(lastSync),
                "%Y-%m-%d %H:%M:%S.%06f", &NTPTimePvt.syncTime);
            printf("Syncronization interval = %.1f seconds\n",
                NTPTimeSyncInterval);
            printf("Last synchronized at %s\n",
                lastSync);
            printf("OS tick rate = %u Hz (nominal)\n",
                NTPTimePvt.ticksPerSecond);
            printf("Measured tick rate = %.3f Hz\n",
                NTPTimePvt.tickRate);
            osdNTPReport();
        }
    } else {
        printf("NTP synchronization thread not running.\n");
    }
    return 0;
}
