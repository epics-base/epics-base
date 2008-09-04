/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* Original Author:  Marty Kraimer
 * Date:  16JUN2000 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <errno.h>

#include "epicsTypes.h"
#include "cantProceed.h"
#include "epicsThread.h"
#include "epicsMutex.h"
#include "epicsTime.h"
#include "envDefs.h"
#include "errlog.h"
#include "epicsGeneralTime.h"
#include "generalTimeSup.h"
#include "osdTime.h"

#define NSEC_PER_SEC 1000000000
#define NTPTimeSyncInterval 10.0


static struct {
    int             synchronized;
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


static void NTPTime_InitOnce(void *pprio)
{
    struct timespec timespecNow;

    NTPTimePvt.synchronized = 0;
    NTPTimePvt.syncsFailed = 0;
    NTPTimePvt.lock = epicsMutexCreate();
    NTPTimePvt.ticksPerSecond = osdTickRateGet();
    NTPTimePvt.nsecsPerTick = NSEC_PER_SEC / NTPTimePvt.ticksPerSecond;

    /* Initialize OS-dependent code */
    osdNTPInit();

    /* If TIMEZONE not defined, set it from EPICS_TIMEZONE */
    if (getenv("TIMEZONE") == NULL) {
        const char *timezone = envGetConfigParamPtr(&EPICS_TIMEZONE);
        if (timezone == NULL) {
            printf("NTPTime_Init: No Time Zone Information\n");
        } else {
            epicsEnvSet("TIMEZONE", timezone);
        }
    }

    /* Try to sync with NTP server */
    if (!osdNTPGet(&timespecNow)) {
        NTPTimePvt.syncTick = osdTickGet();
        epicsTimeFromTimespec(&NTPTimePvt.syncTime, &timespecNow);
        NTPTimePvt.clockTick = NTPTimePvt.syncTick;
        NTPTimePvt.clockTime = NTPTimePvt.syncTime;
        NTPTimePvt.synchronized = 1;
    }

    epicsThreadCreate("NTPTimeSync", epicsThreadPriorityHigh,
        epicsThreadGetStackSize(epicsThreadStackSmall),
        NTPTimeSync, NULL);

    /* Finally register as a time provider */
    generalTimeCurrentTpRegister("NTP", *(int *)pprio, NTPTimeGetCurrent);
}

long NTPTime_Init(int priority)
{
    epicsThreadOnce(&onceId, NTPTime_InitOnce, &priority);
    return 0;
}

static void NTPTimeSync(void *dummy)
{
    int prevStatusBad = 0;

    while (1) {
        int             status;
        struct timespec timespecNow;
        epicsTimeStamp  timeNow;
        epicsUInt32     tickNow;
        double          d;

        epicsThreadSleep(NTPTimeSyncInterval);

        status = osdNTPGet(&timespecNow);
        tickNow = osdTickGet();

        if (status) {
            NTPTimePvt.syncsFailed++;

            /* wait 1 minute before reporting failure */
            if (NTPTimePvt.syncsFailed < 60 / NTPTimeSyncInterval)
                continue;

            if (!prevStatusBad)
                errlogPrintf("NTPTimeSync: NTP requests failing - %s\n",
                    strerror(errno));

            prevStatusBad = 1;
            NTPTimePvt.synchronized = 0;
            continue;
        }

        NTPTimePvt.syncsFailed = 0;
        if (prevStatusBad) {
            errlogPrintf("NTPTimeSync: Sync recovered.\n");
            prevStatusBad = 0;
        }

        epicsTimeFromTimespec(&timeNow, &timespecNow);

        epicsMutexMustLock(NTPTimePvt.lock);
        d = epicsTimeDiffInSeconds(&timeNow, &NTPTimePvt.clockTime);
        if (d >= 0.0) {
            NTPTimePvt.clockTime = timeNow;
            NTPTimePvt.ticksToSkip = 0;
        } else { /* dont go back in time */
            NTPTimePvt.ticksToSkip = -d * NTPTimePvt.ticksPerSecond;
        }
        NTPTimePvt.clockTick = tickNow;
        NTPTimePvt.synchronized = 1;
        epicsMutexUnlock(NTPTimePvt.lock);

        NTPTimePvt.tickRate = (tickNow - NTPTimePvt.syncTick) /
            epicsTimeDiffInSeconds(&timeNow, &NTPTimePvt.syncTime);
        NTPTimePvt.syncTick = tickNow;
        NTPTimePvt.syncTime = timeNow;
    }
}

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

long NTPTime_Report(int level)
{
    if (onceId == EPICS_THREAD_ONCE_INIT) {
        printf("NTP driver not initialized\n");
    } else {
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
    }
    return 0;
}
