/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* iocClock.c */

/* Author:  Marty Kraimer Date:  16JUN2000 */

/* This file is originally named as iocClock.c and provide the NTP time
 * as default time source if no other time source registered during initialization.
 * Now we made some minor change to add this to the time provider list */

/* Sheng Peng @ SNS ORNL 07/2004 */
/* Version 1.1 */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <errno.h>
#include <rtems.h>

#include "epicsTypes.h"
#include "cantProceed.h"
#include "errlog.h"
#include "epicsThread.h"
#include "epicsMutex.h"
#include "epicsTime.h"
#include "envDefs.h"

#define BILLION 1000000000
#define NTPTimeSyncInterval 10.0

#define NTPTIME_DRV_VERSION "NTPTime Driver Version 1.2"

#include "epicsGeneralTime.h"

static void NTPTimeSyncNTP(void);
static int  NTPTimeGetCurrent(epicsTimeStamp *pDest);

long    NTPTime_Report(int level);

typedef struct NTPTimePvt {
    int		synced;	/* if never synced, we can't use it */
    epicsMutexId	lock;
    epicsTimeStamp	clock;
    unsigned long	lastTick;
    epicsUInt32		nanosecondsPerTick;
    int			tickRate;
    int			ticksToSkip;
}NTPTimePvt;
static NTPTimePvt *pNTPTimePvt = 0;
static int nConsecutiveBad = 0;

extern int rtems_bsdnet_get_ntp(int, int(*)(), struct timespec *);

static int
tickGet(void)
{
    rtems_interval t;
    rtems_clock_get (RTEMS_CLOCK_GET_TICKS_SINCE_BOOT, &t);
    return t;
}
static int
sysClkRateGet(void)
{
    rtems_interval t;
    rtems_clock_get (RTEMS_CLOCK_GET_TICKS_PER_SECOND, &t);
    return t;
}

static void NTPTimeSyncNTP(void)
{
    struct timespec Currtime;
    epicsTimeStamp epicsTime;
    int status;
    int prevStatusBad = 0;

    while(1) {
        double diffTime;
        epicsThreadSleep(NTPTimeSyncInterval);
        pNTPTimePvt->tickRate = sysClkRateGet();
        status = rtems_bsdnet_get_ntp(-1, NULL, &Currtime);
        if(status) {
            ++nConsecutiveBad;
            /*wait 1 minute before reporting failure*/
            if(nConsecutiveBad<(60/NTPTimeSyncInterval)) continue;
            if(!prevStatusBad)
                errlogPrintf("NTPTimeSyncWithNTPserver: sntpcTimeGet %s\n",
                    strerror(errno));
            prevStatusBad = 1;
            pNTPTimePvt->synced = FALSE;
            continue;
        }
        nConsecutiveBad=0;
        if(prevStatusBad) {
            errlogPrintf("NTPTimeSyncWithNTPserver: sntpcTimeGet OK\n");
            prevStatusBad = 0;
        }
        epicsTimeFromTimespec(&epicsTime,&Currtime);
        epicsMutexMustLock(pNTPTimePvt->lock);
        diffTime = epicsTimeDiffInSeconds(&epicsTime,&pNTPTimePvt->clock);
        if(diffTime>=0.0) {
            pNTPTimePvt->clock = epicsTime;
            pNTPTimePvt->ticksToSkip = 0;/* fix bug here */
        } else {/*dont go back in time*/
            pNTPTimePvt->ticksToSkip = (int) ((-1)*diffTime*pNTPTimePvt->tickRate);/* fix bug here */
        }
        pNTPTimePvt->lastTick = tickGet();
        pNTPTimePvt->synced = TRUE;
        epicsMutexUnlock(pNTPTimePvt->lock);
    }
}

static long NTPTime_InitOnce(int priority)
{
    int	status;
    struct timespec	Currtime;
    epicsTimeStamp	epicsTime;

    pNTPTimePvt = callocMustSucceed(1,sizeof(NTPTimePvt),"NTPTime_Init");

    memset(pNTPTimePvt, 0, sizeof(NTPTimePvt));
    pNTPTimePvt->synced = FALSE;
    pNTPTimePvt->lock = epicsMutexCreate();
    pNTPTimePvt->nanosecondsPerTick = BILLION/sysClkRateGet();
    pNTPTimePvt->tickRate = sysClkRateGet();
    /* look first for environment variable or CONFIG_SITE_ENV default */
    /* if TIMEZONE not defined, set it from EPICS_TIMEZONE */
    if (getenv("TIMEZONE") == NULL) {
        const char *timezone = envGetConfigParamPtr(&EPICS_TIMEZONE);
        if(timezone == NULL) {
            printf("NTPTime_Init: No Time Zone Information\n");
        } else {
            epicsEnvSet("TIMEZONE",timezone);
        }
    }

    /* try to sync with NTP server once here */
    pNTPTimePvt->tickRate = sysClkRateGet();
    status = rtems_bsdnet_get_ntp(-1, NULL, &Currtime);
    if(status)
    {/* sync failed */
    printf("First try to sync with NTP server failed!\n");
    }
    else
    {/* sync OK */
    epicsTimeFromTimespec(&epicsTime,&Currtime);
    epicsMutexMustLock(pNTPTimePvt->lock);
    pNTPTimePvt->clock = epicsTime;
    pNTPTimePvt->lastTick = tickGet();
    pNTPTimePvt->synced = TRUE;
    epicsMutexUnlock(pNTPTimePvt->lock);
    printf("First try to sync with NTP server succeed!\n");
    }

    epicsThreadCreate("NTPTimeSyncNTP",
        epicsThreadPriorityHigh,
        epicsThreadGetStackSize(epicsThreadStackSmall),
        (EPICSTHREADFUNC)NTPTimeSyncNTP,0);
    
    /* register to link list */
    generalTimeCurrentTpRegister("NTP", priority, NTPTimeGetCurrent);
    
    return	0;
}

struct InitInfo {
    int  priority;
    long retval;
};
static void NTPTime_InitOnceWrapper(void *arg)
{
    struct InitInfo *pargs = (struct InitInfo *)arg;
    pargs->retval = NTPTime_InitOnce(pargs->priority);
}

long NTPTime_Init(int priority)
{
    struct InitInfo args;
    static epicsThreadOnceId onceId = EPICS_THREAD_ONCE_INIT;

    args.priority = priority;
    epicsThreadOnce(&onceId, NTPTime_InitOnceWrapper, &args);
    return args.retval;
}

static int NTPTimeGetCurrent(epicsTimeStamp *pDest)
{
    unsigned long currentTick,nticks,nsecs;

    if(!pNTPTimePvt->synced) return	epicsTimeERROR;

    epicsMutexMustLock(pNTPTimePvt->lock);
    currentTick = tickGet();
    while(currentTick!=pNTPTimePvt->lastTick) {
        nticks = (currentTick>pNTPTimePvt->lastTick)
            ? (currentTick - pNTPTimePvt->lastTick)
            : (currentTick + (ULONG_MAX - pNTPTimePvt->lastTick));
        if(pNTPTimePvt->ticksToSkip>0) {/*dont go back in time*/
            if(nticks<pNTPTimePvt->ticksToSkip) {
                /*pNTPTimePvt->ticksToSkip -= nticks;*/ /* fix bug here */
                break;
            }
            nticks -= pNTPTimePvt->ticksToSkip;
        pNTPTimePvt->ticksToSkip = 0;    /* fix bug here */
        }
        pNTPTimePvt->lastTick = currentTick;
        pNTPTimePvt->tickRate = sysClkRateGet();
        nsecs = nticks/pNTPTimePvt->tickRate;
        nticks = nticks - nsecs*pNTPTimePvt->tickRate;
        pNTPTimePvt->clock.nsec += nticks * pNTPTimePvt->nanosecondsPerTick;
        if(pNTPTimePvt->clock.nsec>=BILLION) {
            ++nsecs;
            pNTPTimePvt->clock.nsec -= BILLION;
        }
        pNTPTimePvt->clock.secPastEpoch += nsecs;
    }
    *pDest = pNTPTimePvt->clock;
    epicsMutexUnlock(pNTPTimePvt->lock);
    return(0);
}

long    NTPTime_Report(int level)
{
    printf(NTPTIME_DRV_VERSION"\n");

    if(!pNTPTimePvt)
    {/* drvNTPTime is not used, we just report version then quit */
        printf("NTP time driver is not initialized yet!\n\n");
    }
    else
    {
        printf("\n");
    }
    return  0;
}
